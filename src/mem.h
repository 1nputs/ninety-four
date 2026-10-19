/* Work RAM by its 68000 address: the machine's view of struct g (src/ram.h: the game's state as
 * native structs, at the 68000's offsets, in the host's byte order). A machine byte is read and
 * written where ram_host_ says the host keeps it, so the 68000's big-endian view holds over fields
 * of any width (the playbook's NATIVE-STATE.md).
 *
 * The game reads its state by name (g.x, tm->score, the replay frames, the sound driver's slots);
 * what still goes by machine address, on purpose: addresses the game keeps or passes as data (m8
 * and its kin follow them, cartridge or RAM), a region's second use in another layout than its own
 * (the replay area as the save RAM's copy and the highlight's keep, the sort objects as the
 * credits' line scroll), the cartridge's tables of offsets into a team struct (the stats screens),
 * clears of a span of fields, the 68000's reads and writes past an array's end, and the edges
 * (the Z80's command buffer, the checks' images). A byte buffer, RAM that holds bytes as the
 * machine has them (Strings, the sprite table, the DMA list, records), is read through a host
 * pointer: ram_bytes. A place (rp) is opaque, so the compiler finds every such access, and there
 * is no other host pointer into RAM's bytes. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ram.h"
#include "rom.h"

typedef struct { uint16_t o; } rp;      /* a place in work RAM: its offset from $FF0000 */

/* the place at a 68000 address (the low 16 bits: RAM is 64 KB, $FF0000-$FFFFFF), of a RAM symbol,
 * n bytes on from another, and its 68000 address (what the game stores: move.l a2,(x)) */
static inline rp ram_at(uint32_t addr) { return (rp){ (uint16_t)addr }; }
#define RAM(name) ram_at(RAM_BASE + RAM_OFF_##name)
static inline rp rp_add(rp p, int32_t n) { return (rp){ (uint16_t)(p.o + n) }; }
static inline uint32_t rp_addr(rp p) { return RAM_BASE + p.o; }
static inline int rp_same(rp a, rp b) { return a.o == b.o; }
static inline int32_t rp_diff(rp a, rp b) { return (int32_t)a.o - b.o; }   /* (a - b in bytes, as the 68000 subtracts addresses) */

/* (the storage: the host byte of the machine byte off bytes from p) */
static inline uint8_t *ram_byte_(rp p, int32_t off) { return (uint8_t *)&g + ram_host_[(uint16_t)(p.o + off)]; }

/* bytes, words and longs at off bytes from p, big-endian as the 68000 reads and writes them */
static inline uint8_t r8(rp p, int32_t off) { return *ram_byte_(p, off); }
static inline uint16_t r16(rp p, int32_t off) { return (uint16_t)(r8(p, off) << 8 | r8(p, off + 1)); }
static inline uint32_t r32(rp p, int32_t off) { return (uint32_t)r16(p, off) << 16 | r16(p, off + 2); }
static inline void w8(rp p, int32_t off, uint8_t v) { *ram_byte_(p, off) = v; }
static inline void w16(rp p, int32_t off, uint16_t v) { w8(p, off, (uint8_t)(v >> 8)); w8(p, off + 1, (uint8_t)v); }
static inline void w32(rp p, int32_t off, uint32_t v) { w16(p, off, (uint16_t)(v >> 16)); w16(p, off + 2, (uint16_t)v); }

/* the 68000's (a0)+ on a byte: read or write at *p, then p one on; -(a0): p one back, then read or
 * write; and (*a0)++: the byte at p, kept one more */
static inline uint8_t r8_next(rp *p) { uint8_t v = r8(*p, 0); *p = rp_add(*p, 1); return v; }
static inline void w8_next(rp *p, uint8_t v) { w8(*p, 0, v); *p = rp_add(*p, 1); }
static inline uint8_t r8_prev(rp *p) { *p = rp_add(*p, -1); return r8(*p, 0); }      /* -(a0) */
static inline void w8_prev(rp *p, uint8_t v) { *p = rp_add(*p, -1); w8(*p, 0, v); }
static inline uint8_t r8_inc(rp p, int32_t off) { uint8_t v = (uint8_t)(r8(p, off) + 1); w8(p, off, v); return v; }   /* ++p[off] */
static inline uint8_t r8_postinc(rp p) { uint8_t v = r8(p, 0); w8(p, 0, (uint8_t)(v + 1)); return v; }
static inline uint8_t r8_postdec(rp p) { uint8_t v = r8(p, 0); w8(p, 0, (uint8_t)(v - 1)); return v; }

