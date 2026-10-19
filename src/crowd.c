/* crowd94.asm: the crowd and hot players. Runs the crowd meter, with the arena record and the
 * crowd noise, checks the corners of the wall, and keeps the tables of which players are hot
 * or cold that the player ratings and the matchups screen use. */
#include "game.h"
#include "rom.h"
#include "symbols_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
#define tmhotcold 0x1A2   /* the hot / cold table: 16 bytes per roster player */

/* crowd94.asm:LoadCrowdRec. CrowdRecord = the arena record of HomeTeam from save RAM, $50
 * when none. */
void LoadCrowdRec(void)
{
    ReadCrowdRecord((uint32_t)(int32_t)(int16_t)g.HomeTeam, ram_addr(g.ThreeStars));
    uint16_t d0 = g.ThreeStars[8];
    if (d0 == 0) d0 = 0x50;
    g.CrowdRecord = d0;
}

/* crowd94.asm:Crowd_Noise. The crowd meter each frame: crowd noise on / off from the
 * excitement against the record, and every $14 ticks the meter reading and its peak. */
void Crowd_Noise(void)
{
    if (g.sflags & HI(sfhor)) return;
    if (g.gmode2 & HI(0)) return;
    if (g.sflags3 & (1 << sf3llcs)) return;
    if (!(g.sflags5 & 0x80)) {
        if (g.crowdnoisedelay != 0) {
            g.crowdnoisedelay--;
            if ((int16_t)g.crowdnoisedelay < 0) g.crowdnoisedelay = 0;
            return;
        }
        int16_t d0 = (int16_t)g.CrowdRecord;              /* the level that turns the noise on */
        g.CrowdAvg = (uint16_t)(d0 - 0xB);
        d0 = (int16_t)(d0 - 0x49);
        int32_t sq = ((int32_t)d0 * d0) >> 2;
        d0 = (int16_t)((int16_t)sq + 6);
        if (d0 > (int16_t)g.CwdExciteLvl) return;
        g.sflags5 |= 0x80;
        return;
    }
    int16_t d0 = (int16_t)(g.CrowdAvg - 0x41);            /* the level that turns it off */
    int32_t sq = ((int32_t)d0 * d0) >> 2;
    if (!((int16_t)sq < (int16_t)g.CwdExciteLvl)) g.sflags5 &= (uint8_t)~0x80;
    g.CwdChkCntr = (uint16_t)(g.CwdChkCntr - elapsed);
    if ((int16_t)g.CwdChkCntr >= 0) return;
    g.CwdChkCntr = 0x14;
    g.CwdChkCnt[0]++;
    int32_t lv = (int16_t)g.CwdExciteLvl;
    lv = (int32_t)((lv & 0xFFFF0000u) | (uint16_t)(lv << 2));
    uint16_t m = (uint16_t)(sroot((uint32_t)lv) + 0x41);  /* decibels */
    g.CurCrowdMeter = m;
    if ((int16_t)m > (int16_t)g.CrowdPeak) g.CrowdPeak = m;
}

/* crowd94.asm:stopna2. Slow the velocity of a3 toward 0 by $7D0. */
void stopna2(struct SortObj *o)
{
    if (o->Xvel < 0) {
        o->Xvel = (int16_t)(o->Xvel + 0x7D0);
        if (o->Xvel >= 0) o->Xvel = 0;
    } else {
        o->Xvel = (int16_t)(o->Xvel - 0x7D0);
        if (o->Xvel < 0) o->Xvel = 0;
    }
    if (o->Yvel < 0) {
        o->Yvel = (int16_t)(o->Yvel + 0x7D0);
        if (o->Yvel >= 0) o->Yvel = 0;
    } else {
        o->Yvel = (int16_t)(o->Yvel - 0x7D0);
        if (o->Yvel < 0) o->Yvel = 0;
    }
}

/* crowd94.asm:checkcornercoll. The corner circles (radius 64) and the side walls for object
 * a3 at d2 / d3 with wcradiusx / wcradiusy; a hit goes to wallcollb. */
