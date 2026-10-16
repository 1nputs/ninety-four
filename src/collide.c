/* collide94.asm: collisions. Bounces players off the walls and each other, runs body checks,
 * holds and hooks, and decides when a player falls or is hurt. */
#include "game.h"
#include "anim_gen.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "collide_tables_gen.h"
#include <string.h>

/* collide94.asm:checkcoll. d2/d3 = new x/y, a3 = object. Wall collision around the object and
 * around the end of the stick, then player collisions; then the object's slot in the on-screen
 * order (OOlist, kept sorted by y) is moved. On a player collision the old position is
 * restored instead. Called from updateplayers. */
void checkcoll(int16_t x, int16_t y, struct SortObj *o)
{
    g.collflag = 0;
    if (g.sflags & HI(sfhor)) { int16_t t = x; x = y; y = t; }
    if (!(o->pflags & (1 << pfnc))) {
        g.wcradiusx = o->radiusx;
        g.wcradiusy = o->radiusy;
        checkwallcoll(o->Xpos, o->Ypos, o);
        if ((uint16_t)(o->Wallcos | o->Wallsin) == 0 && IS_PLAYER(o)) {
            int16_t hx, hy;
            GetHot(o, &hx, &hy);
            g.wcradiusx = 1;
            g.wcradiusy = 1;
            checkwallcoll((int16_t)(o->Xpos + hx), (int16_t)(o->Ypos + hy), o);
        }
        checkplcoll(x, y, o);
    }
    if (g.collflag) {
        o->Xpos = o->OldXpos;
        o->Ypos = o->OldYpos;
        return;
    }
    /* OOlist holds object numbers * 2 in screen order (index 15 = top); OOlistpos[n] is where
     * object n sits in it; Ylist[n] is its y. Bubble this object up past lower-y objects, then
     * down past higher-y ones. */
    unsigned n = (uint16_t)o->SCnum;
    uint16_t pos = g.OOlistpos[n];
    while (pos != 0xF) {
        unsigned nxt = g.OOlist[pos + 1];
        if (y <= (int16_t)g.Ylist[nxt / 2]) break;
        g.OOlistpos[n]++;
        g.OOlistpos[nxt / 2]--;
        g.OOlist[pos] = (uint8_t)nxt;
        g.OOlist[pos + 1] = (uint8_t)(n * 2);
        pos++;
    }
    pos = g.OOlistpos[n];
    while (pos != 0) {
        unsigned nxt = g.OOlist[pos - 1];
        if (y >= (int16_t)g.Ylist[nxt / 2]) break;
        g.OOlistpos[n]--;
        g.OOlistpos[nxt / 2]++;
        g.OOlist[pos] = (uint8_t)nxt;
        g.OOlist[pos - 1] = (uint8_t)(n * 2);
        pos--;
    }
    g.Ylist[n] = (uint16_t)y;
}

/* collide94.asm:checkplcoll. d2/d3 = x/y, a3 = object. Walks the screen order up and down from
 * the object and tries checkcx on every object within $10 in y. Called from checkcoll. */
void checkplcoll(int16_t x, int16_t y, struct SortObj *o)
{
    if (o->pflags2 & (1 << 5))
        return;
    if (!IS_PLAYER(o))
        return;
    unsigned n = (uint16_t)o->SCnum;
    uint16_t pos = g.OOlistpos[n];
    while (pos != 0xF) {
        unsigned nxt = g.OOlist[pos + 1];
        int16_t dy = (int16_t)(g.Ylist[nxt / 2] - y);
        if (dy > 0x10)
            break;
        checkcx(nxt, x, dy, o);
        pos++;
    }
    pos = g.OOlistpos[n];
    while (pos != 0) {
        unsigned nxt = g.OOlist[pos - 1];
        int16_t dy = (int16_t)(y - g.Ylist[nxt / 2]);
        if (dy > 0x10)
            return;
        checkcx(nxt, x, dy, o);
        pos--;
    }
}

/* collide94.asm:checkcx. d4 = other object number * 2, d2 = x, d5 = delta y, a3 = moving object.
 * Within the collision radius: opposing players add impact and run the check / interference
 * logic, then momentum moves from a3 to a2 (elastic collision with masses weight + $8C and
 * elasticity evalue/16). Called from checkplcoll. */
void checkcx(unsigned obj2, int16_t x, int16_t dy, struct SortObj *o)
{
    struct SortObj *p = sortobj((int)(obj2 / 2));
    if (p->pflags & (1 << pfnc))
        return;
    if (p->pflags2 & (1 << 5))
        return;
    if (!IS_PLAYER(p))
        return;
    int16_t dx = (g.sflags & HI(sfhor)) ? p->Ypos : p->Xpos;
    dx = (int16_t)(dx - x);
    if (dx < -0x10 || dx > 0x10)
        return;
    if ((int32_t)dy * dy + (int32_t)dx * dx > 0x100)
        return;
    g.evalue = 0;
    uint8_t teams = (uint8_t)(o->pflags ^ p->pflags);
    int16_t vx = (int16_t)(o->Xvel - p->Xvel);
    int16_t vy = (int16_t)(o->Yvel - p->Yvel);
    int16_t Dx = (int16_t)(p->Xpos - o->Xpos);
    int16_t Dy = (int16_t)(p->Ypos - o->Ypos);
    int32_t v1n = (int32_t)vy * Dy + (int32_t)vx * Dx;   /* closing speed along the line of centres */
    if (v1n < 0)
        return;
    v1n >>= 4;
    if (teams & (1 << pfteam)) {
        uint16_t imp = (uint16_t)v1n >> 8;
        if ((int16_t)imp <= 5)
            imp = 5;                                      /* minimum impact value */
        o->impact = (int16_t)(o->impact + imp);
        p->impact = (int16_t)(p->impact + imp);
        if (!(p->pflags2 & (1 << pf2fight)))
            p->impactp = o->SCnum;
        if (!(o->pflags2 & (1 << pf2fight)))
            o->impactp = p->SCnum;
        if ((int16_t)imp >= 0x14 && (g.puckc == (uint16_t)o->SCnum || g.puckc == (uint16_t)p->SCnum))
            newcheck();
        checkint(p, o);
        checkcheck(p, o, (int16_t)imp);
        /* checkfight: an rts */
    }
    int16_t v1nw = (int16_t)v1n;
    if ((int16_t)g.collflag < 0)
        return;
    int16_t v1t = (int16_t)(((int32_t)vx * Dy - (int32_t)vy * Dx) >> 4);
    uint16_t m1 = (uint16_t)(o->weight + 0x8C);
    uint16_t m2 = (uint16_t)(p->weight + 0x8C);
    uint16_t m2e = (uint16_t)((g.evalue * m2) >> 4);
    uint16_t msum = (uint16_t)(m1 + m2);
    int16_t v1n2 = (int16_t)m68k_divs((int32_t)v1nw * (int16_t)(m1 - m2e), (int16_t)msum);
    int16_t v2n = (int16_t)(((int16_t)(uint16_t)((int32_t)g.evalue * v1nw) >> 4) + v1n2);
    o->Xvel = (int16_t)((((int32_t)v1t * Dy + (int32_t)v1n2 * Dx) >> 4) + p->Xvel);
    o->Yvel = (int16_t)((((int32_t)v1n2 * Dy - (int32_t)v1t * Dx) >> 4) + p->Yvel);
    p->Xvel = (int16_t)(p->Xvel + (((int32_t)v2n * Dx) >> 4));
    p->Yvel = (int16_t)(p->Yvel + (((int32_t)v2n * Dy) >> 4));
    ST_W(g.collflag);
}

/* collide94.asm:newcheck. Start a new check sound: one of 4 sounds $1C-$1F, never the last one
 * again. Called from checkcx and FallDown. */
void newcheck(void)
{
    uint16_t n = (uint16_t)((randomd0(3) + 1 + g.ltack) & 3);
    g.ltack = n;
    sfx((int16_t)(n + 0x1C));
}

/* collide94.asm:checkint (.ci). a2 = goalie, a3 = player interfering with him: a3 falls, and an
 * interference penalty ($22) needs a3 in the crease area, a CPU player or a pad player's own
 * skater, and a low aggression roll. */
static void ci(struct SortObj *a2, struct SortObj *a3)
{
    if (a2->position != 0)
        return;
    if (g.gmode & (1 << gmclock))
        return;
    if (g.puckc != (uint16_t)a3->SCnum && a3->impact <= 0x19)
        return;
    if (a3->impact <= 2)
        return;
    FallDown(a3, a2);
    if (a2->impact <= 0x1E)
        return;
    if (a3->pflags2 & (1 << 4))                           /* already committed a penalty */
        return;
    if (a2->position != 0)
        return;
    uint16_t team = (a3->pflags & (1 << pfteam)) ? 2 : 1;
    if ((team == g.cont1team || team == g.cont2team) && !(a3->pflags & (1 << pfjoycon)))
        return;
    int16_t y = a2->Ypos;
    if (a2->pflags & (1 << pfgoal))
        y = (int16_t)-y;
    if (y < 0xEF || y > 0x10A)
        return;
    if (a2->Xpos > 0x14 || a2->Xpos < -0x14)
        return;
    if (randomd0((uint8_t)(0x28 - a3->aggress)) > 4)
        return;
    if (g.gmode & (1 << gmhl))
        return;
    AddPenalty(0x22, a3);
}

/* collide94.asm:checkint. Interference, each way round. Called from checkcx. */
void checkint(struct SortObj *a2, struct SortObj *a3)
{
    ci(a2, a3);
    ci(a3, a2);
}

