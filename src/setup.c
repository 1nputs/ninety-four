/* setup94.asm: setting up the rink, the teams and the period. Only the parts the game play
 * calls are here so far; the screens come with the front end and the game loop. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "anim_gen.h"
#include "setup_tables_gen.h"

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

/* setup94.asm:setupEASNmap. Load the EASN logo tiles at EASNcset, colours remapped -> d4. */
uint16_t setupEASNmap(void)
{
    static const uint8_t table[8] = { 0x71, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    return DecompressGraphicsWithCallback(SYM_EASNmap + 8, g.EASNcset, inline_table(SYM_setupEASNmap, SYM_setupEASNmap + 0x20, table));
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

/* setup94.asm:IntermissionStart. Between periods: reset the clock, two vblanks with the sound
 * off between, the end of period song, the ticker scores (and the playoff team stats at the end
 * of the game), the intermission, then the next period or the game over. Does not return. */
void IntermissionStart(void)
{
    ResetClock();
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    p_turnoff();
    v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    song(SYM_SngEOG);
    UpdateScores();
    if (g.gsp == 4) AddPlayoffTeamStats();
    Intermission();
    if (g.gsp == 4) GameOver();
    else StartPer();
}

/* setup94.asm:ReloadRefHorTiles. The horizontal referee tiles at ExtraChars -> d4. */
uint16_t ReloadRefHorTiles(void) { return DecompressGraphicsPlain(SYM_RefMap2 + 8, g.ExtraChars); }

/* setup94.asm:defaultsprites. d4 = first char: the 4 screen objects (sso, 0x14 bytes) and the 7
 * objects tied to the rink scrolling (pads, 0x1C bytes) from their tables, each given its
 * chars, then defaultsprites2 -> d4. */
uint16_t defaultsprites(uint16_t chars)
{
    uint8_t *a3 = RAM_W(RAM_OFF_sso);
    const uint16_t *t = listsso;
    for (int i = 0; i < 4; i++, a3 += 0x14) {
        struct SortObj *o = (struct SortObj *)a3;
        o->oldframe = 0xFFFF;                             /* not locked to the rink scroll */
        o->Xpos = (int16_t)*t++;
        o->Xfrac = *t++;
        o->frame = *t++;
        o->attribute = *t++;
        o->VRchar = chars;
        chars += *t++;
    }
    a3 = RAM_W(RAM_OFF_pads);
    t = listffo;
    for (int i = 0; i < 7; i++, a3 += 0x1C) {
        struct SortObj *o = (struct SortObj *)a3;
        ST_W(o->oldframe);                                /* tied to the rink scroll */
        o->Xpos = (int16_t)*t++;
        o->Ypos = (int16_t)*t++;
        o->Zpos = (int16_t)*t++;
        o->frame = *t++;
        o->attribute = *t++;
        o->VRchar = chars;
        chars += *t++;
    }
    return defaultsprites2(chars);
}

/* setup94.asm:defaultsprites2. The 16 sort objects (players, nets, puck, shadow) cleared and set
 * from the table (position, frame, attribute, chars, radius, first assignment, flags), with
 * the sort order lists, then SprSort -> d4. */
uint16_t defaultsprites2(uint16_t chars)
{
    uint8_t *oo = RAM_W(RAM_OFF_OOlist), *pos = RAM_W(RAM_OFF_OOlistpos);
    const uint16_t *t = dslist;
    for (uint16_t n = 0; n < 0x10; n++) {
        struct SortObj *o = sortobj(n);
        memset(o, 0, 0x80);
        o->SCnum = (int16_t)n;
        ST_W(o->oldframe);
        o->pnum = 0xFF;
        o->Xpos = (int16_t)*t++;
        o->Ypos = (int16_t)*t++;
        o->Zpos = (int16_t)*t++;
        o->frame = *t++;
        o->attribute = *t++;
        o->VRchar = chars;
        chars += *t++;
        o->radiusx = (int16_t)*t++;
        o->radiusy = (int16_t)*t++;
        o->asslist[0] = (uint8_t)*t++;
        o->pflags = (uint8_t)*t++;
        *oo++ = (uint8_t)(n << 1);
        wr16(pos, n); pos += 2;
    }
    SprSort();
    return chars;
}

/* setup94.asm:CopyTeamBlockMapData. a0 = the TeamBlocks map, d0 = team, d1 = VRAM address,
 * a1 = map word buffer: copy the team's 22 block chars from the loaded tiles to d1 on (VRAM to
 * VRAM DMA) and store their map words (priority, palette of the map word, the char) -> a1. */
uint8_t *CopyTeamBlockMapData(uint32_t map, uint16_t team, uint16_t vram, uint8_t *dst)
{
    const uint8_t *src = rom_ptr(map + 4 + (uint16_t)(team * 0x2C));
    for (int i = 0; i < 0x16; i++, src += 2, dst += 2) {
        dst[0] = src[0];
        dst[1] = 0;
        wr16(dst, (uint16_t)((rd16(dst) & 0xF800) | (vram >> 5) | 0x8000));
        uint16_t from = (uint16_t)((((rd16(src) & 0x7FF) + g.basetileoffset + 0x2C)) << 5);
        DoDMA_nd2(0x20, vram, from);
        vram += 0x20;
    }
    return dst;
}

/* setup94.asm:setupTeamBlocksMap. d4 = first char: the TeamBlocks tiles at d4 + $2C, then the
 * home and visitor blocks copied to basetileoffset and basetileoffset + $16 -> d4 =
 * basetileoffset + $2C. */
uint16_t setupTeamBlocksMap(uint16_t chars)
{
    g.basetileoffset = chars;
    uint8_t *buf = RAM_W(RAM_OFF_TeamBlockMap);
    DecompressGraphicsPlain(SYM_Teamblocksmap + 8, (uint16_t)(chars + 0x2C));
    uint32_t map = SYM_Teamblocksmap + rd32(rom_ptr(SYM_Teamblocksmap + 4));
    buf = CopyTeamBlockMapData(map, g.HomeTeam, (uint16_t)(g.basetileoffset << 5), buf);
    CopyTeamBlockMapData(map, g.VisTeam, (uint16_t)((g.basetileoffset + 0x16) << 5), buf);
    return (uint16_t)(g.basetileoffset + 0x2C);
}

/* setup94.asm:setupice. Set up the rink: the VDP layout (32 columns, maps at $C000 / $E000 /
 * $F000, sprites at $DC00, scroll at $FC00), clear VRAM, then load the rink, team, EASN, energy
 * bar, crowd, sprite, framer, font and team block tiles, the default objects and palettes, flip
 * the goals for the second period's ends, the pad and logo sprites, and the game vblank. */
void setupice(void)
{
    static const uint8_t small[8] = { 0x43, 0x43, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t big[8] = { 0x71, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    g.disflags |= HI(df32c);
    g.VmMap2 = 0xC000; g.Map2col1 = 6;
    g.VSPRITES = 0xDC00;
    g.VmMap1 = 0xE000; g.Map1col1 = 6;
    g.VmMap3 = 0xF000; g.Map3col1 = 5;
    g.VSCRLPM = 0xFC00;
    setVram(0);
    g.sflags &= (uint16_t)~HI(sfpz);
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    g.Hpos = 0;
    g.Vpos = 0;
    g.Oldrow = 0x7D0;
    ST_W(g.zamx);
    DoFill(0x800, g.VmMap1, 0x7FF);
    g.rinkvrcset = 0;
    uint16_t c = DecompressGraphicsPlain(g.sflags4 & HI(4) ? SYM_RevRinkTiles : SYM_Rinktiles, 0);
    c = LoadHomeTeamGfx(c);
    g.EASNcset = c;
    c = setupEASNmap();
    g.energybarchars = c;
    c = DecompressGraphicsPlain(SYM_EnergyBarMap + 8, c);
    g.animchars = c;
    c = DecompressGraphicsPlain(SYM_CrowdFrameList + 8, c);
    g.arenaanimchars = c;
    c += 0x32;
    g.spritechars = c;
    c = defaultsprites(c);
    setupIceRinkPal();
    c = AddFramer(c);
    g.smallfontchars = c;
    c = DecompressGraphicsWithCallback(SYM_SmallFontMap + 8, c, inline_table(SYM_setupice, SYM_setupice + 0x200, small));
    g.BigFontChars = c;
    c = DecompressGraphicsWithCallback(SYM_BigFontMap + 8, c, inline_table(SYM_setupice + 1, SYM_setupice + 0x200, big));
    c = setupTeamBlocksMap(c);
    g.ExtraChars = c;
    if (g.gmode & (1 << gmdir))
        for (int i = 0; i < 12; i++) sortobj(i)->pflags ^= 1 << pfgoal;
    setplayercolors();
    g.PadControlBits = 0xFFFF;
    g.PadControlBits34 = 0xFFFF;
    memset(RAM_W(RAM_OFF_padcont), 0, 12);
    ST_W(g.c1playernum);
    ST_W(g.c2playernum);
    struct Vid v = { RAM_W(RAM_OFF_DMAList), RAM_W(RAM_OFF_Satt), 1 };
    uint8_t *a3 = RAM_W(RAM_OFF_pads);
    for (int i = 0; i < 6; i++, a3 += 0x1C) addframe2(0, 0, (struct SortObj *)a3, &v);   /* pads, logo, gloves */
    g.DMAlistend = ram_addr(v.dma);
    DoDMAList();
    g.disflags = saved;
    g.vbint = SYM_VBlank;
    g.disflags &= (uint16_t)~(HI(dfok) | HI(dfng));
    cpu_set_sr(0x2300);
}

/* setup94.asm:InitTeamStructure. Team struct a2 for team d0: the team number ($28), its data
 * (tmdata, from TeamList) and its line sets at $16A (the layout by OptLine). */
void InitTeamStructure(uint16_t team, uint8_t *tm)
{
    wr16(tm + 0x28, team);
    uint32_t data = rd32(rom_ptr(SYM_TeamList + (uint16_t)(team << 2)));
    wr32(tm + tmdata, data);
    const uint8_t *src = rom_ptr(rd32(tm + tmdata) + (int16_t)rd16(rom_ptr(data + 6)));
    uint8_t *dst = tm + 0x16A;
    if (g.OptLine != 0) {
        memcpy(dst, src, 8);
        memcpy(dst + 8, src + 16, 12 * 4);
    } else {
        memcpy(dst, src + 8, 14 * 4);
    }
}

/* setup94.asm:setteams. Both team structs from HomeTeam / VisTeam, with their sort objects. */
void setteams(void)
{
    wr16(team_struct(0) + tmsort, RAM_OFF_SortCords);
    InitTeamStructure(g.HomeTeam, team_struct(0));
    wr16(team_struct(1) + tmsort, RAM_OFF_SortCords + 6 * 0x80);
    InitTeamStructure(g.VisTeam, team_struct(1));
}

/* setup94.asm:clearTeamStats. Clear both team structs, keeping the first $1A0 bytes of each
 * hot / cold table (+$1A2, through homehotcoldsave / awayhotcoldsave) and the goalie words; the
 * last roster slot's place is -1. Falls into setteams. */
void clearTeamStats(void)
{
    uint8_t *home = team_struct(0), *away = team_struct(1);
    uint16_t hg = rd16(home + tmgoalie), ag = rd16(away + tmgoalie);
    memcpy(RAM_W(RAM_OFF_homehotcoldsave), home + 0x1A2, 0x1A0);
    memcpy(RAM_W(RAM_OFF_awayhotcoldsave), away + 0x1A2, 0x1A0);
    memset(home, 0, tmsize);
    memset(away, 0, tmsize);
    memcpy(home + 0x1A2, RAM_W(RAM_OFF_homehotcoldsave), 0x1A0);
    memcpy(away + 0x1A2, RAM_W(RAM_OFF_awayhotcoldsave), 0x1A0);
    wr16(away + tmgoalie, ag);
    wr16(home + tmgoalie, hg);
    home[tmpdst + 0x34] = 0xFF;
    away[tmpdst + 0x34] = 0xFF;
    setteams();
}

/* setup94.asm:ReloadRefTiles. The referee tiles (RefsMap) at ExtraChars -> d4. */
uint16_t ReloadRefTiles(void) { return DecompressGraphicsPlain(SYM_RefsMap + 8, g.ExtraChars); }
/* setup94.asm:ReloadFaceOffTiles. The faceoff sprite tiles at faceoffvrcset -> d4. */
uint16_t ReloadFaceOffTiles(void) { return DecompressGraphicsPlain(SYM_FaceOffSprites + 8, g.faceoffvrcset); }
/* setup94.asm:ReloadFaceOffMap. The faceoff map tiles at ExtraChars -> d4. */
uint16_t ReloadFaceOffMap(void) { return DecompressGraphicsPlain(SYM_FaceOffMap + 8, g.ExtraChars); }

/* setup94.asm:setupice_highlight. Rebuild the rink objects for a highlight without reloading
 * the tiles (the sprite chars setupice kept in spritechars), then the game vblank and a fade
 * in. */
void setupice_highlight(void)
{
    forceblack();
    g.sflags &= (uint16_t)~HI(sfpz);
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    g.Hpos = 0;
    g.Vpos = 0;
    g.Oldrow = 0x7D0;
    ST_W(g.zamx);
    defaultsprites(g.spritechars);
    setupIceRinkPal();
    if (g.gmode & (1 << gmdir))
        for (int i = 0; i < 12; i++) sortobj(i)->pflags ^= 1 << pfgoal;
    setplayercolors();
    g.PadControlBits = 0xFFFF;
    g.PadControlBits34 = 0xFFFF;
    memset(RAM_W(RAM_OFF_padcont), 0, 12);
    ST_W(g.c1playernum);
    ST_W(g.c2playernum);
    setupEASNmap();
    struct Vid v = { RAM_W(RAM_OFF_DMAList), RAM_W(RAM_OFF_Satt), 1 };
    uint8_t *a3 = RAM_W(RAM_OFF_pads);
    for (int i = 0; i < 6; i++, a3 += 0x1C) addframe2(0, 0, (struct SortObj *)a3, &v);
    g.DMAlistend = ram_addr(v.dma);
    DoDMAList();
    g.palcount = 0x1C;
    g.disflags = saved;
    g.vbint = SYM_VBlank;
    g.disflags &= (uint16_t)~(HI(dfok) | HI(dfng));
    cpu_set_sr(0x2300);
}

/* setup94.asm:ExitToOpening. The title song, then the options again (Opening2, with a new
 * stack). From the game over, the end of a demo and the end of a shootout. */
void ExitToOpening(void)
{
    song(SYM_SngTitle);
    restart_at(Opening2);
}

/* setup94.asm:Opening. The title screen, then into Opening2. From Begin. */
void Opening(void)
{
    KillCrowd();
    newTitleScreen();
    Opening2();
}

/* setup94.asm:Opening2. With a new stack: clear the game's variables ($B000-$D03D), the
 * options (GameSetUp), the name entry, then Opening3. Does not return. */
void Opening2(void)
{
    KillCrowd();
    cpu_set_sr(0x2700);
    memset(RAM_W(RAM_OFF_VSCRLPM), 0, 0xD03E - RAM_OFF_VSCRLPM);
    GameSetUp();
    UserNameEntry();
    Opening3();
}

/* setup94.asm:Opening3. The playoff screen (in the playoffs), the scouting report (not in a
 * shootout), then the game (StartGame). Does not return. */
void Opening3(void)
{
    PlayoffScreen();
    if (!(g.gmode2 & HI(0))) ScoutingReport();
    StartGame();
}
