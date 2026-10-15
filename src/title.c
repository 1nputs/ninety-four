/* title94.asm: despite the name, mostly helpers of the 94 additions the game play calls: songs,
 * the shootout skate paths, one-timers, the hot / cold icons, board falls and the pad switch. */
#include "game.h"
#include "rom.h"
#include "symbols_gen.h"
#include "anim_gen.h"
#include "title_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))
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
        n = rom_ptr(SYM_TeamSongs)[(int16_t)(g.HmTeam * 6 + g.SongIndex)];
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
    uint16_t v = rd16(team_struct(o->pflags & (1 << pfteam)) + tmgoalie);
    return (uint8_t)((v == 0 ? CCR_Z : 0) | (v & 0x8000 ? CCR_N : 0));
}

/* title94.asm:ShootoutShootCheck. Shootout skater a3 with the puck -> Z: hold the shot (before
 * shootoutclock 4, before the path starts, then until the path end is near or the puck has
 * stopped after $F). Z clear = shoot now; also when it is not the shootout carrier. */
int ShootoutShootCheck(struct SortObj *o)
{
    if (!(g.gmode2 & HI(1))) return 0;
    if (o->SCnum != (int16_t)g.puckc) return 0;
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
    uint8_t *leader = team_struct(1);
    int16_t diff = (int16_t)(rd16(team_struct(0) + 0x24) - rd16(team_struct(1) + 0x24));
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
    uint8_t *cont = RAM_W(RAM_OFF_cont1team), *cur = RAM_W(RAM_OFF_c1playernum);
    struct SortObj *pl = sortobj(rd16(cont + pad) == 1 ? 0 : 6);
    uint16_t mine = rd16(cur + pad), other = rd16(cur + (pad ^ 2));
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
    uint8_t *cnt = RAM_W(pad ? RAM_OFF_awaypresses : RAM_OFF_homepresses);
    uint16_t dir = g.TempWord1;
    wr32(cnt, rd32(cnt) + 1);
    if (dir == 8 || dir == (uint16_t)o->facedir) return;
    wr32(cnt + 4, rd32(cnt + 4) + 1);
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
    uint32_t gfx = rd32(rom_ptr(SYM_TeamGfxList + (int16_t)(g.HomeTeam << 2))) + 8;
    return DecompressGraphicsPlain(gfx, (uint16_t)(start - 0x18));
}

/* title94.asm:HotColdIcon. Team a2: the icon by the name of iconplayer (x 2 away, $20 home,
 * y $19): the hot icon for the team's hot player, the cold one for its cold player, else clear
 * it. */
void HotColdIcon(uint8_t *tm)
{
    int home = tm == team_struct(0);
    g.iconx = home ? 0x20 : 2;
    uint32_t icon;
    uint16_t chars;
    if ((home ? g.homehotplayer : g.awayhotplayer) == g.iconplayer) {
        chars = g.hoticonchars; icon = SYM_HotIconMap;
    } else if ((home ? g.homecoldplayer : g.awaycoldplayer) == g.iconplayer) {
        chars = g.coldiconchars; icon = SYM_ColdIconMap;
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
void clrTmPdst(uint8_t *tm)
{
    uint8_t *p = tm + tmpdst;
    for (int i = 0; i < 26; i++, p += 2) wr16(p, 0xFFFE);
    wr16(p, 0xFFFF);
}

/* title94.asm:ClearPenalties. No penalties: the counters, PenBuf and both teams' slots. */
void ClearPenalties(void)
{
    g.PBnum = 0;
    g.Penaltytimer = 0;
    g.Pencntdwn = 0;
    for (int i = 0; i < 0x44; i++) RAM_W(RAM_OFF_PenBuf)[i] = 0;    /* 17 longs */
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
void PrintPlayerGoals(uint16_t player, uint8_t *tm)
{
    printz(str_PrintPlayerGoals_1, sizeof str_PrintPlayerGoals_1);
    PrintParenNumber(tm + 0xB4, player);
}

/* title94.asm:PrintPlayerAssists. d0 = player, a2 = team: " (assists)", next row. */
void PrintPlayerAssists(uint16_t player, uint8_t *tm)
{
    printz(str_PrintPlayerAssists_1, sizeof str_PrintPlayerAssists_1);
    PrintParenNumber(tm + 0xCE, player);
}
