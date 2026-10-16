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
    cpu_set_sr(0x2500);
    uint8_t *count = RAM_W(RAM_OFF_seed);                 /* (the seed's high word counts down) */
    wr16(count, 0x50);
    do {
        uint16_t d0, d1, d2, d3;
        waitx(4, &d0, &d1, &d2, &d3);
        if (d1) break;
        wr16(count, (uint16_t)(rd16(count) - 1));
    } while ((int16_t)rd16(count) >= 0);
    cpu_set_sr(0x2700);
}

/* attract94.asm:UpdatePlayoffLevel. In the new playoffs: pojoy = 7 (or $B with FourWayPlay) -
 * OptNOP. */
void UpdatePlayoffLevel(void)
{
    if ((int16_t)g.OptPlayMode < 2 || g.OptPlayMode == 4) return;
    g.pojoy = (uint16_t)((g.FourWayPlay ? 0xB : 7) - g.OptNOP);
}

/* attract94.asm:DrawMatchupBitmaps. The visitors' and the home team's bitmaps (DrawTeamBitmap)
 * with their colours from TeamPalettes (the menu's teams in Regular Season and Shootout), then
 * the team structs (setteams) and the palette at once. */
void DrawMatchupBitmaps(void)
{
    if (g.OptPlayMode == 4 || g.OptPlayMode == 0) {
        g.HomeTeam = g.Opt1Team;
        g.VisTeam = g.Opt2Team;
    }
    printz(str_DrawMatchupBitmaps_1, sizeof str_DrawMatchupBitmaps_1);
    uint16_t t = g.VisTeam;
    memcpy(RAM_W(RAM_OFF_palfadenew + 0x26), rom_ptr(SYM_TeamPalettes + (uint32_t)(int16_t)(uint16_t)(t << 6) + 0x26), 4);
    DrawTeamBitmap(t, g.teambitmapchars);
    printz(str_DrawMatchupBitmaps_2, sizeof str_DrawMatchupBitmaps_2);
    t = g.HomeTeam;
    memcpy(RAM_W(RAM_OFF_palfadenew + 0x22), rom_ptr(SYM_TeamPalettes + (uint32_t)(int16_t)(uint16_t)(t << 6) + 2), 4);
    g.palfadenew[0x15] = 0xEEE;
    DrawTeamBitmap(t, 2);
    setteams();
    g.palcount = 0x64;
}

/* attract94.asm:DrawTeamBitmap. Rows d1 * 2 and the next of TeamBitmaps' map (no tiles) at
 * printx / printy from char d4 -> d4. */
uint16_t DrawTeamBitmap(uint16_t team, uint16_t chars)
{
    uint32_t img = SYM_TeamBitmaps, map = img + rd32(rom_ptr(img + 4));
    return dobitmap(img + rd32(rom_ptr(img)), map, SYM_NoTiles, 0, (uint16_t)(team << 1), rd16(rom_ptr(map)), 2, chars, 0);
}

/* attract94.asm:VBlank_SetOptions. The setup screen's vblank: unless dfng, the sprites (when
 * dfok) and the fade; then vcount and the music. */
void VBlank_SetOptions(void)
{
    if (!(g.disflags & HI(dfng))) {
        if (g.disflags & HI(dfok)) {
            g.disflags &= (uint16_t)~HI(dfok);
            DumpSprites2();
        }
        cramfade();
    }
    g.vcount++;
    p_music_vblank();
}