/* n bytes from src to dst (they may overlap, as memmove; byte by byte, as the two may hold
 * scalars differently), or n bytes of v at dst */
static inline void ram_copy(rp dst, rp src, size_t n)
{
    if ((uint16_t)(dst.o - src.o) < n)                    /* (dst inside src's bytes: from the end) */
        for (size_t i = n; i-- > 0;) w8(dst, (int32_t)i, r8(src, (int32_t)i));
    else
        for (size_t i = 0; i < n; i++) w8(dst, (int32_t)i, r8(src, (int32_t)i));
}
static inline void ram_fill(rp dst, uint8_t v, size_t n) { for (size_t i = 0; i < n; i++) w8(dst, (int32_t)i, v); }

/* n bytes between RAM and the host's (the cartridge's data, a buffer of the game's own) */
static inline void ram_write(rp dst, const void *src, size_t n) { for (size_t i = 0; i < n; i++) w8(dst, (int32_t)i, ((const uint8_t *)src)[i]); }
static inline void ram_read(void *dst, rp src, size_t n) { for (size_t i = 0; i < n; i++) ((uint8_t *)dst)[i] = r8(src, (int32_t)i); }

/* a 68000 address that may be the cartridge's or RAM's (a pointer the game keeps, and follows) */
static inline int is_ram(uint32_t a) { return (a & 0xFFFFFF) >= 0xE00000; }
static inline uint8_t m8(uint32_t a) { return is_ram(a) ? r8(ram_at(a), 0) : rom_ptr(a & 0xFFFFFF)[0]; }
static inline uint16_t m16(uint32_t a) { return (uint16_t)(m8(a) << 8 | m8(a + 1)); }
static inline uint32_t m32(uint32_t a) { return (uint32_t)m16(a) << 16 | m16(a + 2); }

/* a byte buffer of RAM's at a place, n bytes of it, as a host pointer: where RAM holds bytes as
 * the machine has them (no word or long of the game's lies there: Strings, the sprite table, the
 * DMA list, records, the save RAM's copy), which it checks; and the 68000 address of a host
 * pointer into RAM (what the game passes or keeps of one) */
void ram_bytes_fail_(rp p, size_t n);
static inline uint8_t *ram_bytes(rp p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p.o + i >= RAM_SIZE || ram_host_[p.o + i] != p.o + i) ram_bytes_fail_(p, n);
    return (uint8_t *)&g + p.o;
}
static inline uint32_t ram_addr(const void *x) { return RAM_BASE + (uint32_t)((const uint8_t *)x - (const uint8_t *)&g); }

/* one of the game's structs at a place, and the place of one (or of one of its fields): for what
 * holds the 68000 address of one (the tests' registers, an address the game keeps) */
static inline void *ram_struct_(rp p) { return (uint8_t *)&g + p.o; }   /* (a struct starts where the machine's does) */
#define RAM_AT(type, p) ((type *)ram_struct_(p))
static inline rp ram_place(const void *x) { return (rp){ (uint16_t)((const uint8_t *)x - (const uint8_t *)&g) }; }

/* a sort object (struct SortObj) at a place (the 68000 address a team struct keeps of its first:
 * sortobj_at), and an object's place */
static inline struct SortObj *obj_at(rp p) { return (struct SortObj *)((uint8_t *)&g + p.o); }   /* (a struct starts where the machine's does) */
static inline rp obj_place(const struct SortObj *o) { return (rp){ (uint16_t)((const uint8_t *)o - (const uint8_t *)&g) }; }

/* a team's struct at a place, and its place (for what reads a team struct by offset: the stats
 * tables, and the code the original runs on whatever it is handed) */
static inline struct Team *team_at(rp p) { return (struct Team *)((uint8_t *)&g + p.o); }
static inline rp team_place(const struct Team *t) { return (rp){ (uint16_t)((const uint8_t *)t - (const uint8_t *)&g) }; }
