/* sound94.asm:Z80_Program_Code, the Z80's 661-byte sound relay, ported to C. The 68k keeps writing
 * Z80 RAM exactly as on the hardware (the 33-byte command buffer at $02, the PCM pointers at
 * $23-$28, and the bytes it patches into the program: the jump target at $96, the PCM rate at $8E,
 * the PCM volume page at $83); this file does what the Z80 program does with them:
 *
 *   $0029 init: DAC on (YM $2B = $80), LFO ($22 = $0C), PCM from the start pointer at $23
 *   $007D the PCM loop: one sample byte from ROM through the bank window, through the volume
 *         table (page $83, built by p_initialZ80) to the DAC ($2A); a zero ends the sample and the
 *         end pointer at $26 takes over; the rate at $8E steps a fraction accumulator
 *   $0095 JP NC to the word at $96: $7D keeps streaming, $D1 runs a command, $29 restarts
 *   $00D1 a command: key off, patches, frequencies, carrier volumes, key on (in the order the
 *         bits of $02-$06 say), then $96 back to $7D, which is what the 68k waits for
 *
 * Writes to the YM2612 go through z80_ym_write (the runtime's FM chip natively, a recorder in the
 * tests). Timing is the program's, instruction by instruction, so the writes come out when the
 * Z80's would: the cycle counts are the Z80's (Zilog UM0080), plus 3 cycles a read through the
 * bank window (the 68k bus access), plus the YM2612's busy flag, which the program polls before
 * each write: set after a data write until 32 of the chip's cycles (42 master clocks each, on a
 * grid from the frame's start) have passed. Time goes on while the 68k holds the bus, so the flag
 * can run out meanwhile. As in the oracle (Genesis Plus GX, a discrete YM2612), only part I's
 * address ($4000) reads the status: the polls while IX points at part II never wait. The Z80
 * runs as the oracle runs it: instructions while it is behind the client's time, so it finishes
 * the one under way when the 68k takes the bus, and starts again at the time it gets it back.
 * Checked against the oracle's sound traces (test/relaytest.c).
 *
 * The PCM loop runs an instruction at a time (z80.pc is the address in the listing). The init and
 * a command run as a routine: their steps (instructions, busy polls, writes, RAM stores) are
 * worked out when they start (the 68k leaves the command buffer alone until $96 is back to $7D),
 * then run in turn. A Z80 reset drops the rest of a routine. */
#include <string.h>

#include "rom.h"
#include "z80.h"

uint8_t z80_ram[0x2000];
struct Z80Relay z80;

#define CYCLE 15                                          /* master clocks a Z80 cycle */
#define BANK_READ 3                                       /* the wait of a read through the window */
#define YM_GRID 42                                        /* master clocks a YM2612 cycle */
#define YM_BUSY 32                                        /* its cycles of the busy flag */

/* the byte at Z80 address a: RAM below $2000, the ROM through the bank window from $8000 */
static uint8_t z80_read(uint16_t a)
{
    if (a < 0x2000) return z80_ram[a];
    if (a >= 0x8000) return rom_ptr(0)[((uint32_t)z80.bank << 15 | (a & 0x7FFFu)) & 0xFFFFFu];
    return 0xFF;
}

/* a YM2612 data write at the end of the instruction: it sets the busy flag */
static void ym_write(int part, uint8_t reg, uint8_t val)
{
    uint64_t t = z80.zc - z80.frame;
    z80.ym_free = z80.frame + ((t + YM_GRID - 1) / YM_GRID + YM_BUSY) * YM_GRID;
    z80_ym_write(part, reg, val);
}

/* ---- a routine's steps ---------------------------------------------------------------------- */

enum { OP_CYCLES, OP_POLL, OP_YM, OP_RAM };

