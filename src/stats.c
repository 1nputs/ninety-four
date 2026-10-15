/* stats94.asm: the statistics screens. Only the parts the game play calls are here so far. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "stats_strings_gen.h"

/* stats94.asm:GetPlayerCount. a2 = team -> d0 = its players (records until a length word 2). */
uint16_t GetPlayerCount(uint8_t *tm)
{
    uint32_t a = rd32(tm + tmdata);
    a += (int16_t)rd16(rom_ptr(a));
    uint16_t n = 0;
    do {
        n++;
        a += (int16_t)rd16(rom_ptr(a));
        a += 8;
    } while (rd16(rom_ptr(a)) != 2);
    return n;
}

/* stats94.asm:GetGoalieCount. a2 = team -> d0 = its goalies (the nibbles of a team data word). */
uint16_t GetGoalieCount(const uint8_t *tm)
{
    uint32_t a = rd32(tm + tmdata);
    a += (int16_t)rd16(rom_ptr(a + 0xA));
    uint16_t w = rd16(rom_ptr(a)), n = 0;
    do { n++; w = (uint16_t)(w << 4); } while (w != 0);
    return n;
}

/* stats94.asm:ReloadRinkGraphics. After a screen over the rink: back to 32 columns, then reload
 * the rink (reversed in a reverse angle replay), team, font, framer, EASN, energy bar and crowd
 * tiles and the palettes. */
void ReloadRinkGraphics(void)
{
    static const uint8_t table[8] = { 0x43, 0x43, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    Vmaddr(g.VSCRLPM);
    vdp_data32(0);
    vdp_ctrl16((uint16_t)(0x8D00 | (g.VSCRLPM >> 8 >> 2)));
    vdp_ctrl16(SYM_VDPR_MODE4);
    g.Map3col1 = 5;
    g.disflags = saved;
    g.disflags |= HI(df32c);
    uint16_t c = DecompressGraphicsPlain(g.sflags4 & HI(4) ? SYM_RevRinkTiles : SYM_Rinktiles, g.rinkvrcset);
    LoadHomeTeamGfx(c);
    DecompressGraphicsWithCallback(SYM_SmallFontMap + 8, g.smallfontchars,
                                   rom_find(SYM_ReloadRinkGraphics, SYM_ReloadRinkGraphics + 0x80, table, 8));
    AddFramer(g.framercset);
    setupEASNmap();
    ReloadEnergyBarTiles();
    ReloadCrowdTiles();
    setupIceRinkPal();
    setplayercolors();
}

/* stats94.asm:SetupScreen. The common start of the stats screens: fade out, 40 columns, the
 * framer and small font tiles, the background (ScoutMap) once plain and once rows d0 to d1
 * with its palettes, a second small font, and plane A cleared -> d4 = the char after them. */
uint16_t SetupScreen(uint16_t row, uint16_t rows)
{
    static const uint8_t framer[8] = { 0x91, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t font2[8] = { 0xD1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    forceblack();
    g.disflags &= (uint16_t)~HI(dfok);
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    Vmaddr(g.VSPRITES);
    vdp_data32(0);
    vdp_ctrl16(SYM_VDPR_MODE4 + 0x81);
    g.Map3col1 = 6;
    vdp_ctrl16(SYM_VDPR_HSCROLL);
    Vmaddr(0);
    vdp_data32(0);
    g.disflags = saved;
    g.disflags &= (uint16_t)~HI(df32c);
    DecompressGraphicsWithCallback(SYM_FramerMap + 8, g.framercset, rom_find(SYM_SetupScreen, SYM_SetupScreen + 0x100, framer, 8));
    AddSmallFont(g.smallfontchars);
    printz(str_SetupScreen_1, sizeof str_SetupScreen_1);
    uint32_t map = SYM_ScoutMap + rd32(rom_ptr(SYM_ScoutMap + 4));
    const uint8_t *m = rom_ptr(map);
    uint16_t c = dobitmap(0, map, SYM_NoTiles, 0, 0, rd16(m), rd16(m + 2), 1, 0);
    printz(str_SetupScreen_2, sizeof str_SetupScreen_2);
    g.printy += row;
    uint32_t pal = SYM_ScoutMap + rd32(rom_ptr(SYM_ScoutMap));
    c = dobitmap(pal, map, SYM_ScoutMap + 8, 0, row, 0x28, (uint16_t)(rows - row), c, 0xD);
    g.smallfont2chars = c;
    c = DecompressGraphicsWithCallback(SYM_SmallFontMap + 8, c, rom_find(SYM_SetupScreen, SYM_SetupScreen + 0x100, font2, 8));
    printz(str_SetupScreen_3, sizeof str_SetupScreen_3);
    eraser(0x40, 0x20, 0x7FF);
    g.palcount = 0x18;
    return c;
}

/* stats94.asm:ExitStatsScreen. Leave a stats screen: fade out, then ReloadRinkGraphics. */
void ExitStatsScreen(void)
{
    forceblack();
    ReloadRinkGraphics();
}

/* stats94.asm:WaitVSyncAndReadInput. Wait for the next vblank and a new button press -> d1 (and
 * the other pad words). */
uint16_t WaitVSyncAndReadInput(uint16_t *d0, uint16_t *d2, uint16_t *d3)
{
    uint16_t d1;
    do {
        uint16_t v = g.vcount;
        while (g.vcount == v) hal_wait_vblank();
        getpzjoy(d0, &d1, d2, d3);
        nodiag(&d1, *d3);
    } while ((d1 & 0xFF) == 0);
    return d1;
}
