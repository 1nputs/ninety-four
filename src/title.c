/* title94.asm: the title screen and credits, the high score screen, the saved lines, and the
 * helpers of the 94 additions: songs, the shootout skate paths, one-timers, the hot / cold icons,
 * board falls, the pad switch, the matchups' "played by" lines, the crowd and win records. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "anim_gen.h"
#include "title_strings_gen.h"

static inline int16_t puckx(void) { return (int16_t)g.puckx[0]; }
static inline int16_t abs16(int16_t v) { return v < 0 ? (int16_t)-v : v; }   /* sub / bpl / neg */

/* title94.asm:ChooseSong. SongNum = song SongIndex of the six of team HmTeam (TeamSongs), or one
 * of the eight RandomSongs when that is 0; none ($FFFF) with gmode bit 4. */
void ChooseSong(void)
{
    uint16_t n;
    if (g.gmode & 0x10) {
        n = 0xFFFF;
    } else {
        g.sflags8 &= (uint8_t)~0x40;
        n = rom_ptr(LG(TeamSongs))[(int16_t)(g.HmTeam * 6 + g.SongIndex)];
        if (n == 0) n = rom_ptr(SYM_RandomSongs)[randomd0(8) & 7];
    }
    g.SongNum = n;
}

/* title94.asm:EndOneTimer. End player a3's one-timer: flags cleared, onetimerplayer = none,
 * glide, then assexit (goalie) or Setplass, and restart the animation. */
void EndOneTimer(struct SortObj *o)
{
    o->pflags3 &= (uint8_t)~(1 << pf3onetimer);
    o->pflags &= (uint8_t)~0x20;
    o->pflags2 &= (uint8_t)~2;
    g.onetimerflags = 0;
    ST_W(g.onetimerplayer);
    SetSPA(o, SPAglide);
    if (o->position < 0) assexit(o);
    else Setplass(o);
    o->SPAnum = 0;
    ST_W(o->SPAcnt);
}

/* title94.asm:getFgtbyte. -> d0 = the fighting byte of a2 / 4. */
uint16_t getFgtbyte(struct SortObj *a2, struct SortObj *a3) { return a2->fighting >> 2; }

/* title94.asm:chkFgtBit1. -> Z: bit 1 of a2's fighting byte is clear. */
int chkFgtBit1(struct SortObj *a2, struct SortObj *a3) { return !(a2->fighting & 2); }

/* title94.asm:ReadGoaliePulled. -> the flags of tst.w on the goalie word of a3's team (Z: the
 * goalie is pulled). */
uint8_t ReadGoaliePulled(struct SortObj *o)
{
    uint16_t v = team_struct(o->pflags & (1 << pfteam))->goalie;
    return (uint8_t)((v == 0 ? CCR_Z : 0) | (v & 0x8000 ? CCR_N : 0));
}

/* title94.asm:ShootoutShootCheck. Shootout skater a3 with the puck -> Z: hold the shot (before
 * shootoutclock 4, before the path starts, then until the path end is near or the puck has
 * stopped after $F). Z clear = shoot now; also when it is not the shootout carrier. */
int ShootoutShootCheck(struct SortObj *o)
{
    if (!(g.gmode2 & HI(1))) return 0;
    if (o->SCnum != (int16_t)puck_carrier()) return 0;
    if ((int16_t)g.shootoutclock <= 3) return 1;
    if ((g.sopathx >> 8) == 0x80) return 1;
    if (!(g.sflags7 & 8)) {
        if (g.puckvx != 0 || g.puckvy != 0) return 0;
        return (int16_t)g.shootoutclock < 0xF;
    }
    int16_t x = (int16_t)g.sopathx, y = (int16_t)g.sopathy;
    if (!(o->pflags & 0x80)) { x = (int16_t)-x; y = (int16_t)-y; }
    x = abs16((int16_t)(x - o->Xpos));
    y = abs16((int16_t)(y - o->Ypos));
    if (x > (int16_t)g.sopathend || y > (int16_t)g.sopathend) return 0;
    return 1;
}

/* title94.asm:SkatePath. Shootout skate path: d0 / d1 = the current point (sopathx / sopathy,
 * mirrored for the bottom net in the test); within $A of it (or $12 while standing) go on to
 * the next one. Unchanged once the path is over. */