static void step(uint8_t kind, uint8_t part, uint8_t reg, uint8_t val, unsigned cycles)
{
    if (z80.nops < Z80_OPS) z80.ops[z80.nops++] = (struct Z80Op){ kind, part, reg, val, (uint16_t)cycles };
}
static int ix_part;                                       /* IX at $4002: part II */
static void op(unsigned cycles) { step(OP_CYCLES, 0, 0, 0, cycles); }
static void op_list(const uint8_t *list, size_t n) { for (size_t i = 0; i < n; i++) op(list[i]); }
/* BIT 7,(IX) / JR NZ back until the chip is free */
static void poll(void) { step(OP_POLL, (uint8_t)ix_part, 0, 0, 0); }
/* the address write after the wait: LD (IX),r or LD (IX),n */
static void ym_address(void) { poll(); op(19); }
/* the data write after the wait: LD (IX+1),r or LD (IX+1),n */
static void ym_data(uint8_t reg, uint8_t val) { poll(); step(OP_YM, (uint8_t)ix_part, reg, val, 19); }
/* a store to Z80 RAM at the end of the instruction before */
static void ram_store(uint8_t a, uint8_t v) { step(OP_RAM, 0, a, v, 0); }

/* $00AA with its RET: the bank register from A, a bit a write to $6000 */
static const uint8_t bank_ops[] = { 4, 4, 13, 4, 13, 4, 13, 4, 13, 4, 13, 4, 13, 4, 13, 4, 13, 4, 13, 4, 10 };

/* $025B with its CALL: HL = the patch of channel e (32 bytes from fm_instrument_patches, bank 5) */
static uint16_t patch_address(int e)
{
    op(17); op(10); op(4); op(4); op(4); op(7); op(4);
    for (int i = 0; i < 10; i++) op(8);                   /* SLA L, RL H x 5 */
    op(10); op(11); op(7); op(10); op_list(bank_ops, sizeof bank_ops);
    z80.bank = 5;
    return (uint16_t)(0xC248 + (z80_ram[0x1C + e] << 5));
}

/* $0280 with its CALL: the channel in its part (IX to part II for channels 4-6) */
static int channel(int e)
{
    op(17); op(4); op(7);
    if (e < 3) { op(11); return e; }
    op(5); op(10); op(10); op(7); op(10);
    ix_part = 1;
    return e - 4;
}

/* a bit of a command byte, as the loops at $00D8 and the like walk them: INC E, SRL A (the
 * caller adds the branch on it); returns the bits left */
static unsigned next_bit(unsigned bits) { op(4); op(8); return bits >> 1; }