/* collide94.asm:checkcheck. Players are in contact: CCStart for a3 on a2, then a2 on a3. */
void checkcheck(struct SortObj *a2, struct SortObj *a3, int16_t impact)
{
    CCStart(a2, a3, impact);
    CCStart(a3, a2, impact);
}

/* collide94.asm:CCStart. Player a3 is checking player a2 (d4 = impact). Holds go to holdcheck,
 * a sweep check to Bcheck; otherwise only a bursting a3 checks: he gets the check animation
 * for his direction, and a big enough hit on a skater makes a2 fall, with a charging or
 * roughing penalty roll. */
void CCStart(struct SortObj *a2, struct SortObj *a3, int16_t impact)
{
    if (a2->SPA == SPAHold || a2->SPA == SPAhook) { holdcheck(a2, a3); return; }
    if (a2->SPA == SPAsweepchk) { Bcheck(a2, a3); return; }
    if (a3->SPA != SPAburst)
        return;
    if (g.gmode & (1 << gmclock)) {                       /* more fights after the whistle */
        a3->impact = 0x100;
        a2->impact = 0x100;
    }
    uint16_t dir = (uint16_t)((vtoa((int16_t)(a2->Xpos - a3->Xpos), (int16_t)(a2->Ypos - a3->Ypos)) - a3->facedir) & 7);
    if (a3->attribute & HI(3))
        dir = (uint16_t)((8 - dir) & 7);
    a3->pflags |= 1 << pfalock;
    SetSPA(a3, checkinglist[dir]);
    if (a2->position == 0)
        return;
    if (impact < 0x14)
        return;
    uint16_t d0 = 0x78;
    if (a3->pflags & (1 << pfjoycon))
        d0 <<= 1;
    d0 = (uint16_t)((d0 & 0xFF00) | (uint8_t)((uint8_t)d0 - a3->weight + a2->weight));
    d0 >>= 1;
    wallcollduringcheck(a3, a2);
    d0 = (uint16_t)(d0 - a2->impact);
    if ((int16_t)d0 <= 0 || (a3->pflags3 & (1 << pf3wall)))
        goto down;
    {
        uint16_t r = randomd0(d0);
        g.TempWord1 = a3->checking;
        g.TempWord1 >>= 1;
        if ((int8_t)r <= (int8_t)g.TempWord1)
            goto down;
        if (checkagr(a2, a3) > 4)
            return;
        if (g.gmode & (1 << gmhl))
            return;
        uint16_t pen = (uint16_t)((vdp_hv8() & 2) + 0x16);  /* PenCharging, two variants */
        AddPenalty(PenShotChk(pen, a2, a3), a3);
        return;
    }
down:
    if (checkagr(a2, a3) <= 3 && !(g.gmode & (1 << gmhl))) {
        uint16_t pen = (uint16_t)((vdp_hv8() & 2) + 0x1A);  /* PenRoughing2, two variants */
        AddPenalty(PenShotChk(pen, a2, a3), a3);
    }
    FallDown(a2, a3);
}

/* collide94.asm:checkagr. a3 = checking player, a2 = player being checked. A penalty roll from
 * a3's aggression: randomd0(((40 - Agr) / 2) * 13), doubled for a pad player, halved near the
 * puck; a2 on a breakaway rolls again by his distance from the goal line; $7F (no penalty)
 * while the other team already has a delayed penalty. Callers call a penalty when it is small. */
uint16_t checkagr(struct SortObj *a2, struct SortObj *a3)
{
    const struct SortObj *puck = sortobj(puckscnum);
    uint16_t range = (uint16_t)(((uint8_t)(0x28 - a3->aggress) >> 1) * 0xD);
    if (a3->pflags & (1 << pfjoycon))
        range <<= 1;
    int16_t d = (int16_t)(a3->Xpos - puck->Xpos);
    if (d <= 0x28 && d >= -0x28) {
        d = (int16_t)(a3->Ypos - puck->Ypos);
        if (d <= 0x28 && d >= -0x28)
            range = (uint16_t)((int16_t)range >> 1);
    }
    uint16_t r = randomd0(range);
    if (a2->pflags3 & (1 << pf3break)) {
        int16_t y = a2->Ypos;
        if (y < 0)
            y = (int16_t)-y;
        y = (int16_t)(0x108 - y);                          /* distance from the goal line */
        uint16_t n = 8;
        if (y <= 0x75) { n = 7; if (y <= 0x3A) { n = 4; if (y <= 0x2C) n = 3; } }
        r = randomd0(n);
    }
    if (a3->pflags & (1 << pfteam)) {
        if (g.DelayedPen & (1 << 1)) r = 0x7F;            /* home team has a delayed penalty */
    } else {
        if (g.DelayedPen & (1 << 2)) r = 0x7F;            /* visitors have a delayed penalty */
    }
    return r;
}

/* collide94.asm:holdcheck. a2 is in a hold animation looking to hold a3, who must be within one
 * direction of where a2 faces. Both slow to their mean velocity; a3 flails, a2 goes to the
 * holding animation, and a penalty roll may call holding ($24) or hooking ($1E). */
void holdcheck(struct SortObj *a2, struct SortObj *a3)
{
    if (a3->pflags & (1 << pfalock))
        return;
    if (a3->position == 0)
        return;
    if (a3->pflags2 & (1 << pf2fight))
        return;
    uint16_t d = (uint16_t)((vtoa((int16_t)(a3->Xpos - a2->Xpos), (int16_t)(a3->Ypos - a2->Ypos)) - a2->facedir + 1) & 7);
    if (d > 2)
        return;
    if (g.puckc == (uint16_t)a3->SCnum)
        g.sflags &= (uint16_t)~HI(sfssdir);                 /* a3 has the puck */
    int16_t v = (int16_t)((int16_t)(a3->Xvel + a2->Xvel) >> 1);
    a3->Xvel = v; a2->Xvel = v;
    v = (int16_t)((int16_t)(a3->Yvel + a2->Yvel) >> 1);
    a3->Yvel = v; a2->Yvel = v;
    a3->pflags |= 1 << pfalock;
    SetSPA(a3, SPAflail);
    a2->pflags |= 1 << pfalock;
    SetSPA(a2, a2->SPA == SPAHold ? SPAHold2 : SPAhook2);
    if (checkagr(a3, a2) <= 6 && !(g.gmode & (1 << gmhl))) {
        uint16_t pen = (a2->SPA == SPAHold2) ? 0x24 : 0x1E;
        AddPenalty(PenShotChk(pen, a3, a2), a2);
    }
    ST_W(g.collflag);
}

/* collide94.asm:Bcheck. a2 is sweep checking, a3 is the player being checked. A pad player with
 * penalties off needs a roll against his checking and the victim's leg strength; if a3 is
 * within one direction of where a2 faces, a3 falls and tripping ($20) may be called. */
void Bcheck(struct SortObj *a2, struct SortObj *a3)
{
    if (a3->pflags & (1 << pfalock))
        return;
    if (a3->position == 0)
        return;
    if (a3->pflags2 & (1 << pf2fight))
        return;
    if (g.OptPen == 0 && (a2->pflags & (1 << pfjoycon))) {
        uint8_t range = (uint8_t)(0x20 + a2->checking - a3->legstr);
        if ((g.sflags8 & (1 << 1)) || (g.sflags7 & (1 << 6)))
            range -= 2;
        if ((int16_t)randomd0(range) < 0x18)
            return;
    }
    uint16_t d = (uint16_t)((vtoa((int16_t)(a3->Xpos - a2->Xpos), (int16_t)(a3->Ypos - a2->Ypos)) - a2->facedir + 1) & 7);
    if (d > 2)
        return;
    FallDown(a3, a2);
    if (checkagr(a3, a2) <= 4)
        AddPenalty(PenShotChk(0x20, a3, a2), a2);
    ST_W(g.collflag);
}

/* collide94.asm:FallDown. Player a2 falls down, player a3 is the hitting player. Skips a2 in
 * some anims, or the same pair as the last call. A skater hitter adds check stats. A hit into
 * the wall picks a fall anim by where a2 is; a strong Stk player may just stumble. A fall back
 * on the puck carrier can injure him. Then the crowd and a check sound. */
