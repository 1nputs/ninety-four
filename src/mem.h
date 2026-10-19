/* Work RAM by its 68000 address, as the original's code holds places in its registers: the way to
 * RAM's bytes besides its named fields (g.x, o->x). A place (rp) is opaque, so the compiler finds
 * every raw access, and there is no host pointer into RAM's bytes outside this header. Behind it is
 * today's struct g, laid out as the console's RAM; the native structs to come read and write the
 * same places through the generated map of addresses to fields (the playbook's NATIVE-STATE.md). */
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

/* (today's storage: g's bytes, big-endian as the 68000 has them) */
static inline uint8_t *ram_byte_(rp p, int32_t off) { return (uint8_t *)&g + (uint16_t)(p.o + off); }

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

/* n bytes from src to dst (they may overlap, as memmove), or n bytes of v at dst */
static inline void ram_copy(rp dst, rp src, size_t n)
{
    if (dst.o + n <= 0x10000u && src.o + n <= 0x10000u) { memmove(ram_byte_(dst, 0), ram_byte_(src, 0), n); return; }
    for (size_t i = 0; i < n; i++) w8(dst, (int32_t)i, r8(src, (int32_t)i));   /* (past the top: byte by byte, as move.b (a0)+,(a1)+) */
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

/* (step 1's bridges, while files move to places one by one: a host pointer into g as a place, and
 * back. They go when the last file has moved, which the compiler then proves.) */
static inline rp rp_of(const void *p) { return (rp){ (uint16_t)((const uint8_t *)p - (const uint8_t *)&g) }; }
static inline uint8_t *ptr_of(rp p) { return ram_byte_(p, 0); }