/* $00D1: the command in $02-$06 */
static void command(void)
{
    unsigned bits;
    uint8_t bank = z80.bank;
    z80.nops = z80.op = 0; ix_part = 0;
    op(11); op(11);                                       /* PUSH HL, PUSH BC */
    /* key off ($02): YM $28 = channel */
    op(7); op(13);
    bits = z80_ram[0x02];
    for (int e = 0;; e++) {
        int bit = bits & 1; bits = next_bit(bits);
        if (bit) { op(7); ym_address(); ym_data(0x28, (uint8_t)e); op(4); }
        else op(12);
        if (!bits) { op(7); break; }
        op(12);
    }
    /* patches ($06): registers $30-$9C (7 x 4 operator registers), then $B0 and $B4 */
    op(7); op(13);
    bits = z80_ram[0x06];
    for (int e = 0;; e++) {
        int bit = bits & 1; bits = next_bit(bits);
        if (bit) {
            op(7); op(11);                                /* JR NC, PUSH AF */
            uint16_t p = patch_address(e);
            int ch = channel(e);
            op(7); op(4); op(7);                          /* ADD A,$30, LD D,A, LD B,30 */
            uint8_t r = (uint8_t)(0x30 + ch);
            for (int n = 30; n > 0; n--) {
                ym_address();
                uint8_t v = z80_read(p++);
                op(7 + BANK_READ); op(6);                 /* LD C,(HL), INC HL */
                ym_data(r, v);
                r = (uint8_t)(r + 4);
                op(4); op(4); op(4); op(4); op(4); op(7); /* INC D x 4, LD A,B, CP 3 */
                if (n == 3) { r = (uint8_t)(r + 0x10); op(7); op(4); op(7); op(4); }   /* after $9C: $B0, $B4 */
                else op(12);
                op(n > 1 ? 13 : 8);                       /* DJNZ */
            }
            op(14); op(10);                               /* LD IX,$4000, POP AF */
            ix_part = 0;
        } else {
            op(12);
        }
        if (!bits) { op(7); break; }
        op(12);
    }
    /* frequencies ($05): $A4 (block, F-number high) then $A0 */
    op(7); op(13);
    bits = z80_ram[0x05];
    for (int e = 0;; e++) {
        int bit = bits & 1; bits = next_bit(bits);
        if (bit) {
            op(7); op(11); op(10); op(4); op(8); op(4); op(4);   /* JR NC, PUSH AF, HL = $0E + 2 e */
            int ch = channel(e);
            op(7);                                        /* ADD A,$A4 */
            ym_address();
            op(7);                                        /* LD D,(HL) */
            ym_data((uint8_t)(0xA4 + ch), z80_ram[0x0E + 2 * e]);
            op(7);                                        /* SUB 4 */
            ym_address();
            op(6); op(7);                                 /* INC HL, LD D,(HL) */
            ym_data((uint8_t)(0xA0 + ch), z80_ram[0x0F + 2 * e]);
            op(14); op(10);                               /* LD IX,$4000, POP AF */
            ix_part = 0;
        } else {
            op(12);
        }
        if (!bits) { op(7); break; }
        op(12);
    }
    /* volumes ($04): the attenuation to the total level of the algorithm's carriers, from operator
     * 4 down (1 carrier for algorithms 0-3, 2 for 4, 3 for 5 and 6, 4 for 7) */
    op(7); op(13);
    bits = z80_ram[0x04];
    for (int e = 0;; e++) {
        int bit = bits & 1; bits = next_bit(bits);
        op(10);                                           /* JP NC */
        if (bit) {
            op(11);                                       /* PUSH AF */
            uint16_t p = patch_address(e);
            op(4); op(7); op(4); op(4); op(7); op(4);     /* HL + $1C */
            op(7 + BANK_READ); op(7); op(7); op(7);       /* the algorithm, AND 7, LD C,1, CP 4 */
            uint8_t alg = z80_read((uint16_t)(p + 0x1C)) & 7;
            int carriers = alg < 4 ? 1 : alg < 5 ? 2 : alg < 7 ? 3 : 4;
            if (alg < 4) op(12);
            else if (alg < 5) { op(7); op(4); op(7); op(12); }
            else if (alg < 7) { op(7); op(4); op(7); op(7); op(4); op(7); op(12); }
            else { op(7); op(4); op(7); op(7); op(4); op(7); op(7); op(4); }
            op(10); op(4); op(4); op(4);                  /* HL = 7 + e */
            int ch = channel(e);
            op(7);                                        /* ADD A,$4C */
            uint8_t att = z80_ram[0x07 + e];
            for (int n = 0; n < carriers; n++) {
                if (n) op(7);                             /* SUB 4 */
                ym_address();
                if (!n) op(7);                            /* LD D,(HL) */
                ym_data((uint8_t)(0x4C - 4 * n + ch), att);
                if (n < 3) { op(4); op(n == carriers - 1 ? 12 : 7); }   /* DEC C, JR Z */
            }
            op(14); op(10);                               /* LD IX,$4000, POP AF */
            ix_part = 0;
        }
        op(10);                                           /* JP NZ */
        if (!bits) break;
    }
    /* key on ($03): bit 6 (channel 6 as FM) turns the DAC off first; YM $28 = $F0 + channel */
    op(7); op(13); op(8);
    bits = z80_ram[0x03];
    if (bits & 0x40) { op(7); ym_address(); ym_data(0x2B, 0x00); }
    else op(12);
    for (int e = 0;; e++) {
        int bit = bits & 1; bits = next_bit(bits);
        if (bit) { op(7); ym_address(); ym_data(0x28, (uint8_t)(0xF0 + e)); op(4); }
        else op(12);
        if (!bits) { op(7); break; }
        op(12);
    }
    /* $0249: back to the sample's bank, and $96 back to $7D: the 68k may send the next command */
    op(13); op(4); op(13);
    ram_store(0x9D, (uint8_t)(z80_ram[0x9D] - 1));
    op(10); op(16);
    ram_store(0x96, 0x7D);
    ram_store(0x97, 0x00);
    op(10); op(10); op(10);                               /* POP BC, POP HL, JP $0098 */
    z80.bank = bank;              /* no read through the window until $0098 sets it from $9D */
    z80.ret = 0x0098;
}