void SkatePath(int16_t *x, int16_t *y, struct SortObj *o)
{
    if ((g.sopathx >> 8) == 0x80) return;
    int16_t dx = (int16_t)g.sopathx, dy = (int16_t)g.sopathy;
    if (!(o->pflags & 0x80)) { dx = (int16_t)-dx; dy = (int16_t)-dy; }
    int standing = o->Xvel == 0 && o->Yvel == 0;
    dx = abs16((int16_t)(dx - o->Xpos));
    if ((standing && dx <= 0x12) || dx <= 0xA) {
        dy = abs16((int16_t)(dy - o->Ypos));
        if ((standing && dy <= 0x12) || dy <= 0xA) NextPathPoint(o);
    }
    *x = (int16_t)g.sopathx;
    *y = (int16_t)g.sopathy;
}

/* title94.asm:NextPathPoint. Step the shootout path sopath to its next point (x turned for a
 * left-handed skater a3); at the end marker ($80) set sflags7 bit 3, sopathend and sopathdir. */
void NextPathPoint(struct SortObj *o)
{
    if ((g.sopathx >> 8) == 0x80) return;
    g.sopathpoint++;
    const uint8_t *path = rom_ptr(rd32(rom_ptr(SYM_ShootoutPaths + (int16_t)(g.sopath << 2))));
    const uint8_t *pt = path + (int16_t)(g.sopathpoint << 2);
    g.sopathx = rd16(pt);
    if (o->handed[0] & 1) g.sopathx = (uint16_t)-g.sopathx;
    g.sopathy = rd16(pt + 2);
    if (pt[4] == 0x80) {
        g.sflags7 |= 8;
        g.sopathend = pt[5];
        g.sopathdir = rd16(pt + 6);
    }
}

/* title94.asm:StartShootoutPath. Pick one of the seven shootout skate paths at random and
 * start it (NextPathPoint, for skater a3). */
void StartShootoutPath(struct SortObj *o)
{
    uint16_t p;
    do p = randomd0(7); while (p > 6);
    g.sopath = p;
    g.sflags7 &= (uint8_t)~8;
    g.sopathpoint = 0xFFFF;
    g.sopathx = 0;
    NextPathPoint(o);
}

/* title94.asm:CheckScoreLeader. Compares the scores and tests a word of the leader, but the
 * branch on the result is a bne.w *+4: nothing happens. */
void CheckScoreLeader(void) {}

/* title94.asm:RandomFaceoffAnim. -> d0 = a random faceoff animation (FaceoffAnims), 6 when
 * the faceoff spot is at fox <= -$40, foy <= $C0. */
uint16_t RandomFaceoffAnim(void)
{
    uint16_t a = rd16(rom_ptr(SYM_FaceoffAnims) + 2 * (randomd0(0x64) & 7));
    if ((int16_t)g.fox <= -0x40 && (int16_t)g.foy <= 0xC0) a = 6;
    return a;
}

/* title94.asm:ClearLeadSong. */
void ClearLeadSong(void) { g.sflags2 &= (uint16_t)~HI(5); }

/* title94.asm:LeadSongExit. LeadSong's exit: sflags2 as it was on entry. */
void LeadSongExit(uint16_t sflags2) { g.sflags2 = sflags2; }

/* title94.asm:LeadSong. Outside a shootout, when one team leads: choose the lead song (SongIndex
 * 1 when the home team leads, 4 when the away team does) and flag it in sflags8 bit 6. The
 * sflags2 bits it tests on the way are put back on exit. */
void LeadSong(void)
{
    uint16_t saved = g.sflags2;
    if (g.gmode2 & HI(1)) { LeadSongExit(saved); return; }
    struct Team *leader = team_struct(1);
    int16_t diff = (int16_t)(team_struct(0)->ap - team_struct(1)->ap);
    if (diff == 0) { LeadSongExit(saved); return; }
    if (diff < 0) {
        if (!(g.sflags2 & HI(6))) { ClearLeadSong(); g.sflags2 |= HI(6); }
    } else {
        leader = team_struct(0);
        if (g.sflags2 & HI(6)) { ClearLeadSong(); g.sflags2 &= (uint16_t)~HI(6); }
    }
    int was = (g.sflags2 & HI(5)) != 0;
    g.sflags2 |= HI(5);
    if (was) { LeadSongExit(saved); return; }
    g.HmTeam = g.HomeTeam;
    g.SongIndex = leader == team_struct(0) ? 1 : 4;
    ChooseSong();
    g.sflags8 |= 0x40;
    LeadSongExit(saved);
}