void FallDown(struct SortObj *a2, struct SortObj *a3)
{
    if (!IS_PLAYER(a2))
        return;
    uint16_t s = a2->SPA;
    if (s == SPAinjuryfall || s == SPAinjury1 || s == SPAstumble || s == SPAfallfwd || s == SPAfallback || s == SPAflip)
        return;
    if (g.PlayerChked == ram_addr(a2) && g.PlayerChking == ram_addr(a3))
        return;
    g.PlayerChked = ram_addr(a2);
    g.PlayerChking = ram_addr(a3);
    if (a2->pflags3 & (1 << pf3onetimer))
        return;
    if (a3->pflags3 & (1 << pf3onetimer))
        return;
    uint16_t anim = SPAstumble;
    uint16_t dir;
    if (!(a3->pflags3 & (1 << pf3wall)) && (int8_t)a2->stickhand >= 0x18 && a2->nopuck[1] == 0) {
        int16_t d0 = (int16_t)((vdp_hv16() & 0xF) + 0x10);
        if (d0 > a2->impact) {                             /* a strong stick hand: just a stumble */
            a2->nopuck[1] = 0x3C;
            goto hitter;
        }
    }
    if (a3->position != 0) {                               /* a skater hit: check stats */
        uint8_t *tm = team_struct(a3->pflags & (1 << pfteam));
        wr16(tm + tmchecks, (uint16_t)(rd16(tm + tmchecks) + 1));
        tm[tmpchecks + a3->pnum]++;
        g.ChkCnt++;
        if (a3->fighting == 0)
            g.ChkCnt += 2;
    }
    a2->nopuck[0] = 0x78;
    dir = vtoa((int16_t)(a3->Xpos - a2->Xpos), (int16_t)(a3->Ypos - a2->Ypos));
    if (a2->position != 0) {
        wallcollduringcheck(a2, a3);
        if (a2->pflags3 & (1 << pf3wall)) {
            int16_t dx = (int16_t)(a2->Xpos - a3->Xpos);
            int16_t dy = (int16_t)(a2->Ypos - a3->Ypos);
            int toward;                                    /* was a2 hit toward centre ice? */
            if (a3->Ypos > 0x108) toward = dy < 0;
            else if (a3->Ypos < -0x108) toward = dy >= 0;
            else if (a3->Xpos >= 0) toward = dx < 3;
            else toward = dx > -3;
            if (toward)
                goto seven;
            if (!(a2->Ypos > 0x10C || a2->Ypos < -0x10C || a2->Xpos > 0x68 || a2->Xpos < -0x68))
                goto seven;                                /* still in the middle of the rink */
            if (IS_PLAYER(a3)) {
                g.CwdExciteLvl += 0xA;
                g.crowdlevel += 0x96;
                a3->temp1 = (int16_t)(a3->temp1 & 0x00FF);
                a3->glitch = 0;
                SetSPA(a3, SPAcrosscheck);
                a3->Xvel = 0;
                a3->Yvel = 0;
            }
            unsigned idx = (a2->Ypos > 0x10C) ? 0 : (a2->Ypos < -0x10C) ? 4 : (a2->Xpos < 0) ? 6 : 2;
            anim = FallList[idx];
            g.FallYPos = (uint16_t)a2->Ypos;
            g.FallXPos = (uint16_t)a2->Xpos;
            if (anim == SPAboardleft && a2->Ypos <= 0x59 && a2->Ypos >= -0x59) {
                StartArenaAnim(7);
                anim = (a2->attribute & HI(3)) ? SPAbenchcheckr : SPAbenchcheckl;
            } else if (anim == SPAboardright && a2->Ypos <= 0x38 && a2->Ypos >= -0x38) {
                anim = (a2->attribute & HI(3)) ? SPAbenchcheckl : SPAbenchcheckr;
            } else if (a2->attribute & HI(3)) {            /* flipped sprites swap the side anims */
                if (anim == SPAboardright) anim = SPAboardleft;
                else if (anim == SPAboardleft) anim = SPAboardright;
            }
            a2->Xvel = 0;
            a2->Yvel = 0;
            a2->pflags3 |= 1 << pf3fall;
            a2->pflags |= 1 << pfdoff;
            goto eight;
        }
    }
seven:
    anim = SPAfallback;
    if (((dir - a2->facedir + 1) & 7) <= 2)
        goto hitter;
    anim = SPAfallfwd;
eight:
    if ((a2->facedir & 3) == 0 && (int8_t)a3->checking >= 0x14 && IS_PLAYER(a3) && !(a2->pflags3 & (1 << pf3fall)))
        anim = SPAflip;
hitter:
    a2->pflags |= 1 << pfalock;
    SetSPA(a2, anim);
    if (a3->position != 0 && a2->position != 0) {
        g.crowdlevel += 0x12C;
        g.CwdExciteLvl += 0xF;
    }
    if ((int16_t)g.puckc < 0 || g.puckc != (uint16_t)a2->SCnum) {
        newcheck();
        return;
    }
    ST_W(g.puckc);                                         /* the puck carrier lost it */
    if (!(g.gmode & (1 << gmhl)) && a2->SPA == SPAfallback && (a2->facedir & 3) == 0 && !(a2->pflags2 & (1 << 4))) {
        uint16_t r = randomd0(0xA0);
        if ((int16_t)r <= a2->impact && !(g.gmode & (1 << gmclock))) {
            SetSPA(a2, SPAinjuryfall);
            setInjuryType(a2, a3);
            if (!(g.sflags7 & (1 << 5)))
                SetSPA(a2, SPAinjury1);
            g.InjCntDown = 4;
            if (a3->position == 0 || g.OptPen == 0) {
                AddPenalty2(0x14, a3);
                return;
            }
            AddPenalty2(0x12, a3);                         /* roughing */
            Stop4Pen(team_struct(a2->pflags & (1 << pfteam)), a3);   /* a0 is still setInjuryType's team struct */
            return;
        }
    }
    song((a2->pflags & (1 << pfteam)) ? 0xB : 0xC);        /* crowd cheer / boo */
}

/* collide94.asm:setInjuryType. a2 = player injured, a3 = player checking. Marks a2 unavailable,
 * pumps up the crowd, plays sfx $D, locks the scroll on a2. a2's tmpdst becomes $FFFD (out for
 * the period) or, from a3's fighting byte, $FFFC (out for the game, sflags7 bit 5). */
void setInjuryType(struct SortObj *a2, struct SortObj *a3)
{
    a2->pflags2 |= 1 << 2;
    g.crowdlevel += 0x12C;
    g.CwdExciteLvl += 0x1E;
    sfx(0xD);
    g.xc1 = (uint16_t)a2->Xpos;
    g.yc1 = (uint16_t)a2->Ypos;
    g.sflags |= HI(sfslock);
    uint8_t *tm = team_struct(0);
    uint16_t d1 = 0;
    if (a2->pflags & (1 << pfteam)) {
        d1 = 0x8000;
        tm += tmsize;
    }
    d1 = (uint16_t)((d1 & 0xFF00) | a2->pnum);
    g.TempPlOffset = d1;
    uint8_t *status = tm + tmpdst + (int8_t)(uint8_t)d1 * 2;
    uint16_t f = getFgtbyte(a3, a2);
    if (f == 0) {
        g.sflags7 &= (uint8_t)~(1 << 5);
        wr16(status, 0xFFFD);
        return;
    }
    if (f != 3) {
        g.sflags7 &= (uint8_t)~(1 << 5);
        wr16(status, 0xFFFD);
        if (chkFgtBit1(a2, a3))
            return;
    }
    g.sflags7 |= 1 << 5;
    wr16(status, 0xFFFC);
}

/* collide94.asm:checkwallcoll. d2/d3 = x/y to test, a3 = object, wcradiusx/y = radius. Checks
 * the corner circles, the goals (checkgoal with the top or bottom net) and then the side and end
 * boards, calling wallcollb on a hit. Called from checkcoll. */
void checkwallcoll(int16_t x, int16_t y, struct SortObj *o)
{
    o->pflags3 &= (uint8_t)~(1 << pf3wall);
    int16_t d4 = (int16_t)(0x88 - g.wcradiusx);           /* side boards */
    int16_t d5 = (int16_t)(0x12A - g.wcradiusy);          /* end boards */
    int16_t cx = (int16_t)(-d4 + 0x40), cy = (int16_t)(-d5 + 0x40);   /* corner circle centre, radius 64 */
    int corner = 0;
    if (y <= cy) {
        if (x < cx) corner = 1;
        else { cx = (int16_t)-cx; if (x > cx) corner = 1; else checkgoal(sortobj(13), o, x, y); }
    } else {
        cy = (int16_t)-cy;
        if (y >= cy) {
            if (x < cx) corner = 1;
            else { cx = (int16_t)-cx; if (x > cx) corner = 1; else checkgoal(sortobj(12), o, x, y); }
        }
    }
    if (corner) {
        int16_t rx = (int16_t)(x - cx), ry = (int16_t)(y - cy);
        int16_t d0 = ry, d1 = (int16_t)-rx;
        uint32_t dist2 = (uint32_t)((int32_t)ry * ry + (int32_t)rx * rx);
        if (dist2 > 0x1000) {                              /* outside the circle: hit the curved boards */
            int16_t dist = (int16_t)sroot(dist2);
            wallcollb((int16_t)m68k_divs((int32_t)d0 << 8, dist), (int16_t)m68k_divs((int32_t)d1 << 8, dist), o);
        }
    }
    if ((uint16_t)(o->Wallcos | o->Wallsin) != 0)
        return;
    int16_t c = 0x100, s = 0;
    if (y >= d5) { wallcollb(c, s, o); return; }
    d5 = (int16_t)-d5; c = (int16_t)-c;
    if (y <= d5) { wallcollb(c, s, o); return; }
    s = c; c = 0;
    if (x >= d4) { wallcollb(c, s, o); return; }
    d4 = (int16_t)-d4; s = (int16_t)-s;
    if (x <= d4) { wallcollb(c, s, o); return; }
}

/* collide94.asm:checkgoal .setass. Give each skater of team a2 that is not fighting assignment
 * d0 and clear its no-collision flag; a one-timer in progress is ended. */
static void setass(uint16_t ass, uint8_t *team)
{
    struct SortObj *p = sortobj_at(rd16(team + tmsort));
    for (int i = 0; i < 6; i++, p++) {
        if (p->position <= 0)
            continue;
        if (p->pflags2 & (1 << pf2fight))
            continue;
        p->pflags &= (uint8_t)~(1 << pfnc);
        if (p->pflags3 & (1 << pf3onetimer)) {
            p->pflags3 &= (uint8_t)~(1 << pf3onetimer);
            EndOneTimer(p);
        }
        assinsert(ass, p);
    }
}

/* collide94.asm:GetPeriodTimeRemaining. d0 = (gsp << 14 | PerTimeTotal) - gameclock. */
uint16_t GetPeriodTimeRemaining(void)
{
    return (uint16_t)(((g.gsp << 14) | g.PerTimeTotal) - g.gameclock);
}

/* collide94.asm:checkgoal. a2 = goal object, a3 = object, d2/d3 = x/y. A puck under the
 * crossbar hits a post or the net (deflecting, with a sound and the crowd) or goes in; a
 * player goes to checkgoalp. Called from checkwallcoll. */