/* $0029: start (from the reset, or the restart when the 68k patches $96 to $29) */
static void init(void)
{
    z80.nops = z80.op = 0; ix_part = 0;
    op(4); op(10); op(8); op(10); op(16);                 /* DI, LD SP, IM 0, LD HL,$7D, LD ($96),HL */
    ram_store(0x96, 0x7D);
    ram_store(0x97, 0x00);
    op(14);                                               /* LD IX,$4000 */
    ym_address(); ym_data(0x2B, 0x80);                    /* DAC on */
    ym_address(); ym_data(0x22, 0x0C);                    /* LFO */
    z80.ret = 0x0061;
}

/* the routine's next step (an instruction, or a store at the end of the one before) */
static void routine_step(void)
{
    const struct Z80Op *o = &z80.ops[z80.op];
    switch (o->kind) {
    case OP_POLL:
        if (!z80.poll) {                                  /* BIT 7,(IX): the status read at its end */
            z80.zc += 20 * CYCLE;
            z80.poll = (!o->part && z80.zc < z80.ym_free) ? 1 : 2;
            return;
        }
        z80.zc += (z80.poll == 1 ? 12 : 7) * CYCLE;       /* JR NZ: back while busy */
        if (z80.poll == 1) { z80.poll = 0; return; }
        z80.poll = 0;
        break;
    case OP_YM:
        z80.zc += o->cycles * CYCLE;
        ym_write(o->part, o->reg, o->val);
        break;
    case OP_RAM:
        z80_ram[o->reg] = o->val;
        break;
    default:
        z80.zc += o->cycles * CYCLE;
        break;
    }
    if (++z80.op == z80.nops) { z80.nops = z80.op = 0; z80.pc = z80.ret; }
}

/* ---- the PCM loop, an instruction at a time ------------------------------------------------- */