/* title94.asm:rtsChgplayer. chgplayer's early exit. */
void rtsChgplayer(void) {}
/* title94.asm:swpchk. jmp Sweepcheck. */
void swpchk(struct SortObj *o) { Sweepcheck(o); }
/* title94.asm:j_setc1player. jmp setc1player. */
void j_setc1player(uint16_t player) { setc1player(player); }
/* title94.asm:j_setc2player. jmp setc2player. */
void j_setc2player(uint16_t player) { setc2player(player); }

/* title94.asm:chgplayer. Pad d4 takes the free skater of its team nearest to where the puck
 * is going (not a goalie, not unavailable, locked or on the other pad, and in a penalty shot
 * or shootout only the shooter or the goalie). When that is its own player, a3 sweep checks.
 * If nobody qualifies the 68k compares a register the caller left (d6); the C does nothing. */
void chgplayer(uint16_t pad, struct SortObj *o)
{
    if (g.sflags5 & 0x40) { rtsChgplayer(); return; }    /* never set */
    int16_t tx = (int16_t)(((int16_t)g.puckvx >> 8) + puckx());
    int16_t ty = (int16_t)(((int16_t)g.puckvy >> 8) + (int16_t)g.pucky);
    rp cont = RAM(cont1team), cur = RAM(c1playernum);
    struct SortObj *pl = sortobj(r16(cont, pad) == 1 ? 0 : 6);
    uint16_t mine = r16(cur, pad), other = r16(cur, (pad ^ 2));
    uint32_t best = 0xFFFFFFFF;
    int found = 0;
    uint16_t pick = 0;
    for (int i = 0; i < 6; i++, pl++) {
        if (pl->position <= 0) continue;                 /* not the goalie */
        if (pl->pflags2 & 4) continue;                   /* unavailable */
        if (pl->SCnum != (int16_t)mine && (pl->pflags & (1 << pfjoycon))) continue;
        if ((g.BA_PS_flags & HI(2)) && pl->SCnum != (int16_t)g.BA_Sktr_SCnum && pl->SCnum != (int16_t)g.BA_Goalie_SCnum) continue;
        if (pl->pflags & (1 << pfalock)) continue;
        int16_t dx = (int16_t)(tx - pl->Xpos), dy = (int16_t)(ty - pl->Ypos);
        uint32_t dist = (uint32_t)((int32_t)dx * dx) + (uint32_t)((int32_t)dy * dy);
        if (dist > best) continue;
        if (pl->SCnum == (int16_t)other) continue;
        best = dist;
        pick = (uint16_t)pl->SCnum;
        found = 1;
    }
    if (!found) return;
    if (pick == mine) swpchk(o);
    else if (pad == 0) j_setc1player(pick);
    else j_setc2player(pick);
}

/* title94.asm:CountButtonPress. Count a button press of pad d4 (homepresses / awaypresses), and
 * in the next long the presses with a direction other than 8 or a3's facing. Not in demos
 * (gmode bit 0). */
void CountButtonPress(int16_t pad, struct SortObj *o)
{
    if (g.gmode & 1) return;
    rp cnt = ram_at(RAM_BASE + (pad ? RAM_OFF_awaypresses : RAM_OFF_homepresses));
    uint16_t dir = g.TempWord1;
    w32(cnt, 0, r32(cnt, 0) + 1);
    if (dir == 8 || dir == (uint16_t)o->facedir) return;
    w32(cnt, 4, r32(cnt, 4) + 1);
}

/* title94.asm:PSandSOpassdir. Penalty shot / shootout: passdir = the direction at the end of
 * the skate path, turned (passdirlist) for the bottom net. */
void PSandSOpassdir(struct SortObj *o)
{
    uint16_t d = g.sopathdir;
    if (!(o->pflags & 0x80)) d = rd16(rom_ptr(SYM_passdirlist) + (int16_t)(d << 1));
    g.passdir = d;
}

/* title94.asm:LoadHomeTeamGfx. d4 = char: load the home team's graphics (TeamGfxList) at
 * d4 - $18 -> d4 = the char after them. */
uint16_t LoadHomeTeamGfx(uint16_t start)
{
    uint32_t gfx = rd32(rom_ptr(LG(TeamGfxList) + (int16_t)(g.HomeTeam << 2))) + 8;
    return DecompressGraphicsPlain(gfx, (uint16_t)(start - 0x18));
}

/* title94.asm:HotColdIcon. Team a2: the icon by the name of iconplayer (x 2 away, $20 home,
 * y $19): the hot icon for the team's hot player, the cold one for its cold player, else clear
 * it. */