void checkgoal(struct SortObj *goal, struct SortObj *o, int16_t x, int16_t y)
{
    if (o->Zpos > 0xD)
        return;
    if (o->SCnum != 0xE) { checkgoalp(goal, o, x, y); return; }
    int16_t d2 = (int16_t)(x - goal->Xpos);
    int16_t d4 = (int16_t)(0x10 + g.wcradiusx);          /* half the goal width */
    if (d2 > d4) return;
    d4 = (int16_t)-d4;
    if (d2 < d4) return;
    int16_t d3 = (int16_t)(y - goal->Ypos);
    int16_t d5 = (int16_t)(2 + g.wcradiusy);             /* half the goal depth */
    if (d3 > d5) return;
    d5 = (int16_t)-d5;
    if (d3 < d5) return;
    ST_W(g.collflag);                                    /* inside the goal area */
    if (o->OldZpos >= 0xD) {                             /* came down from above the crossbar */
        o->Zpos = o->OldZpos;
        if (o->Zvel < 0)
            o->Zvel = (int16_t)-o->Zvel;
        return;
    }
    o->pflags &= (uint8_t)~(1 << pfgoal);
    if ((int16_t)g.puckc >= 0) {                         /* the carrier loses it */
        struct SortObj *car = sortobj((int16_t)g.puckc);
        ST_W(g.puckc);
        car->nopuck[0] = 8;
        int16_t py = sortobj(puckscnum)->Ypos;
        if (!(car->pflags & (1 << pfgoal)))
            py = (int16_t)-py;
        if (py < 0)
            o->pflags |= 1 << pfgoal;
    }
    int16_t d0 = (int16_t)0xFF00;
    int32_t dy = (POS32(o->Ypos, o->Yfrac) - POS32(o->OldYpos, o->OldYfrac)) >> 8;
    if (dy != 0) {
        if (dy >= 0) { d0 = (int16_t)-d0; d5 = (int16_t)-d5; }   /* top entry */
        d5 = (int16_t)(d5 + d3);
        int16_t dx = (int16_t)((POS32(o->Xpos, o->Xfrac) - POS32(o->OldXpos, o->OldXfrac)) >> 8);
        int32_t prod = (int32_t)dx * d5;
        int16_t div = (int16_t)dy;
        if (div != 0) {
            int32_t q = prod / div;
            if (q >= -32768 && q <= 32767) {
                d2 = (int16_t)(d2 - (int16_t)q);         /* x where the puck crossed the goal line */
                if (d2 >= d4) {
                    d4 = (int16_t)-d4;
                    if (d2 <= d4) {
                        if ((int16_t)(o->Ypos ^ d0) < 0) { wallcoll(d0, 0, o); return; }
                        d0 = (int16_t)-d0;
                        if (o->pflags & (1 << pfgoal)) { wallcoll(d0, 0, o); return; }
                        if (o->Zpos != 0xD) {
                            d4 = (int16_t)(d4 - 1);
                            if (d2 <= d4) {
                                d4 = (int16_t)-d4;
                                if (d2 >= d4)
                                    goto goal;
                            }
                        }
                        /* .deflectsf: off the post */
                        ChkShotStat();
                        int16_t snd = 0x25;
                        if (!(g.gmode & (1 << gmclock))) {
                            snd = 8;
                            g.crowdlevel += 0x12C;
                            g.CwdExciteLvl += 0x28;
                        }
                        sfx(snd);
                        int16_t v = (int16_t)randomd0(0x1000);
                        if (o->Ypos >= 0)
                            v = (int16_t)-v;
                        o->Yvel = v;
                        o->Xvel = (int16_t)randomd0s(0x1000);
                        o->Zvel = (int16_t)randomd0s(0x1000);
                        puckflip(o->Zvel, o);
                        return;
                    }
                }
            }
        }
    }
    /* .sideentry: into the side of the net */
    {
        int16_t dx = (int16_t)(o->Xpos - o->OldXpos);
        wallcoll(0, dx < 0 ? 0x100 : -0x100, o);
        return;
    }
goal:
    if (g.BA_PS_flags & HI(2)) {                         /* penalty shot / shootout: only the shooter's end */
        int top;
        if (!(g.gmode2 & HI(0)))
            top = (sortobj((int16_t)g.BA_Sktr_SCnum)->pflags & (1 << pfgoal)) != 0;
        else
            top = g.shootoutteam == 0;
        if (top) { if (sortobj(puckscnum)->Ypos < 0) return; }
        else { if (sortobj(puckscnum)->Ypos >= 0) return; }
        g.sflags4 |= HI(5);
        if (g.shootoutteam) g.awayshootgoals++; else g.homeshootgoals++;
        g.sflags8 |= 1 << 0;
        EndPenaltyShotPlay();
    } else if (g.gmode & (1 << gmclock)) {
        return;                                          /* no goal after the whistle */
    }
    g.sflags4 |= HI(0);
    g.sflags8 &= (uint8_t)~(1 << 3);
    ChkShotStat();
    stop_current_song(0);                                /* checkgoal's d0 is not followed (a register leak) */
    hal_wait_vblank();
    sfx(0);                                              /* SFXsiren */
    freezewindow();
    g.crowdlevel += 0x1F4;
    uint8_t *a2 = team_struct(0), *a1 = team_struct(1);
    if (o->Ypos < 0) { uint8_t *t = a2; a2 = a1; a1 = t; }
    if (g.gmode & (1 << gmdir)) { uint8_t *t = a2; a2 = a1; a1 = t; }
    wr16(a2 + tmscore, (uint16_t)(rd16(a2 + tmscore) + 1));
    if ((g.sflags4 & HI(5)) && !(g.gmode2 & HI(0)))
        wr16(a2 + 0x362, (uint16_t)(rd16(a2 + 0x362) + 1));           /* shorthanded goals */
    if (g.gmode2 & HI(4)) {
        g.gmode2 &= (uint16_t)~HI(4);
        wr16(a2 + 0x35A, (uint16_t)(rd16(a2 + 0x35A) + 1));           /* breakaway goals */
    }
    if (g.sflags2 & HI(sf2pwrplay)) {
        if (!(g.sflags2 & HI(sf2pwrtm))) { if (a2 == team_struct(1)) wr16(a2 + 0x356, (uint16_t)(rd16(a2 + 0x356) + 1)); }
        else { if (a2 == team_struct(0)) wr16(a2 + 0x356, (uint16_t)(rd16(a2 + 0x356) + 1)); }
    }
    { uint8_t *per = a2 + 0x342 + g.gsp * 2; wr16(per, (uint16_t)(rd16(per) + 1)); }   /* goals by period */
    if (g.sflags8 & (1 << 7)) {
        g.sflags8 &= (uint8_t)~(1 << 7);
        wr16(a2 + 0x35E, (uint16_t)(rd16(a2 + 0x35E) + 1));
    }
    if (a2 == team_struct(0)) {                          /* home goal: the goal song */
        g.HmTeam = g.HomeTeam;
        g.SongIndex = 3;
        ChooseSong();
        g.songdelay = 0x78;
        g.delayedsong = g.SongNum;
    }
    if (g.ScoreSumbytes == 0x168)
        g.ScoreSumbytes -= 6;                            /* summary full: overwrite the last entry */
    uint8_t *e = g.ScoreSum + g.ScoreSumbytes;
    g.ScoreSumbytes += 6;
    wr16(e, GetPeriodTimeRemaining());
    e[2] = (uint8_t)(2 + rd16(a2 + tmap) - rd16(a1 + tmap));
    g.CwdExciteLvl += 0x1E;
    if (a2 != team_struct(0)) {
        g.CwdExciteLvl -= 0x14;                          /* less for an away goal */
        e[2] |= 0x80;
    }
    int16_t scorer = (int16_t)rd16(a2 + 0x18);
    e[3] = (uint8_t)scorer;
    e[4] = 0xFF; e[5] = 0xFF;                            /* no assists yet */
    a2[0xB4 + scorer]++;
    int16_t as = (int16_t)rd16(a2 + 0x1A);
    if (as >= 0 && as != scorer) {
        e[4] = (uint8_t)as;
        a2[0xCE + as]++;
        as = (int16_t)rd16(a2 + 0x1C);
        if (as >= 0 && as != scorer) {
            e[5] = (uint8_t)as;
            a2[0xCE + as]++;
        }
    }
    int16_t gk = (int16_t)rd16(a1 + tmgoalie);
    if (gk >= 0)
        a1[0xB4 + gk]++;                                 /* goal against */
    ChkShotStat();
    PenGoalStuff(a1, a2);
    g.BA_PS_flags &= (uint16_t)~HI(3);
    printscores1();
    g.crowdnoisedelay = 0x2710;
    if (!(g.gmode2 & HI(0)))
        setass(7, a2);
    g.collflag = 0;
    o->Xvel = 0;
    o->Yvel = 0;
    o->Xpos = (int16_t)(o->Xpos >= 0 ? 6 : -6);
    o->Ypos = (int16_t)(o->Ypos >= 0 ? 0x110 : -0x110);
    o->Zvel = 0x600;
    o->Zpos = 0;
    g.puckcross2 = 0xFF;
    g.puckcross6 = 0xFF;
    o->pflags |= 1 << pfnc;
    assreplace(0x1A, o);                                 /* puckunflip */
    SetSPA(o + 1, SPAsiren);                             /* the goal light object follows the puck */
    AddPenalty2(0xE, sortobj_at(rd16(a1 + tmsort)));     /* the other team's first object: faceoff */
}

/* collide94.asm:checkgoalp. Player a3 against goal a2 (oval): CheckBump, then wallcoll without
 * setting the wall collision bit. Entered from checkgoal. */
