/* attract94.asm: new in 94, the EA Sports screen shown at power on; also the playoff matchup
 * pictures, helpers of the option screens and the options screen's vblank. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "attract_strings_gen.h"

/* attract94.asm:EASportsScreen. The EA Sports screen, faded in with the fades-only vblank,
 * until a button or $50 x 4 frames (about 5 s). */
void EASportsScreen(void)
{
    g.vbint = SYM_vb2;
    g.disflags &= (uint16_t)~HI(df32c);
    g.maps[2].colshift = 5;
    g.maps[1].vram = 0xA000;
    g.maps[1].colshift = 7;
    g.maps[0].vram = 0xC000;
    g.maps[0].colshift = 7;
    g.maps[2].vram = 0xF000;
    g.VSPRITES = 0xF800;
    g.VSCRLPM = 0xFC00;
    memset(g.palfadenew, 0, sizeof g.palfadenew);        /* all 64 colours black */
    CopyPaletteToCRAM();
    setVram_0();
    printz(str_EASportsScreen_1, sizeof str_EASportsScreen_1);
    uint32_t img = ART(EASportsMap), map = img + rd32(rom_ptr(img + 4));
    dobitmap(img + rd32(rom_ptr(img)), map, img + 8, 0, 0, rd16(rom_ptr(map)), rd16(rom_ptr(map) + 2), 0, 0xF);
    g.palfadenew[0] = 0xEEE;
    g.palfadenew[7] = 0;
    g.palcount = 0x18;                                    /* fade in */
    cpu_set_sr(0x2500);
    set_seed_hi(0x50);                                    /* (the seed's high word counts down) */
    do {
        uint16_t d0, d1, d2, d3;
        waitx(4, &d0, &d1, &d2, &d3);
        if (d1) break;
        set_seed_hi((uint16_t)(seed_hi() - 1));
    } while ((int16_t)seed_hi() >= 0);
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
    pal_load_w(0x13, &lg(t)->matchup[1][3], 2);          /* (TeamPalettes: colours 3 and 4 of its second) */
    DrawTeamBitmap(t, g.teambitmapchars);
    printz(str_DrawMatchupBitmaps_2, sizeof str_DrawMatchupBitmaps_2);
    t = g.HomeTeam;
    pal_load_w(0x11, &lg(t)->matchup[0][1], 2);          /* (colours 1 and 2 of its first) */
    g.palfadenew[0x15] = 0xEEE;
    DrawTeamBitmap(t, 2);
    setteams();
    g.palcount = 0x64;
}

/* attract94.asm:DrawTeamBitmap. Rows d1 * 2 and the next of TeamBitmaps' map (no tiles) at
 * printx / printy from char d4 -> d4. A league with banners of its own draws each from its 22
 * tiles, put into the first 22 chars of the set (the home team's at char 2, as loaded; the
 * visitors' remapped, as LoadSetupTiles loads theirs): one banner of a set shows at a time. */
uint16_t DrawTeamBitmap(uint16_t team, uint16_t chars)
{
    uint32_t img = ART(TeamBitmaps), map = img + rd32(rom_ptr(img + 4));
    if (team < LEAGUE_MAX && league.banner_tiles[team]) {
        if (chars == 2) DoDMApro(league.banner_tiles[team], 0x16 * 16, (uint16_t)(chars << 5));
        else remap(league.banner_tiles[team], SetupTilesRemap(), 0x16 * 16, (uint16_t)(chars << 5));
        return dobitmap(img + rd32(rom_ptr(img)), LG(BannerSlotMap), SYM_NoTiles, 0, 0, 0xB, 2, chars, 0);
    }
    return dobitmap(img + rd32(rom_ptr(img)), map, SYM_NoTiles, 0, (uint16_t)(league_art(team) << 1), rd16(rom_ptr(map)), 2, chars, 0);   /* (its art) */
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