void HotColdIcon(struct Team *tm)
{
    int home = tm == team_struct(0);
    g.iconx = home ? 0x20 : 2;
    uint32_t icon;
    uint16_t chars;
    if ((home ? g.homehotplayer : g.awayhotplayer) == g.iconplayer) {
        chars = g.hoticonchars; icon = ART(HotIconMap);
    } else if ((home ? g.homecoldplayer : g.awaycoldplayer) == g.iconplayer) {
        chars = g.coldiconchars; icon = ART(ColdIconMap);
    } else {
        g.printx = g.iconx;
        g.printy = 0x19;
        eraser(8, 3, 0x7FF);
        return;
    }
    g.printx = g.iconx;
    g.printy = 0x19;
    const uint8_t *hdr = rom_ptr(icon);
    uint32_t pal = icon + rd32(hdr), map = icon + rd32(hdr + 4);
    const uint8_t *m = rom_ptr(map);
    dobitmap(pal, map, SYM_NoTiles, 0, 0, rd16(m), rd16(m + 2), chars, 0);
}

/* title94.asm:clrTmPdst. Team a0: all 26 roster slots on the bench (-2), then the end word -1. */
void clrTmPdst(struct Team *tm)
{
    for (int i = 0; i < 26; i++) tm->pdst[i] = 0xFFFE;
    tm->penbox[0] = tm->penbox[1] = 0xFF;                 /* (the end word -1 after them: the penalty box empty) */
}

/* title94.asm:ClearPenalties. No penalties: the counters, PenBuf and both teams' slots. */
void ClearPenalties(void)
{
    g.PBnum = 0;
    g.Penaltytimer = 0;
    g.Pencntdwn = 0;
    for (int i = 0; i < 0x44; i++) w8(RAM(PenBuf), i, 0);           /* 17 longs */
    clrTmPdst(team_struct(0));
    clrTmPdst(team_struct(1));
}

/* move a coordinate half way to a target (sub / asr.w #1 / add) */
static int16_t half_way(int16_t pos, int16_t target) { return (int16_t)(pos + ((int16_t)(target - pos) >> 1)); }

/* title94.asm:PlaceBoardFall. Player a3 falling into the boards (or over the bench): move him
 * half way to the boards for the animation SPA. Front / back: y to +-$124 (+-$116 near the
 * corners, by FallXPos), only toward the boards. Sides: x to +-$82 (+-$88 over the bench), the
 * side by his flip (attribute bit 11). */
void PlaceBoardFall(struct SortObj *o)
{
    int flip = (o->attribute & 0x0800) != 0;
    int corner = (int16_t)g.FallXPos > 0x56 || (int16_t)g.FallXPos <= -0x56;
    switch (o->SPA) {
    case SPAbenchcheckl: o->Xpos = half_way(o->Xpos, flip ? 0x88 : -0x88); break;
    case SPAbenchcheckr: o->Xpos = half_way(o->Xpos, flip ? -0x88 : 0x88); break;
    case SPAboardleft:   o->Xpos = half_way(o->Xpos, flip ? 0x82 : -0x82); break;
    case SPAboardright:  o->Xpos = half_way(o->Xpos, flip ? -0x82 : 0x82); break;
    case SPAboardfront: {
        int16_t d = (int16_t)((corner ? 0x116 : 0x124) - o->Ypos);
        if (d >= 0) o->Ypos = (int16_t)(o->Ypos + (d >> 1));
        break;
    }
    case SPAboardback: {
        int16_t d = (int16_t)((corner ? -0x116 : -0x124) - o->Ypos);
        if (d < 0) o->Ypos = (int16_t)(o->Ypos + (d >> 1));
        break;
    }
    }
}

/* title94.asm:SetSflags8Bit1. Sets sflags8 bit 1 (the bclr after the bra.w is never reached). */
void SetSflags8Bit1(void) { g.sflags8 |= 2; }

/* PrintPlayerGoals / PrintPlayerAssists: " (n)" with byte d0 of the team table, then the next
 * row at x $E */
void PrintParenNumber(const uint8_t *table, uint16_t player)
{
    int16_t n = (int8_t)table[(int16_t)player];
    uint16_t width = n > 0x63 ? 3 : n > 9 ? 2 : 1;
    print(PushNumberWidth((uint16_t)n, width));
    printz(str_PrintParenNumber_1, sizeof str_PrintParenNumber_1);
    g.printy++;
    g.printx = 0xE;
}

/* title94.asm:PrintPlayerGoals. d0 = player, a2 = team: " (goals)", next row. */
void PrintPlayerGoals(uint16_t player, struct Team *tm)
{
    printz(str_PrintPlayerGoals_1, sizeof str_PrintPlayerGoals_1);
    PrintParenNumber(tm->pgoals, player);
}