/* the instruction at pc: its cycles */
static unsigned instruction(void)
{
    unsigned sum;
    switch (z80.pc) {
    case 0x0000: z80.pc = 0x0029; return 12;                              /* JR $0029 */
    case 0x0029: init(); return 0;                                        /* (the routine has the time) */
    case 0x00D1: command(); return 0;
    case 0x0061: z80.iy = 0x23; z80.pc = 0x0065; return 14;               /* LD IY,$23: the start pointer */
    case 0x0065: z80.pc = 0x006B; return 12;                              /* JR $006B */
    case 0x0067: z80.iy = 0x26; z80.pc = 0x006B; return 14;               /* LD IY,$26: the end pointer */
    case 0x006B: z80.hl = (uint16_t)(z80_ram[z80.iy + 1] << 8 | (z80.hl & 0xFF)); z80.pc = 0x006E; return 19;   /* LD H,(IY+1) */
    case 0x006E: z80.hl = (uint16_t)((z80.hl & 0xFF00) | z80_ram[z80.iy + 2]); z80.pc = 0x0071; return 19;      /* LD L,(IY+2) */
    case 0x0071: z80.a = (uint8_t)(z80.hl >> 8); z80.pc = 0x0072; return 4;           /* LD A,H */
    case 0x0072: z80.carry = z80.a >> 7; z80.pc = 0x0073; return 4;                   /* RLA: the address's bit 15 */
    case 0x0073: z80.a = z80_ram[z80.iy]; z80.pc = 0x0076; return 19;                 /* LD A,(IY): the bank byte */
    case 0x0076: z80.a = (uint8_t)(z80.a << 1 | z80.carry); z80.pc = 0x0077; return 4;   /* RLA */
    case 0x0077: z80_ram[0x9D] = z80.a; z80.pc = 0x007A; return 13;                   /* LD ($9D),A */
    case 0x007A: z80.pc = 0x0098; return 10;                                          /* JP $0098 */

    case 0x007D: z80.pc = 0x0081; return 19;                                          /* LD (IX),$2A */
    case 0x0081: z80.e = z80_read(z80.hl); z80.pc = 0x0082; return 7 + BANK_READ;     /* LD E,(HL) */
    case 0x0082: z80.d = z80_ram[0x83]; z80.pc = 0x0084; return 7;                    /* LD D,page */
    case 0x0084: z80.a = z80_ram[(z80.d << 8 | z80.e) & 0x1FFF]; z80.pc = 0x0085; return 7;   /* LD A,(DE) */
    case 0x0085: z80.pc = 0x0086; return 4;                                           /* OR A */
    case 0x0086: z80.pc = z80.a ? 0x0089 : 0x0067; return 10;                         /* JP Z: the sample ended */
    case 0x0089: z80.zc += 13 * CYCLE; ym_write(0, 0x2A, z80.a); z80.pc = 0x008C; return 0;   /* LD ($4001),A */
    case 0x008C: z80.a = z80.c; z80.pc = 0x008D; return 4;                            /* LD A,C */
    case 0x008D:                                                                      /* ADD A,rate */
        sum = (unsigned)z80.a + z80_ram[0x8E];
        z80.a = (uint8_t)sum; z80.carry = sum > 0xFF; z80.pc = 0x008F; return 7;
    case 0x008F: z80.c = z80.a; z80.pc = 0x0090; return 4;                            /* LD C,A */
    case 0x0090: z80.pc = 0x0093; return 10;                                          /* LD DE,0 */
    case 0x0093:                                                                      /* ADC HL,DE */
        sum = (unsigned)z80.hl + z80.carry;
        z80.hl = (uint16_t)sum; z80.carry = sum > 0xFFFF; z80.pc = 0x0095; return 15;
    case 0x0095:                                                                      /* JP NC,($96) */
        if (z80.carry) { z80.pc = 0x0098; return 10; }
        z80.pc = (uint16_t)(z80_ram[0x96] | z80_ram[0x97] << 8);
        return 10;
    case 0x0098: z80.a = (uint8_t)(z80.hl >> 8); z80.pc = 0x0099; return 4;           /* LD A,H */
    case 0x0099: z80.a |= 0x80; z80.pc = 0x009B; return 7;                            /* OR $80 */
    case 0x009B: z80.hl = (uint16_t)(z80.a << 8 | (z80.hl & 0xFF)); z80.pc = 0x009C; return 4;   /* LD H,A */
    case 0x009C: z80.a = z80_ram[0x9D]; z80.pc = 0x009E; return 7;                    /* LD A,bank */
    case 0x009E: z80.n = 0; z80.pc = 0x00AA; return 17;                               /* CALL $00AA */
    case 0x00AA: z80.pc = 0x00AB; return 4;                                           /* SCF */
    case 0x00AB: z80.pc = 0x00AC; return 4;                                           /* CCF */
    case 0x00AC: z80.pc = 0x00AF; return 13;                                          /* LD ($6000),A: a bit of the bank */
    case 0x00AF: z80.pc = ++z80.n < 9 ? 0x00AC : 0x00D0; return 4;                    /* RRA */
    case 0x00D0: z80.bank = z80.a; z80.pc = 0x00A1; return 10;                        /* RET: the bank register is A */
    case 0x00A1: z80.a++; z80.pc = 0x00A2; return 4;                                  /* INC A */
    case 0x00A2: z80_ram[0x9D] = z80.a; z80.pc = 0x00A5; return 13;                   /* LD ($9D),A: the next bank */
    case 0x00A5: z80.pc = 0x00A6; return 4;                                           /* SCF */
    case 0x00A6: z80.carry = 0; z80.pc = 0x00A7; return 4;                            /* CCF */
    case 0x00A7: z80.pc = 0x0095; return 10;                                          /* JP $0095 */
    default:     z80.pc = 0x007D; return 0;               /* (the 68k only patches in $7D, $D1, $29) */
    }
}

/* ---- SegaInit.asm:z80_reg, the startup program the 68k loads first ------------------------
 * It clears the Z80's RAM from $26 on, zeroes the registers from it and loops at 0. NHL '94 gives
 * it a run at power-on, and a second one it stops two instructions in (holding the bus): the
 * relay's PCM loop starts with the C that leaves. */