void checkgoalp(struct SortObj *goal, struct SortObj *o, int16_t x, int16_t y)
{
    if (o->pflags2 & (1 << 5))
        return;
    if (o->Zpos > 0xA)
        return;
    if (!IS_PLAYER(o))
        return;
    int16_t d3 = (int16_t)(y - goal->Ypos), d0 = (int16_t)-d3;
    int16_t d2 = (int16_t)(x - goal->Xpos), d1 = d2;
    int16_t sx = (int16_t)(d2 << 4), sy = (int16_t)(d3 << 4);
    uint16_t qx = (uint16_t)m68k_divu((uint32_t)((int32_t)sx * sx), 0x400);
    if (qx > 0x100)
        return;
    uint16_t qy = (uint16_t)m68k_divu((uint32_t)((int32_t)sy * sy), 0x79);
    if ((uint16_t)(qy + qx) > 0x100)
        return;
    if (CheckBump(goal, o))
        return;
    int16_t dist = (int16_t)(sroot((uint32_t)((int32_t)d0 * d0 + (int32_t)d1 * d1)) + 1);
    int16_t c = (int16_t)m68k_divs((int32_t)d0 << 8, dist);
    int16_t s = (int16_t)m68k_divs((int32_t)d1 << 8, dist);
    g.sflags6 |= HI(4);
    wallcoll(c, s, o);
    g.sflags6 &= (uint16_t)~HI(4);
}

/* collide94.asm:CheckBump. a2 = goal, a3 = player. On one frame in 32, a player near the puck
 * moving fast knocks the net: it takes a quarter of his velocity, he stops, the scroll locks,
 * and while the clock runs penalty 8 is called. Returns 1 then: the asm unwinds the stack and
 * returns straight to checkwallcoll. */
int CheckBump(struct SortObj *goal, struct SortObj *o)
{
    if ((vdp_hv16() & 0x1F) != 0)
        return 0;
    int16_t d0 = (int16_t)(sortobj(puckscnum)->Ypos - goal->Ypos);
    if (d0 > 0x28 || d0 < -0x28)
        return 0;
    int16_t vx = o->Xvel, vy = o->Yvel;
    if (!(vx > 0x2000 || vx < -0x2000 || vy > 0x2000 || vy < -0x2000))
        return 0;
    goal->Xvel = (int16_t)(vx >> 2);
    goal->Yvel = (int16_t)(vy >> 2);
    o->Xvel = 0;
    o->Yvel = 0;
    g.sflags |= HI(sfslock);
    if (!(g.gmode & (1 << gmclock)))
        AddPenalty2(8, o);
    return 1;
}

/* collide94.asm:wallcollb. a3 = object, d0/d1 = cos/sin of the wall. A puck high enough goes
 * over the boards (out of play: scroll lock, no collision, and a penalty on the last player to
 * touch it while the clock runs), right over the bench with a glass hit; otherwise wallcoll. */
void wallcollb(int16_t c, int16_t s, struct SortObj *o)
{
    if (o->SCnum != 0xE) { wallcoll(c, s, o); return; }
    if (o->Zpos <= 0x1D) {
        if ((uint16_t)o->Zpos <= 0x12) { wallcoll(c, s, o); return; }
        if (o->Ypos >= 0x118) {
            if (o->Xpos < 0xD || o->Xpos > 0xF || o->Yvel < 0xFA0) { wallcoll(c, s, o); return; }
            o->Yvel = (int16_t)(o->Yvel >> 1);
            SetSPA(o + 1, SPAbglass);
            sfx(0xE);
            g.crowdlevel += 0x258;
            g.CwdExciteLvl += 0xF;
        }
    }
    g.sflags |= HI(sfslock);
    o->pflags |= 1 << pfnc;
    if (o->Ypos < 0)
        o->attribute |= 0x8000;
    (o + 1)->frame = 0;
    if (g.gmode & (1 << gmclock))
        return;
    AddPenalty2(6, sortobj((int16_t)g.ltplayer));        /* PenOOP */
}

/* collide94.asm:wallcoll. d0/d1 = cosine/sine of the wall, a3 = object. Bounce a3 off the
 * wall: the puck loses speed, flips and plays a board sound; a player sets the wall collision
 * bit (unless checkgoalp asked not to) and plays SFXplayerwall on a hard hit. */
void wallcoll(int16_t c, int16_t s, struct SortObj *o)
{
    o->Wallcos = c;
    o->Wallsin = s;
    int16_t v1n = (int16_t)(((int32_t)c * o->Yvel - (int32_t)s * o->Xvel) >> 8);
    int16_t v1t = (int16_t)(((int32_t)c * o->Xvel + (int32_t)s * o->Yvel) >> 8);
    v1n = (int16_t)-v1n;
    if (o->SCnum == 0xE) {
        g.sflags2 &= (uint16_t)~HI(sf2shot);
        if (v1n >= 0)
            goto nocoll;
        v1n = (int16_t)(v1n >> 2);                         /* reduce normal speed on the puck */
        if (v1n <= -0x400) {
            o->Zvel = (int16_t)-(int16_t)randomd0(0x800);
            puckflip(o->Zvel, o);
            int16_t d0 = (int16_t)((v1n >> 8) >> 2);
            d0 = (int16_t)(d0 + 4);
            if (d0 < 0)
                d0 = 0;
            sfx((int16_t)((d0 & 3) + 0x28));                /* SFXpuckwall1 + 0-3 */
        }
        int16_t t = (int16_t)(v1t >> 6);                   /* reduce tangent speed on the puck */
        v1t = (int16_t)(v1t - t);
        t = (int16_t)(t >> 1);
        v1t = (int16_t)(v1t - t);
    } else {
        if (v1n > 0x3E8)
            goto nocoll;
        if (g.sflags6 & HI(4))
            g.sflags6 &= (uint16_t)~HI(4);
        else
            o->pflags3 |= 1 << pf3wall;
        if (v1n <= -0x1000 && o->impact >= 0xA)
            sfx(0x20);                                     /* SFXplayerwall */
        v1n = (int16_t)(v1n >> 2);
        if (v1n >= -0x384)
            v1n = -0x3E8;
    }
    o->Xvel = (int16_t)(((int32_t)c * v1t - (int32_t)s * v1n) >> 8);
    o->Yvel = (int16_t)(((int32_t)s * v1t + (int32_t)c * v1n) >> 8);
    if (o->Zvel >= 0)
        o->Zvel = 0;
nocoll: ;
}

/* collide94.asm:checkpuckcoll_sfx. Puck in the air: sfx 5 once when sflags4 bit 2 is set. */
void checkpuckcoll_sfx(void)
{
    if (g.sflags4 & HI(2)) {
        g.sflags4 &= (uint16_t)~HI(2);
        sfx(5);
    }
}

/* collide94.asm:checkpuckcoll .chkgoalie. The goalie's reach (ChkBodyG) from his agility, then
 * the puck against his body. */
static void chkgoalie(struct SortObj *p, struct SortObj *puck)
{
    uint16_t d0 = p->legstr;
    if ((g.sflags8 & (1 << 1)) || (g.sflags7 & (1 << 6)))
        d0 = (uint16_t)((d0 & 0xFF00) | (uint8_t)(d0 + 2));
    d0 >>= 1;
    if (p->pflags & (1 << pfjoycon))
        d0 = 0xF;
    d0 = (uint16_t)(d0 + d0);
    if ((g.gmode2 & HI(0)) && d0 != 0)
        d0 -= 2;
    if ((int16_t)d0 >= 0x1E)
        d0 = 0x1E;
    g.ChkBodyG = cbg[d0 / 2];
    g.ChkBodySqG = cbgsq[d0 / 2];
    g.NegChkBodyG = (uint16_t)-g.ChkBodyG;
    if (g.sflags6 & HI(1)) { g.ChkBodyG = 0xC; g.ChkBodySqG = 0x90; g.NegChkBodyG = 0xFFF4; }
    if (p->SPA == SPAgstackr || p->SPA == SPAgstackl) { g.ChkBodyG = 0x12; g.ChkBodySqG = 0x144; g.NegChkBodyG = 0xFFEE; }
    if (sortobj(puckscnum)->Zpos > 0xF)
        goto exit2;
    int16_t dx = p->Xpos;
    if (p->SPA == SPAgstackr || p->SPA == SPAgstackl) {
        int16_t d1 = (int16_t)(p->SPA == SPAgstackr ? -6 : 6);
        if (!(p->pflags & (1 << pfgoal)))
            d1 = (int16_t)-d1;
        if (p->handed[0] & 1)
            d1 = (int16_t)-d1;
        dx = (int16_t)(dx + d1);
    }
    dx = (int16_t)(dx - puck->Xpos);
    if (dx > (int16_t)g.ChkBodyG || dx < (int16_t)g.NegChkBodyG)
        goto exit2;
    int16_t dy = (int16_t)(p->Ypos - puck->Ypos);
    if (dy > (int16_t)g.ChkBodyG || dy < (int16_t)g.NegChkBodyG)
        goto exit2;
    if ((uint32_t)((int32_t)dx * dx + (int32_t)dy * dy) > g.ChkBodySqG)
        goto exit2;
    puckgoalie(p, puck, dx, dy);
    return;
exit2:
    if (g.sflags4 & HI(2)) {
        g.sflags4 &= (uint16_t)~HI(2);
        if (g.puckc != (uint16_t)p->SCnum)
            sfx(5);                                        /* SFXshotwiff */
    }
}

