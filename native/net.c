/* Netplay (pulseman's mdrt/netplay.h over mdrt/net_udp.h): two to four machines, a host and its
 * guests, from power on; the players at each (--here, the Online page's Players here) on the
 * pads in turn, the host's first, then each guest's as it came, up to the host's players in all
 * (--players): two machines with one each, the host pad 1 and the guest pad 2; four with one each;
 * two each; two and one and one ... EA's 4 Way Play is in the ports on every machine when pads 3
 * or 4 are played. A player who leaves ends the game for all. The game's resumable point is the
 * game loop's wait (setvideo's, where states are taken: state.c), and a rollback goes on from there
 * as a loaded state does (restart_at native_resume_game_loop). The game plays whatever session its
 * console has (mdrt_get_net): the window's, made on its Online page or from the command line, or a
 * headless run's own. The identity the peers compare: the build (its commit) and the ROM. What is
 * sent, the
 * host's for both (native_net_settings, made where the session is: the window's Start, or the
 * command line): the next game's rules, its league (a package: a folder's files packed), and its
 * save RAM; no player's save RAM is written while the session lasts. Each player's buttons are
 * their own controls' (the window's players in turn; headless, their pads' scripts). Headless, a
 * run that reaches --frames waits there until every machine has every button up to it: the host
 * then a second more, and says goodbye; a guest until the host's goodbye.
 *
 * mdrt_net_frame is called at every wait before the handshake, as states are taken: a frame's
 * game code, then the session's turn, then the handshake reads the next frame's buttons. */
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "game.h"
#include "native.h"
#include "rules.h"
#include "mdrt/net_udp.h"
#include "build_id.h"

static mdrt_udp *udp;                   /* the command line's session (net) and its link */
static mdrt_net *net;
static mdrt *console;
static int headless, started;           /* (headless: this file connects, starts and ends the session) */
static native_input_fn local_fn;
static long stop_at = -1;

static void local_input(void *user, uint16_t b[MDRT_PADS])
{
    (void)user;
    uint8_t pads = mdrt_net_pads(net);
    int k = 0;
    for (int p = 0; p < MDRT_PADS; p++)
        if (pads >> p & 1) { b[k] = local_fn ? local_fn(p, k) : 0; k++; }
}

const char *native_net_identity(const char *setup)
{
    static char identity[256];
    uint64_t h = 0xCBF29CE484222325ull;                   /* (the command line's part, hashed: the string stays short) */
    for (const char *s = setup ? setup : ""; *s; s++) h = (h ^ (uint8_t)*s) * 0x100000001B3ull;
    snprintf(identity, sizeof identity, "ninety-four %s, NHL 94 (UE) %08x, setup %016llx", NHL94_BUILD, NATIVE_ROM_CRC32,
             (unsigned long long)h);
    return identity;
}

/* ---- the session's settings: the host's, the next game's for both ----------------------------------
 * "N94S", a version byte (1), then little-endian: the rules (a count byte, each rule's value), save
 * RAM (a u16 size, 0 for blank or 0x2000, the bytes), the league (a u32 size, 0 for the cartridge's,
 * a package's bytes) */
static uint8_t *settings;
static uint8_t session_rules[RULE_COUNT];               /* the settings the game plays by (the window's pages show them) */
static int session_league, session_host;
static atomic_int session_known;

const void *native_net_settings(const char *sram, size_t *size)
{
    static uint8_t s[0x2000];
    uint8_t v[RULE_COUNT], *pkg;
    long n = league_next_package(&pkg);
    if (n < 0) { fprintf(stderr, "netplay: the league: %s: the session plays the cartridge's teams\n", league_error()); n = 0; }
    FILE *f = sram ? fopen(sram, "rb") : NULL;            /* (none: blank, as a new cartridge's) */
    size_t sn = f && fread(s, 1, sizeof s, f) == sizeof s ? sizeof s : 0;
    if (f) fclose(f);
    rules_next_get(v);
    size_t total = 5 + 1 + RULE_COUNT + 2 + sn + 4 + (size_t)n;
    free(settings);
    if (!(settings = malloc(total))) { free(pkg); *size = 0; return NULL; }
    uint8_t *o = settings;
    memcpy(o, "N94S", 4); o[4] = 1; o += 5;
    *o++ = RULE_COUNT; memcpy(o, v, RULE_COUNT); o += RULE_COUNT;
    *o++ = (uint8_t)sn; *o++ = (uint8_t)(sn >> 8); memcpy(o, s, sn); o += sn;
    for (int k = 0; k < 4; k++) *o++ = (uint8_t)((size_t)n >> 8 * k);
    if (n) memcpy(o, pkg, (size_t)n);
    free(pkg);
    *size = total;
    return settings;
}