static uint16_t pop(void)
{
    uint16_t v = (uint16_t)(z80_ram[z80.sp & 0x1FFF] | z80_ram[(z80.sp + 1) & 0x1FFF] << 8);
    z80.sp = (uint16_t)(z80.sp + 2);
    return v;
}
static uint16_t operand(void) { return (uint16_t)(z80_ram[z80.pc + 1] | z80_ram[z80.pc + 2] << 8); }

static unsigned startup_instruction(void)
{
    uint16_t v, de = (uint16_t)(z80.d << 8 | z80.e);
    switch (z80.pc) {
    case 0x00: z80.a = 0; z80.pc = 0x01; return 4;                                    /* XOR A */
    case 0x01: v = operand(); z80.b = (uint8_t)(v >> 8); z80.c = (uint8_t)v; z80.pc = 0x04; return 10;   /* LD BC,$1FD9 */
    case 0x04: v = operand(); z80.d = (uint8_t)(v >> 8); z80.e = (uint8_t)v; z80.pc = 0x07; return 10;   /* LD DE,$0027 */
    case 0x07: z80.hl = operand(); z80.pc = 0x0A; return 10;                          /* LD HL,$0026 */
    case 0x0A: z80.sp = z80.hl; z80.pc = 0x0B; return 6;                              /* LD SP,HL */
    case 0x0B: z80_ram[z80.hl & 0x1FFF] = z80.a; z80.pc = 0x0C; return 7;             /* LD (HL),A */
    case 0x0C:                                                                        /* LDIR: a byte a time */
        z80_ram[de & 0x1FFF] = z80_ram[z80.hl & 0x1FFF];
        de++; z80.hl++;
        z80.d = (uint8_t)(de >> 8); z80.e = (uint8_t)de;
        v = (uint16_t)((z80.b << 8 | z80.c) - 1);
        z80.b = (uint8_t)(v >> 8); z80.c = (uint8_t)v;
        if (v) return 21;
        z80.pc = 0x0E; return 16;
    case 0x0E: pop(); z80.pc = 0x10; return 14;                                       /* POP IX */
    case 0x10: z80.iy = pop(); z80.pc = 0x12; return 14;                              /* POP IY */
    case 0x12: z80.pc = 0x14; return 9;                                               /* LD I,A */
    case 0x14: z80.pc = 0x16; return 9;                                               /* LD R,A */
    case 0x16: v = pop(); z80.d = (uint8_t)(v >> 8); z80.e = (uint8_t)v; z80.pc = 0x17; return 10;   /* POP DE */
    case 0x17: z80.hl = pop(); z80.pc = 0x18; return 10;                              /* POP HL */
    case 0x18: z80.a = (uint8_t)(pop() >> 8); z80.pc = 0x19; return 10;               /* POP AF */
    case 0x19: z80.pc = 0x1A; return 4;                                               /* EX AF,AF' */
    case 0x1A:                                                                        /* EXX */
        v = (uint16_t)(z80.b << 8 | z80.c); z80.b = (uint8_t)(z80.bc2 >> 8); z80.c = (uint8_t)z80.bc2; z80.bc2 = v;
        v = de; z80.d = (uint8_t)(z80.de2 >> 8); z80.e = (uint8_t)z80.de2; z80.de2 = v;
        v = z80.hl; z80.hl = z80.hl2; z80.hl2 = v;
        z80.pc = 0x1B; return 4;
    case 0x1B: v = pop(); z80.b = (uint8_t)(v >> 8); z80.c = (uint8_t)v; z80.pc = 0x1C; return 10;   /* POP BC */
    case 0x1C: v = pop(); z80.d = (uint8_t)(v >> 8); z80.e = (uint8_t)v; z80.pc = 0x1D; return 10;   /* POP DE */
    case 0x1D: z80.hl = pop(); z80.pc = 0x1E; return 10;                              /* POP HL */
    case 0x1E: z80.a = (uint8_t)(pop() >> 8); z80.pc = 0x1F; return 10;               /* POP AF */
    case 0x1F: z80.sp = z80.hl; z80.pc = 0x20; return 6;                              /* LD SP,HL */
    case 0x20: z80.pc = 0x21; return 4;                                               /* DI */
    case 0x21: z80.pc = 0x23; return 8;                                               /* IM 1 */
    case 0x23: z80_ram[z80.hl & 0x1FFF] = 0xE9; z80.pc = 0x25; return 10;             /* LD (HL),$E9 */
    default:   z80.pc = 0xFFFF; return 4;                 /* JP (HL), there for good */
    }
}