/* collide94.asm:checkpuckcoll .ccx. d4 = object number * 2, a3 = puck: stick, body or goalie. */
static void ccx(unsigned obj2, struct SortObj *puck)
{
    unsigned n = obj2 >> 1;
    if ((uint16_t)n == g.puckc)
        return;
    if ((int16_t)n > 0xB)
        return;
    struct SortObj *p = sortobj((int)n);
    if ((g.BA_PS_flags & HI(2)) && (uint16_t)n != g.BA_Sktr_SCnum && (uint16_t)n != g.BA_Goalie_SCnum)
        return;
    if (p->pflags & (1 << pfnc))
        return;
    if (p->nopuck[0] != 0)
        return;
    if (!(p->pflags2 & (1 << 2)) && puck->Zpos <= 5 && puck->Zvel <= 0x200) {
        int16_t hx, hy;
        GetHot(p, &hx, &hy);
        int16_t d0 = (int16_t)(hx + p->Xpos - puck->Xpos);
        if (d0 <= 0xE && d0 >= -0xE) {
            int16_t d1 = (int16_t)(hy + p->Ypos - puck->Ypos);
            if (d1 <= 0xE && d1 >= -0xE) {
                uint32_t dsq = (uint32_t)((int32_t)d0 * d0 + (int32_t)d1 * d1);
                uint32_t lim = 0xC4;                       /* stick reach squared */
                if ((int8_t)puck->nopuck[0] > 0)
                    lim >>= 2;
                if (dsq <= lim) { puckstick(p, puck, dsq); return; }
            }
        }
    }
    if (p->position == 0) { chkgoalie(p, puck); return; }
    if (p->pflags3 & (1 << pf3onetimer))
        return;
    int16_t d0 = (int16_t)(p->Xpos - puck->Xpos);
    if (d0 > 8 || d0 < -8)
        return;
    int16_t d1 = (int16_t)(p->Ypos - puck->Ypos);
    if (d1 > 8 || d1 < -8)
        return;
    uint32_t dsq = (uint32_t)((int32_t)d0 * d0 + (int32_t)d1 * d1);
    if (dsq > 0x40)
        return;
    puckbody(p, puck, dsq);
}

/* collide94.asm:checkpuckcoll. a3 = puck. Clears Yvel past the back boards, then walks the
 * screen order up and down from the puck and tries every object within $16 in y. */
void checkpuckcoll(struct SortObj *puck)
{
    if (puck->Ypos > 0x190 || puck->Ypos <= -0x190)
        puck->Yvel = 0;
    if (puck->Zpos > 0x10) { checkpuckcoll_sfx(); return; }
    unsigned n = (uint16_t)puck->SCnum;
    uint16_t pos = g.OOlistpos[n];
    while (pos != 0xF) {
        unsigned nxt = g.OOlist[pos + 1];
        if ((int16_t)(g.Ylist[nxt / 2] - puck->Ypos) > 0x16)
            break;
        ccx(nxt, puck);
        pos++;
    }
    pos = g.OOlistpos[n];
    while (pos != 0) {
        unsigned nxt = g.OOlist[pos - 1];
        if ((int16_t)(puck->Ypos - g.Ylist[nxt / 2]) > 0x16)
            return;
        ccx(nxt, puck);
        pos--;
    }
}

/* collide94.asm:puckstick. a2 = player, a3 = puck, d0 = distance squared. A stick on the puck
 * carrier can steal it (stick handling rolls, tighter in the slot); otherwise a slow enough
 * puck is caught (puckglue) or a one-timer shoots it. */
void puckstick(struct SortObj *p, struct SortObj *puck, uint32_t dsq)
{
    if (p->position == 0 && (g.gmode & (1 << gmhl)))
        return;                                            /* the goalie lets the highlight shot in */
    p->pflags &= (uint8_t)~(1 << pfdoff);
    if ((int16_t)g.puckc >= 0) {
        struct SortObj *car = sortobj((int16_t)g.puckc);
        if (!((car->pflags ^ p->pflags) & (1 << pfteam)))
            return;                                        /* no stealing from a teammate */
        if (dsq > ((g.sflags6 & HI(5)) ? 0x40u : 0x24u))
            return;
        if (p->position != 0) {
            if (car->position == 0)
                return;                                    /* no stealing from a goalie */
            uint16_t his = makepde((int8_t)(car->stickhand >> 1), car);
            uint16_t mine = makepde((int8_t)(p->stickhand >> 1), p);
            uint16_t r = randomd0((uint16_t)(his - mine + 0x24));
            if (r > ((g.sflags6 & HI(5)) ? 4u : 2u))
                return;
        }
        car->nopuck[0] = (uint8_t)(makepde((int8_t)(car->stickhand >> 1), car) + 0x14);
        p->nopuck[0] = (uint8_t)(makepde((int8_t)(p->stickhand >> 1), p) + 0x14);
        g.lastplayer = (uint16_t)car->SCnum;
        g.sflags &= (uint16_t)~(HI(sfspdir) | HI(sfssdir));
        sfx(6);                                            /* SFXstdef */
        a2touchpuck(p, puck);
        deflect(puck);
        return;
    }
    if (p->position == 0 && dsq > 0x40)
        return;
    if (p->pflags3 & (1 << pf3onetimer)) {
        if (dsq > 0x64) {
            if (dsq <= 0x144 && p->SPAnum < 0x18)
                p->SPAnum = 0x18;
            return;
        }
        if (p->SPAnum < 0x18) {
            p->SPAnum = 0x18;
            g.sflags8 |= 1 << 7;
        }
    }
    uint32_t spd = (uint32_t)((int32_t)puck->Xvel * puck->Xvel + (int32_t)puck->Yvel * puck->Yvel);
    uint16_t lim = 0;
    if (!(g.sflags2 & HI(sf2shot))) {
        if (p->pflags3 & (1 << pf3onetimer)) { onetimershot(puck, p); return; }
        lim = (uint16_t)(p->stickhand * 0x15E);
    }
    lim = (uint16_t)(lim + 0x32C8);
    if (spd <= (uint32_t)lim * lim) { puckglue(p, puck); return; }
    p->nopuck[0] = 8;
    sfx(6);
    a2touchpuck(p, puck);
    deflect(puck);
}

/* collide94.asm:puckglue .setd0player. Give control of player d0 to a pad if his team is
 * controlled (pads 3 and 4 too). */
static void setd0player(uint16_t d0)
{
    if (d0 == g.c1playernum || d0 == g.c2playernum)
        return;
    if (g.cont3team && d0 == g.c3playernum)
        return;
    if (g.cont4team && d0 == g.c4playernum)
        return;
    if (g.cont3team || g.cont4team) {
        struct SortObj *o = sortobj(d0);
        if (o->position == 0 && (o->pflags & (1 << pfjoycon)))
            return;
    }
    uint16_t team = (uint16_t)((d0 < 6) ? 1 : 2);
    if (g.lastplayer != g.c2playernum && team == g.cont1team) { setc1player(d0); return; }
    if (team == g.cont2team) { setc2player(d0); return; }
    if (team == g.cont1team) setc1player(d0);
}

/* collide94.asm:puckglue. Player a2 takes the puck: faceoff and pass stats, the crowd, a goalie
 * hold time, then pad control. Entered from puckstick and puckgoalie. */
void puckglue(struct SortObj *p, struct SortObj *puck)
{
    (void)puck;
    int16_t snd = 7;                                       /* SFXpuckget */
    uint16_t d0 = (uint16_t)p->SCnum;
    g.puckc = d0;
    uint8_t *other = team_struct(0), *own = team_struct(1);
    if (!(p->pflags & (1 << pfteam))) { uint8_t *t = other; other = own; own = t; }
    other[0x1A] = 0xFF;
    other[0x1C] = 0xFF;
    other[tmflags] |= 1 << 3;
    if (g.sflags3 & (1 << 4)) {                            /* faceoff won */
        g.sflags3 &= (uint8_t)~(1 << 4);
        wr16(own + 0xE, (uint16_t)(rd16(own + 0xE) + 1));
        g.crowdlevel += 0xC8;
        snd = 0xC;
        if (own == team_struct(0)) {
            g.CwdExciteLvl += 0xA;
            snd = 0xB;
        }
    }
    song(snd);
    if (d0 == g.passplayer)
        wr16(own + 0x14, (uint16_t)(rd16(own + 0x14) + 1));   /* pass completed */
    ST_W(g.passplayer);
    g.sflags &= (uint16_t)~(HI(sfspdir) | HI(sfssdir));
    if (p->position == 0) {
        ChkShotStat();
        p->temp5 = 0x8C;
        if (p->SPA == SPAgdive)
            p->temp5 = 5;
    }
    setd0player(d0);
}

/* collide94.asm:puckbody. The puck hits player a2 and bounces off; a high, fast puck makes him
 * fall, a slower one starts the catch animation. */
void puckbody(struct SortObj *p, struct SortObj *puck, uint32_t dsq)
{
    if (p->pflags3 & (1 << pf3onetimer))
        return;
    if (puck->Zpos <= 8 && ((uint16_t)dsq & 0xF) != 0)
        return;
    a2touchpuck(p, puck);
    sfx(0x24);                                             /* SFXpuckbody */
    puck->Zvel = 0;
    p->nopuck[0] = 8;
    int16_t d0 = (int16_t)(puck->Xpos - p->Xpos);
    int16_t d1 = (int16_t)(puck->Ypos - p->Ypos);
    if (d1 == 0)
        GetHot(p, &d0, &d1);
    int16_t vx = puck->Xvel, vy = puck->Yvel;
    puck->Xvel = (int16_t)((puck->Xvel & 0xFF) | ((uint8_t)d0 << 8));
    puck->Yvel = (int16_t)((puck->Yvel & 0xFF) | ((uint8_t)d1 << 8));
    puckflip(d0, puck);
    if (puck->Zpos <= 8)
        return;
    if ((uint32_t)((int32_t)vx * vx + (int32_t)vy * vy) > 0x9000000) {   /* fast puck */
        if (puck->Zpos > 0xC)
            FallDown(p, puck);
        return;
    }
    if (p->pflags & (1 << pfalock))
        return;
    p->pflags |= 1 << pfalock;
    SetSPA(p, SPAcatch);
}

