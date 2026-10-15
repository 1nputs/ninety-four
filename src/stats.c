/* stats94.asm: the statistics screens. Only the parts the game play calls are here so far. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"

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
