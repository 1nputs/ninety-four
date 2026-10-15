/* setup94.asm: setting up the rink, the teams and the period. Only the parts the game play
 * calls are here so far; the screens come with the front end and the game loop. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "anim_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))

/* the remap table that follows a DecompressGraphicsWithCallback call in routine [from, to) */
static uint32_t inline_table(uint32_t from, uint32_t to, const uint8_t table[8])
{
    return rom_find(from, to, table, 8);
}

/* setup94.asm:PeriodOver. The period is over: count it (gsp) and flip the ends; after the third
 * period a tied game goes to overtime, after overtime only a tied playoff game goes on; else
 * the game is over (gsp 4). Fade out and fall into IntermissionStart, which does not return. */
void PeriodOver(void)
{
    g.gsp++;
    g.gmode ^= 1 << gmdir;
    if (g.gsp >= 3) {
        if (g.gsp > 3) {
            g.gsp = 3;
            if (g.OptPlayMode == 0) g.gsp = 4;
        }
        if (rd16(team_struct(0) + tmscore) != rd16(team_struct(1) + tmscore)) g.gsp = 4;
    }
    forceblack();
    IntermissionStart();
}

/* resetplstuff's .top: team a2's skaters lose their flags and glide again */
static void reset_team(uint8_t *tm)
{
    tm[tmflags] &= (uint8_t)~0x10;                      /* team offside bit */
    struct SortObj *o = sortobj_at(rd16(tm + tmsort));
    for (int i = 0; i < 6; i++, o++) {
        o->pflags2 = 0;
        if (o->position < 0) continue;
        SetSPA(o, SPAglide);
        o->impact = 0;
        o->nopuck[0] = 0;
        o->pflags &= 0xC2;                               /* keep pfteam, pfgoal, pfna */
    }
}

/* setup94.asm:resetplstuff. Reset the team variables and the players of both teams. */
void resetplstuff(void)
{
    reset_team(team_struct(0));
    reset_team(team_struct(1));
}

/* setup94.asm:SprSort. Sort the sort objects by y (x in horizontal mode): Ylist = the
 * coordinates, OOlist = the order (byte offsets into Ylist), OOlistpos = each one's place.
 * A bubble sort over the 16 entries until nothing moves. */
void SprSort(void)
{
    uint8_t *ylist = RAM_W(RAM_OFF_Ylist), *oo = RAM_W(RAM_OFF_OOlist), *pos = RAM_W(RAM_OFF_OOlistpos);
    for (int i = 0; i < 0x10; i++) {
        struct SortObj *o = sortobj(i);
        wr16(ylist + 2 * i, (uint16_t)(g.sflags & HI(sfhor) ? o->Xpos : o->Ypos));
    }
    int moved;
    do {
        moved = 0;
        for (int i = 0; i < 0xF; i++) {
            uint8_t a = oo[i], b = oo[i + 1];
            if ((int16_t)rd16(ylist + a) <= (int16_t)rd16(ylist + b)) continue;
            oo[i + 1] = a;
            oo[i] = b;
            wr16(pos + a, (uint16_t)(i + 1));
            wr16(pos + b, (uint16_t)i);
            moved = 1;
        }
    } while (moved);
}

/* setup94.asm:setupEASNmap. Load the EASN logo tiles at EASNcset, colours remapped. */
void setupEASNmap(void)
{
    static const uint8_t table[8] = { 0x71, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    DecompressGraphicsWithCallback(SYM_EASNmap + 8, g.EASNcset, inline_table(SYM_setupEASNmap, SYM_setupEASNmap + 0x20, table));
}

/* setup94.asm:setupIceRinkPal. The ice rink palettes 0 and 1 (Rinktilelist) to palfadenew. */
void setupIceRinkPal(void)
{
    uint32_t pal = SYM_Rinktilelist + rd32(rom_ptr(SYM_Rinktilelist));
    for (int i = 0; i < 0x40; i++) RAM_W(RAM_OFF_palfadenew)[i] = rom_ptr(pal)[i];
}

/* setup94.asm:ReloadEnergyBarTiles. -> d4 = the char after them */
uint16_t ReloadEnergyBarTiles(void) { return DecompressGraphicsPlain(SYM_EnergyBarMap + 8, g.energybarchars); }

/* setup94.asm:ReloadCrowdTiles. -> d4 = the char after them */
uint16_t ReloadCrowdTiles(void) { return DecompressGraphicsPlain(SYM_CrowdFrameList + 8, g.animchars); }

/* setup94.asm:setplayercolors. Each team's colours (team data + its palette offset) to
 * palfadenew palettes 2 (home) and 3 (visitors). */
void setplayercolors(void)
{
    for (int away = 0; away < 2; away++) {
        uint32_t data = rd32(team_struct(away) + tmdata);
        const uint8_t *src = rom_ptr(data + (int16_t)rd16(rom_ptr(data + 2)) + 0x20 * away);
        uint8_t *dst = RAM_W(RAM_OFF_palfadenew + 0x40 + 0x20 * away);
        for (int i = 0; i < 0x20; i++) dst[i] = src[i];
    }
}