/* title94.asm:PrintPlayerAssists. d0 = player, a2 = team: " (assists)", next row. */
void PrintPlayerAssists(uint16_t player, struct Team *tm)
{
    printz(str_PrintPlayerAssists_1, sizeof str_PrintPlayerAssists_1);
    PrintParenNumber(tm->passists, player);
}

/* title94.asm:ReadLineData. Read the $100 bytes at save RAM LineDataBase to databuffer;
 * sflags bit 4 cleared, lastsfx = none, recbpr = M68K_RAM. */
void ReadLineData(void)
{
    ReadSRAM(SYM_LineDataBase, 0x100, RAM_BASE + RAM_OFF_databuffer);
    g.sflags &= (uint16_t)~HI(sfwrap);
    g.lastsfx = 0xFFFF;
    g.recbpr = RAM_BASE;
}

/* a picture (TitleScreenImg ...): the offsets of its palettes and map, then its tiles */
struct Picture { uint32_t pal, map, tiles; uint16_t w, h; };
static struct Picture picture(uint32_t img)
{
    struct Picture p = { img + rd32(rom_ptr(img)), img + rd32(rom_ptr(img + 4)), img + 8, 0, 0 };
    p.w = m16(p.map);
    p.h = m16(p.map + 2);
    return p;
}

/* the screens' VRAM layout: planes at $C000 / $A000, window $F000, sprites $F800, hscroll $FC00 */
static void screen_layout(void)
{
    g.Map3col1 = 5;
    g.VmMap2 = 0xA000;
    g.Map2col1 = 7;
    g.VmMap1 = 0xC000;
    g.Map1col1 = 7;
    g.VmMap3 = 0xF000;
    g.VSPRITES = 0xF800;
    g.VSCRLPM = 0xFC00;
}

/* title94.asm:HiScoreScreen. The attract mode's second screen (a bitmap and HiScoreImg) with
 * the fades-only vblank, for $50 x 4 frames or until a button. */
void HiScoreScreen(void)
{
    g.vbint = SYM_vb2;
    g.disflags &= (uint16_t)~HI(df32c);
    screen_layout();
    forceblack();
    ram_fill(RAM(palfadenew), 0, 0x80);
    setVram_0();
    uint8_t at[2][sizeof str_HiScoreScreen_1];            /* (str_HiScoreScreen_1 and _2 at the league's places) */
    for (int k = 0; k < 2; k++) {
        memcpy(at[k], k ? str_HiScoreScreen_2 : str_HiScoreScreen_1, sizeof at[k]);
        at[k][3] = league.hiscore_at[k][0];
        at[k][4] = league.hiscore_at[k][1];
    }
    printz(at[0], sizeof at[0]);
    struct Picture p = picture(ART(PlayoffSprite) + 0x12E0);
    uint16_t c = dobitmap(p.pal, p.map, p.tiles, 0, 0, p.w, p.h, 0, 0xF);
    printz(at[1], sizeof at[1]);
    p = picture(ART(HiScoreImg));
    dobitmap(p.pal, p.map, p.tiles, 0, 0, p.w, p.h, c, 0);
    g.palcount = 0x18;
    cpu_set_sr(0x2500);
    rp count = RAM(seed);                                 /* (the seed's high word counts down) */
    w16(count, 0, 0x50);
    do {
        uint16_t d0, d1, d2, d3;
        waitx(4, &d0, &d1, &d2, &d3);
        if (d1) break;
        w16(count, 0, (uint16_t)(r16(count, 0) - 1));
    } while ((int16_t)r16(count, 0) >= 0);
    cpu_set_sr(0x2700);
}

/* title94.asm:newTitleScreen. The title screen (TitleScreenImg, the NHL shield, the NHLPA logo,
 * the title) with its own vblank (VBlank_TitleScreen: a line scroll), the title song, then the
 * credits (Credits, then CreditsList) scrolling up a row at a time until they end or Start. */
