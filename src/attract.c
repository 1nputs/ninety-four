/* attract94.asm: new in 94, the EA Sports screen shown at power on; also the playoff matchup
 * pictures, helpers of the option screens and the options screen's vblank. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "attract_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))

/* attract94.asm:EASportsScreen. The EA Sports screen, faded in with the fades-only vblank,
 * until a button or $50 x 4 frames (about 5 s). */
void EASportsScreen(void)
{
    g.vbint = SYM_vb2;
    g.disflags &= (uint16_t)~HI(df32c);
    g.Map3col1 = 5;
    g.VmMap2 = 0xA000;
    g.Map2col1 = 7;
    g.VmMap1 = 0xC000;
    g.Map1col1 = 7;
    g.VmMap3 = 0xF000;
    g.VSPRITES = 0xF800;
    g.VSCRLPM = 0xFC00;
    memset(RAM_W(RAM_OFF_palfadenew), 0, 0x80);          /* all 64 colours black */
    CopyPaletteToCRAM();
    setVram_0();
    printz(str_EASportsScreen_1, sizeof str_EASportsScreen_1);
    uint32_t img = SYM_EASportsMap, map = img + rd32(rom_ptr(img + 4));
    dobitmap(img + rd32(rom_ptr(img)), map, img + 8, 0, 0, rd16(rom_ptr(map)), rd16(rom_ptr(map) + 2), 0, 0xF);
    g.palfadenew[0] = 0xEEE;
    g.palfadenew[7] = 0;
    g.palcount = 0x18;                                    /* fade in */
    uint8_t *count = RAM_W(RAM_OFF_seed);                 /* (the seed's high word counts down) */
    wr16(count, 0x50);
    do {
        uint16_t d0, d1, d2, d3;
        waitx(4, &d0, &d1, &d2, &d3);
        if (d1) break;
        wr16(count, (uint16_t)(rd16(count) - 1));
    } while ((int16_t)rd16(count) >= 0);
}
