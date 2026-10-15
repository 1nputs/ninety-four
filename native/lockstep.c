/* Lockstep against the oracle (nhl94 --lockstep FILE, tools/oracle/lockstep.py): the game reads the
 * oracle's pads and HV counter values, and at each of its vblank waits its work RAM (below the
 * stack) and VDP state must hash as the oracle's did at the matching vblank interrupt.
 *
 * The native game runs its vblank code only where it waits, the way the 68k does when its frame's
 * work fits in a frame. When it does not (a lag frame: the oracle's interrupt lands in the game's
 * logic), the oracle runs the vblank code an extra time mid-logic and the two part. The
 * recording marks those interrupts; the player follows the native's waits to the oracle's next
 * interrupt in a wait loop, and there takes the oracle's whole state (recorded at that point: work
 * RAM, the VDP, and `elapsed`, the d7 the game loop keeps) and goes on checking. A difference anywhere else stops the run, with work RAM in
 * out/lockstep-native.ram for diffing against the oracle's (lockstep.py --dump). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"

typedef struct record {
    uint8_t flags;              /* 1: in a wait loop, 2: the whole state follows */
    uint32_t frame, at;
    uint16_t buttons, nhv;
    const uint8_t *hv, *state;
    uint64_t ram, vdp;
} record;
static record *recs;
static size_t nrecs, next;              /* the oracle's next record */
static size_t span_lo, span_hi;         /* the records whose HV reads this span replays */
static size_t hv_rec, hv_used;
static int hv_over;
static long checked, lagged, resynced;

static uint64_t fnv64(uint64_t h, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

/* the oracle's HV values, in order, through the span's records: a byte read gets its byte in the
 * half it read */
static uint16_t hv_source(void *user)
{
    (void)user;
    while (hv_rec < span_hi && hv_used >= recs[hv_rec].nhv) { hv_rec++; hv_used = 0; }
    if (hv_rec >= span_hi) { hv_over = 1; return 0; }
    const uint8_t *e = recs[hv_rec].hv + 3 * hv_used++;
    uint16_t v = le16(e + 1);
    return e[0] == 0 ? (uint16_t)(v << 8) : v;
}

static uint16_t buttons;
static void pad_source(void *user, uint16_t b[2]) { (void)user; b[0] = buttons; b[1] = 0; }

/* the span up to the oracle's next interrupt in a wait loop */
static void next_span(void)
{
    span_lo = hv_rec = next;
    hv_used = 0;
    hv_over = 0;
    while (next < nrecs && !(recs[next].flags & 1)) next++;
    span_hi = next < nrecs ? next + 1 : nrecs;
}

int lockstep_load(const char *path, mdrt *m)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)len);
    if (!d || fread(d, 1, (size_t)len, f) != (size_t)len || len < 12 || memcmp(d, "MDLS", 4) || le32(d + 4) != 2) {
        fclose(f); fprintf(stderr, "%s: not a version 2 lockstep file\n", path); return 0;
    }
    fclose(f);
    size_t o = 12, cap = 1024;
    recs = malloc(cap * sizeof *recs);
    while (o + 14 <= (size_t)len && d[o] == 1) {
        if (nrecs == cap) recs = realloc(recs, (cap *= 2) * sizeof *recs);
        record *r = &recs[nrecs++];
        r->flags = d[o + 1]; r->frame = le32(d + o + 2); r->at = le32(d + o + 6);
        r->buttons = le16(d + o + 10); r->nhv = le16(d + o + 12);
        r->hv = d + o + 14;
        o += 14 + 3 * (size_t)r->nhv;
        r->ram = le64(d + o); r->vdp = le64(d + o + 8);
        o += 16;
        r->state = NULL;
        if (r->flags & 2) { r->state = d + o; o += 0x10000 + 24 + 0x10000 + 128 + 80 + 64; }
    }
    mdrt_set_hv_source(m, hv_source, NULL);
    mdrt_set_pad_source(m, pad_source, NULL);
    next_span();
    printf("%s: %zu vblanks\n", path, nrecs);
    return 1;
}

static uint64_t vdp_hash(void)
{
    mdrt_vdp_state s;
    mdrt_vdp_get_state(mdrt_get_vdp(native_m), &s);
    uint8_t c[128], v[80];
    for (int i = 0; i < 64; i++) { c[2 * i] = (uint8_t)(s.cram[i] >> 8); c[2 * i + 1] = (uint8_t)s.cram[i]; }
    for (int i = 0; i < 40; i++) { v[2 * i] = (uint8_t)(s.vsram[i] >> 8); v[2 * i + 1] = (uint8_t)s.vsram[i]; }
    return fnv64(fnv64(fnv64(fnv64(0xCBF29CE484222325ull, s.reg, 24), s.vram, 0x10000), c, 128), v, 80);
}

/* the oracle's whole state at its record: work RAM and the VDP */
static void take_state(const record *r)
{
    const uint8_t *p = r->state;
    memcpy(ram_ptr(RAM_BASE), p, 0x10000); p += 0x10000;
    mdrt_vdp_state s;
    memcpy(s.reg, p, 24); p += 24;
    memcpy(s.vram, p, 0x10000); p += 0x10000;
    for (int i = 0; i < 64; i++, p += 2) s.cram[i] = (uint16_t)(p[0] << 8 | p[1]);
    for (int i = 0; i < 40; i++, p += 2) s.vsram[i] = (uint16_t)(p[0] << 8 | p[1]);
    mdrt_vdp_set_state(mdrt_get_vdp(native_m), &s);
    elapsed = (int16_t)le32(p + 7 * 4);                   /* d7: the frames the game loop's pass covers */
}

/* at the game's vblank wait: the check against the oracle's interrupt the span ended at */
void lockstep_check(long frame)
{
    if (!recs) return;
    const record *r = span_lo < nrecs ? &recs[span_hi - 1] : NULL;
    if (!r || !(r->flags & 1)) {                          /* the recording's end (perhaps in lag frames) */
        printf("lockstep: %zu vblanks: %ld of the game's vblank waits checked, all the same as the oracle's; %ld lag "
               "frames (the oracle's vblank in the game's logic) after which the oracle's state was taken %ld times\n",
               nrecs, checked, lagged + (r ? (long)(span_hi - span_lo) : 0), resynced);
        native_quit();
    }
    int lag = span_hi - span_lo > 1;
    if (lag) {
        lagged += (long)(span_hi - span_lo - 1);
        take_state(r);
        resynced++;
    } else {
        uint64_t rh = fnv64(0xCBF29CE484222325ull, ram_ptr(RAM_BASE), 0xDF00), vh = vdp_hash();
        const char *bad = rh != r->ram ? "work RAM" : vh != r->vdp ? "VDP state" : hv_over || hv_used != r->nhv ? "HV reads" : NULL;
        if (bad) {
            printf("lockstep: %s differs at the game's vblank wait %ld (the oracle's frame %u, the 68k at %06x); "
                   "%zu HV reads of %u; %ld waits checked, %ld lag frames before\n",
                   bad, frame, r->frame, r->at, hv_used, r->nhv, checked, lagged);
            FILE *f = fopen("out/lockstep-native.ram", "wb");
            if (f) { fwrite(ram_ptr(RAM_BASE), 1, 0x10000, f); fclose(f); }
            native_quit();
        }
        checked++;
    }
    buttons = r->buttons;
    next = span_hi;
    next_span();
}