void newTitleScreen(void)
{
    static const uint8_t font[8] = { 0x0D, 0x10, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    cpu_set_sr(0x2700);
    w16(RAM(seed), 0, vdp_hv16());
    w16(RAM(seed), 2, vdp_hv16());
    g.vbint = SYM_VBlank_TitleScreen;
    g.disflags |= HI(df32c);
    screen_layout();
    setVram(0);
    vdp_ctrl16(SYM_VDPR_WINX);
    vdp_ctrl16(SYM_VDPR_WINY + 0x17);
    vdp_ctrl16(SYM_VDPR_MODE3 + 3);                       /* line hscroll */
    g.Vscroll = 0;
    printz(str_newTitleScreen_1, sizeof str_newTitleScreen_1);
    eraser(0x80, 0x20, 0x7FF);
    printz(str_newTitleScreen_2, sizeof str_newTitleScreen_2);
    eraser(0x80, 0x20, 0x7FF);
    printz(str_newTitleScreen_3, sizeof str_newTitleScreen_3);
    struct Picture p = picture(ART(TitleScreenImg));
    g.TempWord1 = dobitmap(p.pal, p.map, p.tiles, 0, 0, p.w, 0x17, 0, 0xF);
    printz(str_newTitleScreen_4, sizeof str_newTitleScreen_4);   /* its last 5 rows on plane B */
    g.sflags6 |= HI(0);
    dobitmap(p.pal, p.map, p.tiles, 0, 0x17, p.w, 5, 0, 0);
    g.sflags6 &= (uint16_t)~HI(0);
    printz(str_newTitleScreen_5, sizeof str_newTitleScreen_5);
    p = picture(ART(NHLShieldImg));
    uint16_t c = dobitmap(p.pal, p.map, p.tiles, 0, 0, p.w, p.h, g.TempWord1, 8);
    printz(str_newTitleScreen_6, sizeof str_newTitleScreen_6);
    p = picture(ART(PAlogoImg));
    c = dobitmap(p.pal, p.map, p.tiles, 0, 0, p.w, p.h, c, 0);
    printz(str_newTitleScreen_7, sizeof str_newTitleScreen_7);
    p = picture(ART(TitleImg));
    c = dobitmap(p.pal, p.map, p.tiles, 0, 0, p.w, p.h, c, 8);
    g.palfadenew[0] = 0;
    g.fofdata2[0] = 0;
    w16(RAM(fofdata2), 4, 0);
    g.smallfontchars = c;
    DecompressGraphicsWithCallback(ART(SmallFontMap) + 8, c, rom_find(SYM_newTitleScreen, SYM_CreditsPrintRow, font, 8));
    printz(str_newTitleScreen_8, sizeof str_newTitleScreen_8);
    CreditsPrintRow(SYM_Credits);
    g.Vscroll += 0x20;
    g.clampcounter = 0x104;
    g.playoffspritex = 0x120;
    g.playoffspritey = 0xD0;
    g.asv = 0;
    g.DispAttribCtr = 0xFFCE;
    g.palcount = 0x20;
    song(SYM_SngTitle);
    CreditsLineScroll();
    cpu_set_sr(0x2500);
    do if (CreditsWait()) return; while (g.clampcounter != 0);
    for (int i = 0; i <= 0x3C; i++) if (CreditsWait()) return;
    printz(str_newTitleScreen_9, sizeof str_newTitleScreen_9);
    uint32_t a1 = SYM_CreditsList;
    do {
        a1 = CreditsPrintRow(a1);
        a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));       /* past the row's end */
        for (int i = 0; i <= 0x27; i++) {
            if (CreditsWait() || CreditsWait()) return;
            g.Vscroll++;
        }
        for (int i = 0; i <= 0x78; i++) if (CreditsWait()) return;
    } while ((int16_t)rd16(rom_ptr(a1 + 2)) >= 0);
}

/* title94.asm:CreditsPrintRow. Clear the six rows below the screen (Vscroll / 8 + $1C) and print
 * the Strings from a1 there, one a row, each centred, until one that starts with a control
 * code -> a1 at that one. */
uint32_t CreditsPrintRow(uint32_t a1)
{
    uint16_t row = (uint16_t)((((int16_t)g.Vscroll >> 3) + 0x1C) & 0x1F);
    g.printy = row;
    g.printx = 0;
    eraser(0x20, 6, 0x7FF);
    g.printy = row;
    do {
        g.printx = (uint16_t)(0x11 - ((int16_t)m16(a1) >> 1));
        a1 = print(a1);
        g.printy = (g.printy + 1) & 0x1F;
    } while ((int16_t)m16(a1 + 2) >= 0);
    return a1;
}

/* title94.asm:CreditsWait. Wait for the vblank, count clampcounter down by 2 (to 0) and read
 * the pads -> Start pressed (the 68k then returns from its caller too). */
int CreditsWait(void)
{
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    g.clampcounter -= 2;
    if ((int16_t)g.clampcounter < 0) g.clampcounter = 0;
    return (orjoy() & 0x80) != 0;
}