/* collide94.asm:puckgoalie. The puck hits goalie a2 (d0/d1 = goalie - puck x/y). The save
 * odds come from the frame tables; a save bounces the puck off, a slow puck near the crease is
 * held. */
void puckgoalie(struct SortObj *gk, struct SortObj *puck, int16_t dx, int16_t dy)
{
    if (g.gmode & (1 << gmhl))
        return;
    uint16_t d2 = 0;
    if (puck->Zpos <= 8)
        d2 += 2;
    uint16_t d0 = (uint16_t)((vtoa((int16_t)-dx, (int16_t)-dy) - gk->facedir) & 7);
    if ((d0 & 3) == 0)
        d0 = vdp_hv16();                                   /* straight on: a random side */
    d0 = ((d0 & 4) >> 2) ^ 1;
    d2 = (uint16_t)((d2 + d0) * 2);                        /* 0 glove L, 2 glove R, 4 stick L, 6 stick R */
    (void)savelist;                                        /* the save attribute is looked up but not used */
    const uint8_t *tbl = saveodds;
    if (gk->attribute & HI(3))
        d2 ^= 2;
    uint16_t fr = gk->frame, base = SPFGoalie;
    if ((int16_t)fr >= SPFSiren) {
        base = SPFghighr;
        tbl = saveodds3;
    }
    fr = (uint16_t)(fr - base);
    d0 = (uint16_t)((tbl[fr] >> d2) & 3);
    g.sflags4 &= (uint16_t)~HI(2);
    ChkShotStat();
    a2touchpuck(gk, puck);
    if (puck->Yvel > 0x3000 || puck->Yvel <= -0x3000)
        song((gk->pflags & (1 << pfteam)) ? 0xD : 0xB);
    sfx(0x24);                                             /* SFXpuckbody */
    if ((int16_t)g.puckc >= 0) {
        struct SortObj *car = sortobj((int16_t)g.puckc);
        ST_W(g.puckc);
        car->nopuck[0] = 0x14;
        g.lastplayer = (uint16_t)car->SCnum;
        g.sflags &= (uint16_t)~(HI(sfspdir) | HI(sfssdir));
        goto bounceoff;
    }
    if (gk->SPA == SPAgdive)
        goto bounceoff;
    {
        uint16_t w = (uint16_t)((uint8_t)(2 + gk->shotspd) << 9);
        int16_t v = (int16_t)(randomd0(w) + 0xC00);
        v = (int16_t)(v + v);
        if (v < 0)
            v = 0x7FFF;
        if (!(gk->pflags2 & (1 << 1)) && puck->Xpos <= 0x2C && puck->Xpos >= -0x2C && puck->Ypos <= 0x10E && puck->Ypos >= -0x10E
            && (puck->Ypos > 0xD2 || puck->Ypos <= -0xD2))
            v = (int16_t)(v >> 2);
        if (v < puck->Xvel || v < puck->Yvel)
            goto bounceoff;
        v = (int16_t)-v;
        if (v > puck->Xvel || v > puck->Yvel)
            goto bounceoff;
        if (gk->pflags2 & (1 << 2))
            goto bounceoff;
        puck->Zvel = 0;
        puckglue(gk, puck);
        return;
    }
bounceoff:
    puck->Zvel = 0;
    gk->nopuck[0] = 0xA;
    gk->temp4 = 4;
    {
        int16_t bx = (int16_t)(puck->Xpos - gk->Xpos), by = (int16_t)(puck->Ypos - gk->Ypos);
        if (by == 0)
            GetHot(gk, &bx, &by);
        puck->Xvel = (int16_t)((puck->Xvel & 0xFF) | ((uint8_t)bx << 8));
        puck->Yvel = (int16_t)((puck->Yvel & 0xFF) | ((uint8_t)by << 8));
        puckflip(bx, puck);
    }
}

/* collide94.asm:deflect. Random puck direction on a deflection. */
void deflect(struct SortObj *puck)
{
    ST_W(g.puckc);
    puck->Yvel = (int16_t)randomd0s(0x1000);
    puck->Xvel = (int16_t)randomd0s(0x1000);
    puck->Zvel = (int16_t)randomd0(0x1000);
    puckflip(puck->Zvel, puck);
}

/* collide94.asm:getpde. a3 = player -> d0 = his energy level (tmpde of his team). */
int16_t getpde(struct SortObj *pl)
{
    uint8_t *tm = team_struct(pl->pflags & (1 << pfteam));
    return (int16_t)rd16(tm + tmpde + (int8_t)pl->pnum * 2);
}

/* collide94.asm:makepde. Scale d0 by player a0's energy level (full while sflags7 bit 4). */
uint16_t makepde(int8_t v, struct SortObj *pl)
{
    int16_t e = getpde(pl);
    if (g.sflags7 & (1 << 4))
        e = 0x1000;
    uint32_t prod = (uint32_t)((int32_t)e * v) << 4;
    return (uint16_t)(prod >> 16);
}

/* collide94.asm:setpde. d1 = roster number * 2, a2 = team, d0 = new energy level (floor 0). */
void setpde(uint8_t *team, int16_t d1, int16_t e)
{
    if (e < 0)
        e = 0;
    wr16(team + tmpde + d1, (uint16_t)e);
}

/* collide94.asm:setpersonel. Set the personnel of team a2: PlList gives the players wanted on
 * the ice; each sort object gets newpos / newpnum. */
void setpersonel(uint8_t *team)
{
    struct SortObj *o = sortobj_at(rd16(team + tmsort));
    for (int i = 0; i < 6; i++) {
        o[i].newpos = 0xFF;
        o[i].newpnum = 0xFF;
    }
    SetPlList(team);
    for (int d4 = 5; d4 >= 0; d4--) {                      /* players already on the ice keep their object */
        uint8_t d5 = g.PlList[d4];
        if (!d5)
            continue;
        d5--;
        int found = -1;
        for (int d3 = 0; d3 < 6; d3++)
            if (o[d3].pnum == d5) { found = d3; break; }
        if (found < 0)
            continue;
        o[found].newpos = g.PlList[6 + d4];
        o[found].newpnum = d5;
        g.PlList[d4] = 0;
    }
    for (int d4 = 5; d4 >= 0; d4--) {                      /* the rest take a free object */
        uint8_t d5 = g.PlList[d4];
        if (!d5)
            continue;
        d5--;
        int d3 = 5;
        struct SortObj *a3 = o - 1, *a0 = NULL;
        for (int guard = 0; guard < 16; guard++) {
            a3++;
            int mi = (int8_t)a3->newpnum < 0;
            if (!mi && --d3 != -1)
                continue;
            if (mi)
                a0 = a3;
            if (a3->position >= 0)
                break;
            if (--d3 == -1)
                break;
        }
        if (a0) {
            a0->newpos = g.PlList[6 + d4];
            a0->newpnum = d5;
        }
        g.PlList[d4] = 0;
    }
}

/* collide94.asm:TryAddPlayerToList. d0 = player number (1 based), d4 = PlList slot. Put him in
 * the slot if he is on the bench or ice and not in the list yet; returns 0 if not. */
int TryAddPlayerToList(uint8_t *team, uint16_t player, int slot)
{
    int16_t st = (int16_t)rd16(team + tmpdst + (player - 1) * 2);
    if (st > 0 || st == -3 || st == -4)
        return 0;
    for (int i = 5; i >= 0; i--)
        if (g.PlList[i] == (uint8_t)player)
            return 0;
    g.PlList[slot] = (uint8_t)player;
    return 1;
}

/* collide94.asm:SetPlList. Build PlList, the players wanted on the ice now, from the current
 * line by position priority; an unavailable player is replaced from the substitution list for
 * his position, or from the whole roster. */
void SetPlList(uint8_t *team)
{
    memset(g.PlList, 0, 6);
    const uint8_t *prio = rom_ptr(SYM_priolist);
    if ((int16_t)rd16(team + tmgoalie) < 0)
        prio++;                                            /* no goalie: an extra attacker */
    const uint8_t *line = team + tmlines + rd16(team + tmline) * 8;
    for (int d4 = (int16_t)rd16(team + tmap) - 1; d4 >= 0; d4--) {
        uint8_t pos = prio[d4];
        g.PlList[d4] = line[pos];
        g.PlList[6 + d4] = pos;
        if (pos == 0)
            g.PlList[d4] = (uint8_t)(1 + rd16(team + tmgoalie));
    }
    for (int d4 = 5; d4 >= 0; d4--) {
        int8_t pl = (int8_t)g.PlList[d4];
        if (!pl)
            continue;
        int16_t st = (int16_t)rd16(team + tmpdst + (pl - 1) * 2);
        if (st != -3 && st != -4 && st <= 0)
            continue;                                      /* available */
        const uint8_t *sub = rom_ptr(SYM_sublist);
        sub += (int16_t)rd16(sub + (int8_t)g.PlList[6 + d4] * 2);
        int ok = 0;
        for (;;) {
            int8_t pos = (int8_t)*sub++;
            if (pos < 0)
                break;
            if (TryAddPlayerToList(team, (team + tmlines)[pos], d4)) { ok = 1; break; }
        }
        if (!ok) {
            int16_t d3 = (int16_t)GetPlayerCount(team);
            for (;;) {
                int16_t d0 = d3;
                d3--;
                if (d3 < 0)
                    break;
                if (TryAddPlayerToList(team, (uint16_t)d0, d4))
                    break;
            }
        }
    }
}