/* run the Z80 until it has caught up with the client's time */
static void catch_up(void)
{
    while (z80.running && !z80.bus_taken && z80.zc < z80.now) {
        if (z80.nops) routine_step();
        else if (z80.prog == Z80_STARTUP) z80.zc += startup_instruction() * CYCLE;
        else z80.zc += instruction() * CYCLE;
    }
}

void z80_relay_run(uint32_t mclk)
{
    z80.now += mclk;
    catch_up();
}

/* the Z80 and the YM2612 out of reset (the chip's reset clears its busy flag). A reset sets the
 * Z80's PC, not its other registers: C, the PCM loop's fraction, goes on from where it was (the
 * startup program leaves it) */
static void restart(void)
{
    z80.pc = 0x0000;
    z80.nops = z80.op = 0;
    z80.poll = 0;
    z80.ym_free = 0;
    z80_ym_reset();
}

void z80_relay_reset(uint16_t v)
{
    if (v & 0x100) {                                      /* released: the program starts at 0 */
        /* the relay once p_initialZ80 has loaded it, the startup program before (and NOPs before
         * that, which run to no effect) */
        if (!z80.running) {
            z80.prog = z80_ram[0] == 0x18 && z80_ram[1] == 0x27 ? Z80_RELAY
                     : z80_ram[0] == 0xAF && z80_ram[1] == 0x01 ? Z80_STARTUP : 0;
            if (!z80.prog) return;
            z80.running = 1;
            if (!z80.bus_taken) z80.zc = (z80.now + CYCLE - 1) / CYCLE * CYCLE;
            restart();
        }
    } else {                                              /* held in reset: a routine is cut short */
        z80.running = 0;
        z80.nops = z80.op = 0;
        z80.poll = 0;
    }
}

void z80_relay_busreq(uint16_t v)
{
    if (v & 0x100) { z80.bus_taken = 1; return; }
    if (z80.bus_taken && z80.running) z80.zc = (z80.now + CYCLE - 1) / CYCLE * CYCLE;   /* on again from now */
    z80.bus_taken = 0;
}

void z80_relay_frame(void) { z80.frame = z80.now; }

void z80_relay_seed(uint16_t pc, uint16_t hl, uint8_t c, uint8_t bank)
{
    z80.running = 1;
    z80.prog = Z80_RELAY;
    z80.zc = z80.now;
    z80.hl = hl;
    z80.c = c;
    z80.bank = bank;
    z80.nops = z80.op = 0;
    z80.poll = 0;
    if (pc >= 0x006E && pc <= 0x007A) {                   /* loading a pointer: the one hl came from */
        uint16_t start = (uint16_t)(z80_ram[0x24] << 8 | z80_ram[0x25]);
        z80.iy = hl == start ? 0x23 : 0x26;
        z80.pc = pc;                                      /* A and the carry as the instructions before left them */
        z80.carry = (uint8_t)(hl >> 15);
        z80.a = pc <= 0x0072 ? (uint8_t)(hl >> 8) : pc == 0x0073 ? (uint8_t)(hl >> 8)
              : pc == 0x0076 ? z80_ram[z80.iy] : (uint8_t)(z80_ram[z80.iy] << 1 | hl >> 15);
    } else if (pc >= 0x0067 && pc < 0x006E) {
        z80.iy = 0x26;
        z80.pc = 0x006B;
    } else if (pc > 0x007D && pc <= 0x0089) {
        z80.pc = 0x007D;                                  /* the pass again */
    } else if (pc > 0x0089 && pc <= 0x0095) {
        z80.pc = 0x008C;                                  /* (A and the carry: on from LD A,C) */
    } else if (pc >= 0x0098 && pc <= 0x00D0) {
        z80.pc = 0x0098;                                  /* the bank switch again */
    } else {
        z80.pc = 0x007D;
    }
}