/* title94.asm:VBlank_TitleScreen. The title's vblank: unless dfng, the line scroll table (at
 * SortCords) and Vscroll, the sprites, the fade; then vcount, CreditsScrollStep and the music. */
void VBlank_TitleScreen(void)
{
    if (!(g.disflags & HI(dfng))) {
        DoDMA(RAM_BASE + RAM_OFF_SortCords, 0x1C0, g.VSCRLPM);
        vdp_ctrl(VSRAM_W);
        vdp_data(g.Vscroll);
        if (g.Sattsize) {
            uint16_t n = g.Sattsize;
            g.Sattsize = 0;
            DoDMA(RAM_BASE + RAM_OFF_Satt, n, g.VSPRITES);
        }
        cramfade();
    }
    g.vcount++;
    CreditsScrollStep();
    p_music_vblank();
}

/* title94.asm:CreditsLineScroll. The title's line scroll table at SortCords: $51 lines of
 * plane B from $FD80 (one less every other line), then $61 from $100 (4 more a line); the
 * step every $50 frames by $14. */
void CreditsLineScroll(void)
{
    g.Hscroll = 0x14;
    g.TempWord2 = 0x50;
    rp a0 = RAM(SortCords);
    uint32_t d1 = 0xFD80;
    for (int16_t d0 = 0x50; d0 >= 0; d0--, a0 = rp_add(a0, 4)) {
        w32(a0, 0, d1);
        if (!(d0 & 1)) d1 = (d1 & 0xFFFF0000) | (uint16_t)(d1 - 1);
    }
    d1 = 0x100;
    for (int i = 0; i <= 0x60; i++, a0 = rp_add(a0, 4), d1 += 4) w32(a0, 0, d1);
}

/* title94.asm:CreditsScrollStep. Every TempWord2 frames (then every frame) the lines of the
 * line scroll table move: plane B's of the first $51 by + Hscroll until they wrap to 0, the
 * next $66 by - Hscroll down to 0. */
void CreditsScrollStep(void)
{
    if ((int16_t)--g.TempWord2 >= 0) return;
    g.TempWord2 = 0;
    rp a0 = RAM(SortCords);
    for (int16_t d0 = 0xDF; d0 >= 0; d0--) {
        a0 = rp_add(a0, 4);
        if (d0 <= 0x28) continue;
        rp b = rp_add(a0, -2);
        if (d0 >= 0x8F) {
            uint16_t v = r16(b, 0);
            if (v == 0) continue;
            v = (uint16_t)(v + g.Hscroll);
            w16(b, 0, v);
            if ((v & 0xFC00) != 0xFC00) w16(b, 0, 0);
        } else {
            uint16_t v = (uint16_t)(r16(b, 0) - g.Hscroll);
            w16(b, 0, (int16_t)v < 0 ? 0 : v);
        }
    }
}

/* title94.asm:UnpackPicture. A picture a2 (a tile count, then 3 bytes a row of 8 pixels, 3 bits
 * each) to 4-bit tiles at picturebuf (the count first), each pixel + 5. */
void UnpackPicture(uint32_t pic)
{
    uint32_t in = pic;
    rp out = RAM(picturebuf);
    uint16_t n = m16(in);
    in += 2;
    w16(out, 0, n);
    out = rp_add(out, 2);
    uint16_t rows = (uint16_t)((uint16_t)(n << 3) - 1);
    do {
        uint32_t b = (uint32_t)m8(in) << 24 | m8(in + 1) << 16 | m8(in + 2) << 8, v = 0;
        in += 3;
        g.picturebits = b;
        for (int px = 0; px < 8; px++)                    /* pixel px: bits 31-3px .. 29-3px to nibble 7-px */
            v |= ((b & (0xE0000000u >> (3 * px))) >> (px + 1)) + (5u << (28 - 4 * px));
        w32(out, 0, v);
        out = rp_add(out, 4);
    } while (rows-- != 0);
}

/* title94.asm:DrawPlayoffSprite. The PlayoffSprite sprite at playoffspritex / playoffspritey in
 * Satt (an empty entry when it is off screen), the list ended there (Sattsize). */
void DrawPlayoffSprite(void)
{
    struct Vid v = { { 0 }, RAM(Satt), 1 };
    SetSframe(ART(PlayoffSprite), (int16_t)g.playoffspritex, (int16_t)g.playoffspritey, 1, (uint16_t)(g.energybarchars | 0x8000), &v);
    if (rp_same(v.sat, RAM(Satt))) { ram_fill(v.sat, 0, 8); v.sat = rp_add(v.sat, 8); }
    w8(v.sat, -5, 0);                                     /* the last sprite's link */
    g.Sattsize = (uint16_t)((uint16_t)(rp_diff(v.sat, RAM(Satt))) >> 1);
}