/* collide94.asm:SetupPenaltyShot. Team a2: the shooter plays centre, the goalie stays, every
 * other player is made unavailable with the wait assignment. */
void SetupPenaltyShot(uint8_t *team)
{
    struct SortObj *o = sortobj_at(rd16(team + tmsort));
    for (int i = 0; i < 6; i++, o++) {
        uint16_t n = (uint16_t)o->SCnum;
        if (n == g.BA_Sktr_SCnum) {
            Setplass(o);
            g.savednewpnum = o->newpnum;
            o->newpnum = (uint8_t)g.BA_Skater_Offset;
            o->position = 4;
        } else if (n == g.BA_Goalie_SCnum) {
            Setplass(o);
            o->position = 0;
        } else {
            o->pflags2 |= 1 << 2;
            assreplace(0x20, o);                           /* assgoaliebreakwait */
            goto next;
        }
        {
            uint16_t d3 = (uint16_t)(o->newpnum * 2);
            wr16(team + tmpdst + d3, 0xFFFF);
            setplayer(d3 >> 1, o);
        }
next:
        o->newpnum = 0xFF;
        o->newpos = 0xFF;
    }
}

/* collide94.asm:forcepldata. Force the players of team a2 to their requested position and
 * roster slot (faceoffs only). */
void forcepldata(uint8_t *team)
{
    struct SortObj *o = sortobj_at(rd16(team + tmsort));
    for (int i = 0; i < 6; i++, o++) {
        int16_t d0 = (int8_t)o->newpos;
        if (o->position == 0 && d0 != 0) {                 /* a goalie leaving the net */
            int16_t sc = o->SCnum;
            o->SCnum = 0xF;
            Set4WayPlayerStub(o);
            o->SCnum = sc;
        }
        o->position = d0;
        if (d0 >= 0) {
            Setplass(o);
            if (o->position == 4)
                assinsert(0x11, o);                        /* anearest */
            uint16_t d3 = (uint16_t)(o->newpnum * 2);
            wr16(team + tmpdst + d3, 0xFFFF);              /* on the ice */
            setplayer(d3 >> 1, o);
        }
        o->newpnum = 0xFF;
        o->newpos = 0xFF;
    }
}

/* collide94.asm:ResetBench .rb. One team: count the players left in the box into PBnum (byte
 * at PBnum, home in the high nibble) and put everyone else on the bench. */
static void rb(uint8_t d1, uint8_t *tm)
{
    uint8_t *pb = ram_ptr(RAM_BASE + RAM_OFF_PBnum);
    for (int d0 = 0x32; d0 >= 0; d0 -= 2) {
        *pb = (uint8_t)(*pb + d1);
        uint16_t st = rd16(tm + tmpdst + d0);
        if ((int16_t)st > 0) {
            if (!(st & (1u << 12)))
                continue;
            if (st & 0x7FF)
                continue;
            *pb = (uint8_t)(*pb - d1);
            continue;
        }
        *pb = (uint8_t)(*pb - d1);
        if (st == 0xFFFD || st == 0xFFFC)
            continue;                                      /* injured */
        wr16(tm + tmpdst + d0, 0xFFFE);
    }
}

/* collide94.asm:ResetBench. Everyone out of the penalty box and onto his bench. */
void ResetBench(void)
{
    *ram_ptr(RAM_BASE + RAM_OFF_PBnum) = 0;
    rb(0x10, team_struct(0));
    rb(1, team_struct(1));
}

/* collide94.asm:Setplass. Player a3's initial assignment by position. */
void Setplass(struct SortObj *o)
{
    if (o->position < 0)
        return;
    assreplace(alist[o->position], o);
}

/* collide94.asm:ClampNibble. Clamp byte d3 to 0-$1E. */
uint8_t ClampNibble(uint8_t v)
{
    if ((int8_t)v < 0)
        return 0;
    if ((int8_t)v > 0x1E)
        return 0x1E;
    return v;
}

/* collide94.asm:setplayer. Bring player d3 of the roster onto the ice as sort object a3 and set
 * his attributes from the team data, through AttributeCalc, with the power play / penalty kill,
 * home / away and third period bonuses. */
void setplayer(uint16_t roster, struct SortObj *o)
{
    o->pflags2 &= (uint8_t)~(1 << 6);
    uint8_t *tm = team_struct(o->pflags & (1 << pfteam));
    o->pnum = (uint8_t)roster;
    int16_t idx = (int16_t)((int8_t)(uint8_t)roster * 2);
    uint16_t ass = 0xA;                                    /* aepen */
    int16_t st = (int16_t)rd16(tm + tmpdst + idx);
    int give = 1;
    if (st < 0) {
        ass = 9;                                           /* assben */
        if (st != -2)
            give = 0;
    }
    if (give) {
        assinsert(ass, o);
        o->pflags &= (uint8_t)~(1 << pfalock);
        o->SPA = 0;
    }
    wr16(tm + tmpdst + idx, 0xFFFF);
    const uint8_t *td = rom_ptr(rd32(tm + tmdata));
    const uint8_t *odds = td + (int16_t)rd16(td + 8);     /* the team's ScoreOdds bytes */
    uint8_t *bonus = ram_ptr(RAM_BASE + RAM_OFF_PPBonus);
    bonus[0] = bonus[1] = bonus[2] = bonus[3] = 0;        /* PPBonus, PKBonus, ThirdPBonus, HmAwBonus */
    if (o->position != 0) {
        if (g.sflags2 & HI(sf2pwrplay)) {
            if (!chkpk2(o))
                g.PPBonus = odds[1] & 0xF;
            else
                g.PKBonus = (uint8_t)-(odds[1] >> 4);
        }
        uint8_t ha = (uint8_t)-(odds[2] & 0xF);
        if (!(o->pflags & (1 << pfteam)))
            ha = odds[2] >> 4;
        g.HmAwBonus = ha;
        if ((int16_t)g.gsp >= 2) {
            int lead = 1;
            if (g.gsp == 2 && (int16_t)(GetPeriodTime() >> 1) > (int16_t)g.gameclock)
                lead = 0;                                  /* first half of the third period */
            if (lead) {
                int16_t diff = (int16_t)(g.HmGoals[0] - g.AwGoals[0]);
                int behind = 1;
                if (diff != 0) {
                    int neg = diff < 0;
                    if (o->pflags & (1 << pfteam))
                        neg = !neg;
                    behind = neg;
                }
                if (behind)
                    g.ThirdPBonus = 2;
            }
        }
    }
    const uint8_t *p = td + (int16_t)rd16(td);
    for (int i = 0; i <= (int)(uint16_t)(idx >> 1); i++) {
        p += (int16_t)rd16(p);
        p += 8;
    }
    p -= 8;
    o->rostnum = p[0];
    o->weight = (uint8_t)((p[1] & 0xF0) >> 1);
    uint8_t d3;
    g.TempWord2 = 3;  d3 = AttributeCalc(p[1] & 0xF, o);  o->legstr = d3;
    g.TempWord2 = 4;  d3 = AttributeCalc(p[2] >> 4, o);   o->legspd = d3;
    g.TempWord2 = 5;  d3 = AttributeCalc(p[2] & 0xF, o);
    d3 = (uint8_t)(d3 + g.PPBonus + g.PKBonus + g.HmAwBonus + g.ThirdPBonus);
    d3 = ClampNibble(d3); d3 >>= 1; d3 ^= 0xF; d3 = (uint8_t)(d3 + 0xF); d3 >>= 1;
    o->aioff = d3;                                         /* (30 - (OfA + bonuses) / 2) / 2 */
    g.TempWord2 = 6;  d3 = AttributeCalc(p[3] >> 4, o);
    d3 = (uint8_t)(d3 + g.HmAwBonus);
    d3 = ClampNibble(d3); d3 >>= 1; d3 ^= 0xF; d3 = (uint8_t)(d3 + 0xF); d3 >>= 1;
    o->aidef = d3;
    o->shotspd = p[3] & 0xF;
    g.TempWord2 = 7;  o->shotspd = AttributeCalc(o->shotspd, o);
    g.TempWord2 = 8;  d3 = AttributeCalc(p[4] >> 4, o);
    d3 = (uint8_t)(d3 + g.ThirdPBonus);
    o->checking = ClampNibble(d3);
    o->attribute &= (uint16_t)~HI(3);
    o->handed[0] = (uint8_t)((p[4] & 1) ^ 1);             /* 1 = left handed */
    if (o->handed[0] == 0)
        o->attribute |= HI(3);
    o->fighting = p[4] & 0xE;
    g.TempWord2 = 0xA; d3 = AttributeCalc(p[5] >> 4, o);
    o->stickhand = ClampNibble((uint8_t)(d3 + g.PPBonus + g.PKBonus + g.HmAwBonus));
    g.TempWord2 = 0xB; d3 = AttributeCalc(p[5] & 0xF, o);
    o->shotacc = ClampNibble((uint8_t)(d3 + g.PPBonus + g.PKBonus + g.HmAwBonus));
    g.TempWord2 = 0xC; o->endurance = AttributeCalc(p[6] >> 4, o);
    g.TempWord2 = 0xD; d3 = AttributeCalc(p[6] & 0xF, o);
    o->spodds = ClampNibble((uint8_t)(d3 + g.ThirdPBonus + g.ThirdPBonus));
    g.TempWord2 = 0xE; d3 = AttributeCalc(p[7] >> 4, o);
    o->passacc = ClampNibble((uint8_t)(d3 + g.PPBonus + g.HmAwBonus));
    o->aggress = p[7];
    g.TempWord2 = 0xF; d3 = AttributeCalc(o->aggress & 0xF, o);
    o->aggress = d3 & 0xF;
}