/* the settings b (size bytes) the next game's: NULL, or what is wrong */
static const char *settings_use(mdrt *m, const uint8_t *b, size_t size)
{
    if (!b || size < 6 || memcmp(b, "N94S", 4) || b[4] != 1) return "none from this build";
    size_t at = 5, k = b[at++];
    if (size < at + k + 2) return "cut short";
    const uint8_t *v = b + at;
    at += k;
    size_t sn = (size_t)(b[at] | b[at + 1] << 8);
    at += 2;
    if ((sn && sn != 0x2000) || size < at + sn + 4) return "its save RAM cut short";
    const uint8_t *s = b + at;
    at += sn;
    size_t n = (size_t)b[at] | (size_t)b[at + 1] << 8 | (size_t)b[at + 2] << 16 | (size_t)b[at + 3] << 24;
    at += 4;
    if (size != at + n) return "its league cut short";
    if (rules_next_set(v, (int)k) < 0) return "a rule out of range";
    if (league_next_bytes(b + at, n) < 0) return league_error();
    mdrt_sram *r = mdrt_get_sram(m);
    memset(r->bytes, 0, r->size);
    if (sn) memcpy(r->bytes, s, sn < r->size ? sn : r->size);
    rules_next_get(session_rules);
    session_league = n != 0;
    session_host = mdrt_net_is_host(mdrt_get_net(m));
    atomic_store(&session_known, 1);
    return NULL;
}

int native_net_session(uint8_t *v, int *sent, int *host)
{
    if (!atomic_load(&session_known)) return 0;
    memcpy(v, session_rules, RULE_COUNT);
    *sent = session_league;
    *host = session_host;
    return 1;
}

void native_net_power_on(mdrt *m)
{
    mdrt_net *n = mdrt_get_net(m);
    atomic_store(&session_known, 0);
    if (!n || mdrt_net_state(n) != MDRT_NET_GO) return;
    size_t size;
    const void *b = mdrt_net_settings(n, &size);
    const char *why = settings_use(m, b, b ? size : 0);
    if (!why) return;
    fprintf(stderr, "netplay: the host's settings: %s: the session ends\n", why);   /* (the game alone, as it would have been) */
    mdrt_net_quit(n);
}

int native_net_open(const mdrt_net_options *o, const char *setup, const void *set, size_t set_size, native_input_fn input,
                    char *err, size_t errsize)
{
    udp = mdrt_net_open_udp(o, err, errsize);
    if (!udp) return 0;
    mdrt_net_config c = { .role = o->role, .identity = native_net_identity(setup), .name = o->name, .delay = o->delay,
                          .window = 8, .rollback = 30, .check_every = 60, .settings = set, .settings_size = set ? set_size : 0 };
    c.players = o->here;
    c.players_total = o->players;
    c.link = mdrt_udp_link(udp);
    c.local_input = local_input;
    local_fn = input;
    net = mdrt_net_new(&c);
    if (!net) {
        snprintf(err, errsize, c.settings_size > MDRT_NET_MAX_SETTINGS ? "the game's settings are too big to send (its league)" : "a bad configuration");
        mdrt_udp_close(udp);
        udp = NULL;
        return 0;
    }
    return 1;
}

/* the session the game plays in now, while it lasts */
static mdrt_net *playing(void)
{
    mdrt_net *n = console ? mdrt_get_net(console) : NULL;
    return n && mdrt_net_state(n) == MDRT_NET_GO ? n : NULL;
}

int native_net_on(void) { return playing() != NULL || (headless && !started); }

/* pads, for a person: "pad 2", "pads 1 and 2", "pads 2, 3 and 4" */
static void pads_text(uint8_t pads, char *buf, size_t size)
{
    int n = 0, k = 0;
    for (int p = 0; p < MDRT_PADS; p++) n += pads >> p & 1;
    size_t at = (size_t)snprintf(buf, size, n == 1 ? "pad" : "pads");
    for (int p = 0; p < MDRT_PADS && at < size; p++)
        if (pads >> p & 1) at += (size_t)snprintf(buf + at, size - at, "%s%d", !k++ ? " " : k == n ? " and " : ", ", p + 1);
}