void checkcornercoll(int16_t x, int16_t y, struct SortObj *o)
{
    o->pflags3 &= (uint8_t)~(1 << pf3wall);
    int16_t d4 = (int16_t)(0x88 - (int16_t)g.wcradiusx);
    int16_t d5 = (int16_t)(0x12A - (int16_t)g.wcradiusy);
    int16_t cx = (int16_t)(-d4 + 0x40), cy = (int16_t)(-d5 + 0x40);   /* the corner circle centers (one end, left) */
    int incorner = 0, check = 1;
    if (y > cy) {                                         /* the other end's circles, unless between the ends */
        cy = (int16_t)-cy;
        check = y >= cy;
    }
    if (check) {
        if (x < cx) incorner = 1;
        else if (x > (int16_t)-cx) { cx = (int16_t)-cx; incorner = 1; }   /* the right-hand circle */
    }
    if (incorner) {
        int16_t dx = (int16_t)(x - cx), dy = (int16_t)(y - cy);
        int16_t c = dy, s = (int16_t)-dx;
        int32_t dist = (int32_t)dy * dy + (int32_t)dx * dx;
        if ((uint32_t)dist > 0x1000) {
            int16_t r = (int16_t)sroot((uint32_t)dist);
            c = (int16_t)m68k_divs((int32_t)c << 8, r);
            s = (int16_t)m68k_divs((int32_t)s << 8, r);
            wallcollb(c, s, o);
        }
    }
    if ((uint16_t)(o->Wallcos | o->Wallsin) != 0) return;
    int16_t d0 = 0x100, d1 = 0;                           /* now the side walls */
    if (y >= d5) { wallcollb(d0, d1, o); return; }
    d5 = (int16_t)-d5;
    d0 = (int16_t)-d0;
    if (y <= d5) { wallcollb(d0, d1, o); return; }
    d1 = d0; d0 = 0;
    if (x >= d4) { wallcollb(d0, d1, o); return; }
    d4 = (int16_t)-d4;
    d1 = (int16_t)-d1;
    if (x <= d4) wallcollb(d0, d1, o);
}

/* crowd94.asm:HotColdLoop. The loop of Create_HotCold_Table from entry d1 down. */
void HotColdLoop(struct Team *tm, int16_t d1)
{
    uint8_t *hc = tm->hotcold[0];                         /* (the table, row after row) */
    for (; d1 >= 0; d1--)
        hc[d1] = (uint8_t)randomd0s(9);
}

/* crowd94.asm:Create_HotCold_Table. Fill the hot / cold table of team a0 with 416 random
 * values (-9 ... 8). */
void Create_HotCold_Table(struct Team *tm)
{
    HotColdLoop(tm, 0x19F);
}

/* crowd94.asm:Clamp0to100. Unused. */
uint16_t Clamp0to100(int16_t v)
{
    if (v < 0) return 0;
    if (v > 0x64) return 0x64;
    return (uint16_t)v;
}

/* crowd94.asm:AttributeCalc. Attribute d3 of player a3 * 5 plus his hot / cold value / 3,
 * limited to 0 ... $1E. */
uint8_t AttributeCalc(uint8_t v, struct SortObj *o)
{
    struct Team *tm = team_struct(PFLAG(o, pfteam));
    int32_t hc = (int8_t)tm->hotcold[o->pnum][0];
    hc = (int16_t)m68k_divs(hc, 3);
    hc = rule_scale(RULE_STREAKS, hc);                    /* (rules: the streaks' effect) */
    int16_t d3 = (int16_t)(v * 5 + hc);
    if (d3 < 0) d3 = 0;
    else if (d3 >= 0x1E) d3 = 0x1E;
    return (uint8_t)d3;
}

/* crowd94.asm:NextHomeHotPlayer etc. The next hot / cold player of a team for the matchups
 * text (the one entry lists at homehotplayer ...), with its team struct. */