/* title94.asm:PlayedByHome. The text player's '#' -> a1 = "(played by NAME)" for homeuser
 * (PlayedByText), or an empty String. */
uint32_t PlayedByHome(void) { return PlayedByText(g.homeuser); }

/* title94.asm:PlayedByAway. The same for awayuser ('%'). */
uint32_t PlayedByAway(void) { return PlayedByText(g.awayuser); }

/* title94.asm:PlayedByText. User d0's name (GetLogName from the name log, as a String at
 * nameentrybuf) in "(played by NAME)" at TempBuffer -> a1; user 0: PlayedByNone. */
uint32_t PlayedByText(uint16_t user)
{
    if (user == 0) return PlayedByNone();
    g.namelogsel = user;
    GetLogName(RAM_BASE + RAM_OFF_nameentrybuf + 2);
    g.nameentrybuf[2 + (int16_t)g.namelength] = 0;
    g.namelength = (uint16_t)(((g.namelength + 1) & 0xFFFE) + 2);
    w16(RAM(nameentrybuf), 0, g.namelength);
    CopyString(SYM_PlayedByTxt, RAM_BASE + RAM_OFF_TempBuffer);
    appstring(RAM_BASE + RAM_OFF_nameentrybuf, RAM_BASE + RAM_OFF_TempBuffer);
    appendz(str_PlayedByText_1, sizeof str_PlayedByText_1, RAM_BASE + RAM_OFF_TempBuffer);
    return PlayedByExit(RAM_BASE + RAM_OFF_TempBuffer);
}

/* title94.asm:PlayedByNone. No user: the empty String. */
uint32_t PlayedByNone(void) { return PlayedByExit(SYM_PlayedByEmptyTxt); }

/* title94.asm:PlayedByExit. The common exit -> a1. */
uint32_t PlayedByExit(uint32_t a1) { return a1; }

/* title94.asm:AttribAdjust. An attribute under $32 becomes d0 / 2 + $19. */
uint16_t AttribAdjust(uint16_t v)
{
    if ((int16_t)v >= 0x32) return v;
    return (uint16_t)(((int16_t)v >> 1) + 0x19);
}

/* title94.asm:WriteLineData. databuffer back to save RAM at LineDataBase, the checksum; then as
 * ReadLineData after. */
void WriteLineData(void)
{
    WriteSRAM(SYM_LineDataBase, 0x100, RAM_BASE + RAM_OFF_databuffer);
    MakeSRAMChecksum();
    g.sflags &= (uint16_t)~HI(sfwrap);
    g.lastsfx = 0xFFFF;
    g.recbpr = RAM_BASE;
}

/* title94.asm:GetLeagueCrowdRecord. d0 = the team (of NumTeams, the last first) with the loudest
 * crowd record (byte 8 of ReadCrowdRecord's buffer at ThreeStars; 0 counts as $50). */
uint16_t GetLeagueCrowdRecord(void)
{
    uint16_t best = 0, team = 0;
    for (int16_t t = (int16_t)(league.teams - 1); t >= 0; t--) {
        ReadCrowdRecord((uint32_t)t, RAM_BASE + RAM_OFF_ThreeStars);
        uint16_t db = g.ThreeStars[8];
        if (db == 0) db = 0x50;
        if ((int16_t)db > (int16_t)best) {
            best = db;
            team = (uint16_t)t;
        }
    }
    return team;
}

/* title94.asm:ClearWinRecords. The win records (bytes 8-$B of the 8 records of $10 at ThreeStars)
 * cleared, the $80 bytes written back at TeamRecBase (WriteSRAM, MakeSRAMChecksum). */
void ClearWinRecords(void)
{
    for (int i = 0; i < 8; i++) ram_fill(rp_add(RAM(ThreeStars), i * 0x10 + 8), 0, 4);
    WriteSRAM(SYM_TeamRecBase, 0x80, RAM_BASE + RAM_OFF_ThreeStars);
    MakeSRAMChecksum();
}

/* title94.asm:GetTeamStruct. a2 = the home team struct, or the visitors' when team d2 is not its
 * team ($28 of the struct). */
struct Team *GetTeamStruct(uint16_t team)
{
    struct Team *tm = team_struct(0);
    return tm->team == team ? tm : team_struct(1);
}