/* headless: connect, then the game powers on (native_net_start) */
int native_net_connect(void)
{
    char peer[96];
    fprintf(stderr, "netplay: on port %u, connecting\n", (unsigned)mdrt_udp_port(udp));
    for (;;) {
        int r = mdrt_net_connect(net);
        if (r == MDRT_NET_READY) break;
        if (r != MDRT_NET_CONNECTING) { fprintf(stderr, "netplay: %s\n", mdrt_net_why(net)); return 0; }
        mdrt_udp_wait(udp, 10);
    }
    mdrt_udp_lock(udp);
    headless = 1;
    char mine[32];
    pads_text(mdrt_net_pads(net), mine, sizeof mine);
    fprintf(stderr, "netplay: playing with %s, %s, %s, input delay %d\n", mdrt_udp_peer(udp, peer, sizeof peer),
            mdrt_net_is_host(net) ? "hosting" : "joined", mine, mdrt_net_delay(net));
    for (int k = 0; k < mdrt_net_peers(net); k++) {       /* (who plays which) */
        char pads[32];
        const char *name = mdrt_net_name_of(net, k);
        pads_text(mdrt_net_pads_of(net, k), pads, sizeof pads);
        fprintf(stderr, "netplay: %s%s, %s\n", name[0] ? name : k ? "a guest" : "the host", k == mdrt_net_me(net) ? " (here)" : "", pads);
    }
    return 1;
}

int native_net_four_way(void) { return net && (mdrt_net_all_pads(net) & 0xC); }

mdrt_net *native_net(void) { return net; }
mdrt_udp *native_net_udp(void) { return udp; }

/* a console powered on (its states' parts added): in the window, its session (if any) is the
 * window's, started already; headless, this run's own, from this power on */
void native_net_start(mdrt *m)
{
    console = m;
    if (!headless || started || mdrt_get_net(m)) return;
    mdrt_set_net(m, net);
    mdrt_net_start(net, m);
    started = 1;
}

void native_net_stop_at(long frame) { stop_at = frame; }

static void report(mdrt_net *n)
{
    mdrt_net_stats s;
    mdrt_net_stats_get(n, &s);
    fprintf(stderr, "netplay: frame %u, %ld rollbacks (%ld frames played again, at most %d), %ld frames given back, %ld desync checks, round trip %d ms, %ld resets\n",
            (unsigned)s.frame, s.rollbacks, s.replayed, s.most_replayed, s.gave_back, s.checks, s.rtt_ms, s.resets);
}

static void over(mdrt_net *n)
{
    if (headless) fprintf(stderr, "netplay: %s\n", mdrt_net_why(n));   /* (the window says why itself) */
    report(n);
}

/* the end of a headless run: every button up to the last frame known on every machine; the host
 * then a second more for the others and says goodbye, a guest waits for that (15 seconds at most) */
static void finish(mdrt_net *n, int resumable)
{
    uint64_t linger = 0;
    int host = mdrt_net_is_host(n);
    for (;;) {
        int r = mdrt_net_frame(n, resumable);
        if (r == MDRT_NET_RESTART) restart_at(native_resume_game_loop);
        if (r == MDRT_NET_RESET) restart_at(native_power_on_again);
        mdrt_net_stats s;
        mdrt_net_stats_get(n, &s);
        if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) {   /* (the other done first, or gone) */
            if (s.confirmed < (uint32_t)stop_at) over(n);
            else report(n);
            native_quit();
        }
        mdrt_net_link l = mdrt_udp_link(udp);
        if (!linger && s.confirmed >= (uint32_t)stop_at && s.remote_has >= (uint32_t)stop_at && s.remote_frame >= (uint32_t)stop_at)
            linger = l.ticks(l.user) + (host ? 1000000u : 15000000u);
        if (linger && l.ticks(l.user) >= linger) break;
        mdrt_udp_wait(udp, 1);
    }
    report(n);
    mdrt_net_quit(n);
    native_quit();
}

/* at every wait, before the handshake (a rollback: the game loop from its wait, as a state loaded) */
void native_net_wait(void)
{
    mdrt_net *n = playing();
    if (!n) return;                                       /* (none, or over: the console plays on alone) */
    int resumable = native_at_game_loop_wait(), r;
    while ((r = mdrt_net_frame(n, resumable)) == MDRT_NET_WAIT) {
        if (mdrt_idle(console)) {                         /* (the window closing: goodbye) */
            mdrt_net_quit(n);
            native_quit();
        }
        if (headless) mdrt_udp_wait(udp, 1);              /* (the window waits in mdrt_idle) */
    }
    if (r == MDRT_NET_RESTART) restart_at(native_resume_game_loop);
    if (r == MDRT_NET_RESET) restart_at(native_power_on_again);   /* (the host reset the console: powered on again, the game from its start) */
    if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) { over(n); return; }
    if (headless && stop_at >= 0 && (long)mdrt_frame_count(console) >= stop_at && !mdrt_quiet(console)) finish(n, resumable);
}

void native_net_close(void)
{
    if (net && mdrt_net_state(net) == MDRT_NET_GO) { report(net); fprintf(stderr, "netplay: goodbye\n"); }
    if (net) mdrt_net_quit(net);
    mdrt_net_free(net);
    mdrt_udp_close(udp);
    net = NULL;
    udp = NULL;
}