static uint16_t listed(uint16_t list, uint16_t idx) { return m16(RAM_BASE + list + idx * 2); }
uint16_t NextHomeHotPlayer(struct Team **team)
{
    *team = team_struct(0);
    uint16_t d1 = listed(RAM_OFF_homehotplayer, g.homehotidx);
    if ((int16_t)g.homehotidx < 0) g.homehotidx++;       /* (only below 0: it stays at the first) */
    return d1;
}
uint16_t NextAwayHotPlayer(struct Team **team)
{
    *team = team_struct(1);
    uint16_t d1 = listed(RAM_OFF_awayhotplayer, g.awayhotidx);
    if ((int16_t)g.awayhotidx < 0) g.awayhotidx++;
    return d1;
}
uint16_t NextHomeColdPlayer(struct Team **team)
{
    *team = team_struct(0);
    uint16_t d1 = listed(RAM_OFF_homecoldplayer, g.homecoldidx);
    if ((int16_t)g.homecoldidx < 0) g.homecoldidx++;
    return d1;
}
uint16_t NextAwayColdPlayer(struct Team **team)
{
    *team = team_struct(1);
    uint16_t d1 = listed(RAM_OFF_awaycoldplayer, g.awaycoldidx);
    if ((int16_t)g.awaycoldidx < 0) g.awaycoldidx++;
    return d1;
}

/* crowd94.asm:BuildHotColdLists. The hot / cold player lists of both teams. */
void BuildHotColdLists(void)
{
    g.awayhotidx = 0;
    g.homehotidx = 0;
    SortHotColdStarters(team_struct(0));
    g.homehotplayer = CopyHottestPlayer();
    g.homecoldplayer = CopyColdestPlayer();
    SortHotColdStarters(team_struct(1));
    g.awayhotplayer = CopyHottestPlayer();
    g.awaycoldplayer = CopyColdestPlayer();
}

/* crowd94.asm:SortHotColdStarters. Sum the hot / cold values of the 6 starters of team a0 into
 * TempBuffer (byte pairs: player, sum), then sort them by sum. */
void SortHotColdStarters(struct Team *tm)
{
    const uint8_t *hc = tm->hotcold[0];                   /* (the table, row after row) */
    const uint8_t *a0 = rom_ptr(tm->data);
    a0 += rd16(a0 + 6);                                   /* the starters */
    uint8_t *a1 = g.TempBuffer;
    for (int i = 0; i < 6; i++) {
        uint8_t d1 = (uint8_t)(*a0++ - 1);
        *a1++ = d1;
        int16_t idx = (int16_t)((int8_t)d1 << 4);
        a1[0] = 0;
        idx = (int16_t)(idx + 3);
        for (int d7 = 3; d7 != 0x10; d7++, idx++) {
            if (d7 == 9 || d7 == 0xD) continue;
            a1[0] = (uint8_t)(a1[0] + hc[idx]);
        }
        a1 += 1;
    }
    int swapped;
    do {
        swapped = 0;
        a1 = g.TempBuffer;
        for (int i = 0; i < 5; i++, a1 += 2) {
            if ((int8_t)a1[3] <= (int8_t)a1[1]) continue;
            swapped = 1;
            uint8_t t0 = a1[0], t1 = a1[1];
            a1[0] = a1[2]; a1[1] = a1[3];
            a1[2] = t0; a1[3] = t1;
        }
    } while (swapped);
}

/* crowd94.asm:CopyHottestPlayer / CopyColdestPlayer. The hottest (first) or coldest (sixth)
 * player of TempBuffer, for the one word list at a0. */
uint16_t CopyHottestPlayer(void) { return g.TempBuffer[0]; }
uint16_t CopyColdestPlayer(void) { return g.TempBuffer[10]; }

/* crowd94.asm:CompareHotColdTotals. awayhotter = 1 when the away total is higher; d0 = -1, $22
 * or $23 by the difference. */
uint16_t CompareHotColdTotals(void)
{
    g.TempWord1 = GetHotColdTotal(team_struct(0));
    g.TempWord2 = GetHotColdTotal(team_struct(1));
    int16_t d2 = (int16_t)(g.TempWord2 - g.TempWord1);
    g.awayhotter = d2 < 0 ? 0 : 1;
    if (d2 < 0) d2 = (int16_t)-d2;
    uint16_t d0 = 0xFFFF;
    if (d2 >= 0x5E) d0 = 0x22;
    if (d2 >= 0xBD) d0 = 0x23;
    return d0;
}

/* crowd94.asm:GetHotColdTotal. The hot / cold total of the starters of team a0. */
uint16_t GetHotColdTotal(struct Team *tm)
{
    SortHotColdStarters(tm);
    int16_t d1 = 0;
    for (int i = 0; i < 7; i++)
        d1 = (int16_t)(d1 + (int8_t)g.TempBuffer[i * 2 + 1]);
    return (uint16_t)d1;
}
