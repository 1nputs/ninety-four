/* sram94.asm: the battery save RAM, one byte in the odd half of each word at $200000. */
#include "game.h"
#include "hal.h"

/* sram94.asm:ReadSRAM. d0 = first save RAM byte, d1 = count, a0 = destination. */
void ReadSRAM(uint32_t at, uint32_t count, uint32_t dst)
{
    uint16_t off = (uint16_t)(at << 1);
    uint8_t *p = mem_ptr(dst);
    uint16_t n = (uint16_t)(count - 1);
    do { *p++ = sram_read8((uint32_t)(int16_t)off + 1); off += 2; } while (n-- != 0);
}

/* sram94.asm:WriteSRAM. d0 = first save RAM byte, d1 = count, a0 = source (each byte written as
 * the low byte of a word). */
void WriteSRAM(uint32_t at, uint32_t count, uint32_t src)
{
    uint16_t off = (uint16_t)(at << 1);
    const uint8_t *p = mem_ptr(src);
    uint16_t n = (uint16_t)(count - 1);
    do { sram_write16((uint32_t)(int16_t)off, *p++); off += 2; } while (n-- != 0);
}
