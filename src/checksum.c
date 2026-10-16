/* checksum94.asm: the ROM check at power on. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"

/* checksum94.asm:ValidationRoutine. Add up every ROM long below this routine but the one
 * holding the header's checksum ($18C); the wrong sum turns the screen red and hangs. (Its loop's
 * labels ValidationLoop, SkipIncrement, ContinueValidation and the error path's VDPErrorSetup,
 * VRAMWriteLoop and Halt are global in the asm.) */
void ValidationRoutine(void)
{
    uint32_t sum = 0;
    for (uint32_t a = 0; a < SYM_ValidationRoutine; a += 4)
        if (a != 0x18C) sum += rd32(rom_ptr(a));
    if (sum == 0x8AB9F121) return;
    vdp_ctrl16(SYM_VDPR_AUTOINC + 2);
    vdp_ctrl16(SYM_VDPR_MODE1 + 4);
    vdp_ctrl16(SYM_VDPR_BGCOL);
    vdp_ctrl16(SYM_VDPR_MODE2 + 0x44);                    /* display on, no interrupt */
    vdp_ctrl16(0xC000);                                   /* CRAM, colour 0 */
    for (int i = 0; i < 64; i++) vdp_data(0xE);           /* all red */
    for (;;) hal_wait_vblank();                           /* (bra.s Halt: the frames go by) */
}
