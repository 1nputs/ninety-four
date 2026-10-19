/* onetimer94.asm: new in 94, the one-timer. A pass is aimed at a teammate who shoots the puck
 * as it arrives; this file picks the target, sets up the shooter and plays his animation and
 * shot. */
#include "game.h"
#include "anim_gen.h"
#include "rom.h"
#include "symbols_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
static inline int16_t puckx(void) { return (int16_t)g.puckx[0]; }
static inline int16_t pucky(void) { return (int16_t)g.pucky; }
static inline int16_t puckvx(void) { return (int16_t)g.puckvx; }
static inline int16_t puckvy(void) { return (int16_t)g.puckvy; }
static inline int16_t puckc(void) { return (int16_t)g.puckc; }
static inline uint16_t divu16(uint32_t n, uint16_t d) { return (uint16_t)m68k_divu(n, d); }

/* onetimer94.asm:puckvzadj. Set puckvz for a top shelf shot from the distance to the goal line
 * over puckvy, at most $7FFF. */
void puckvzadj(struct SortObj *o)
{
    int16_t d0 = pucky();
    if (d0 < 0) d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 - 0x108);
    if (d0 < 0) d0 = (int16_t)-d0;
    uint32_t dist = (uint32_t)(uint16_t)d0 << 16;
    int16_t d1 = puckvy();
    if (d1 == 0) return;
    if (d1 < 0) d1 = (int16_t)-d1;
    uint16_t d2 = g.PalMode != 0 ? 0x16 : 0x11;           /* frames per second-ish */
    uint16_t t = divu16(dist, (uint16_t)d1);
    t = divu16(t, d2);
    if (t == 0) t = 1;
    uint32_t v = 0xA0000;
    uint16_t d3 = t;
    uint32_t prod = (uint32_t)t * d2;
    v = m68k_divu(v, (uint16_t)prod);
    uint32_t h = (uint32_t)(uint16_t)(d2 * 3) * d3;
    if (!((int32_t)h < 0x7FFF)) h = 0x7FFF;
    int16_t vz = (int16_t)((uint16_t)v + (uint16_t)h);
    if (vz < 0) vz = 0x7FFF;
    g.puckvz[0] = (uint16_t)vz;
}

/* onetimer94.asm:OneTimerPass. puckvz = sqrt(12 * onetimerheight), then puckvx / puckvy toward
 * the target in puckvz / 3 frames; flip the puck. */
void OneTimerPass(struct SortObj *o)
{
    uint32_t d0 = (uint32_t)0xC * g.onetimerheight;
    d0 = (d0 & 0xFFFF) << 16;
    uint16_t vz = sroot(d0);
    g.puckvz[0] = vz;
    uint32_t q = m68k_divu((uint32_t)(int32_t)(int16_t)vz, 3);
    int16_t t = (int16_t)q;
    if (t == 0) t = 1;
    int32_t d1 = (int32_t)((uint32_t)(uint16_t)(g.onetimertargetx - puckx()) << 16);
    g.puckvx = (uint16_t)m68k_divs(d1, t);
    d1 = (int32_t)((uint32_t)(uint16_t)(g.onetimertargety - pucky()) << 16);
    g.puckvy = (uint16_t)m68k_divs(d1, t);
    puckflip((int16_t)g.puckvz[0], sortobj(puckscnum));
}

/* onetimer94.asm:OneTimerTarget. The pass target for receiver a3: an offset by facing from the
 * near / far / goalie tables, +-10 at random, into onetimertargetx / y; sflags6 bit 2 = near. */
void OneTimerTarget(struct SortObj *o)
{
    g.sflags6 &= (uint16_t)~HI(2);
    uint32_t tbl;
    if (o->position == 0)
        tbl = SYM_OneTimerGoalieTbl;
    else {
        int16_t y = o->Ypos;
        if (!PFLAG(o, pfgoal)) y = (int16_t)-y;
        g.sflags6 |= HI(2);
        tbl = SYM_OneTimerNearTbl;
        if (y >= 0x58) {
            tbl = SYM_OneTimerFarTbl;
            g.sflags6 &= (uint16_t)~HI(2);
        }
    }
    uint16_t d0 = (uint16_t)o->facedir;
    if (!PFLAG(o, pfgoal)) d0 = (uint16_t)((d0 + 4) & 7);
    const uint8_t *e = rom_ptr(tbl) + d0 * 4;
    int16_t d1 = (int16_t)rd16(e), d2 = (int16_t)rd16(e + 2);
    if (tbl == SYM_OneTimerFarTbl) {
        int16_t y = o->Ypos;
        if (y < 0) y = (int16_t)-y;
        if (y >= 0x8A) {
            int16_t x = o->Xpos;
            if (x < 0) x = (int16_t)-x;
            if (x <= 0x37) d2 = 0x103;                    /* in the slot: aim at the goal line */
        }
    }
    if (!PFLAG(o, pfgoal)) { d1 = (int16_t)-d1; d2 = (int16_t)-d2; }
    d1 = (int16_t)(d1 + randomd0(0xA));
    d2 = (int16_t)(d2 + randomd0(0xA));
    g.onetimertargetx = (uint16_t)d1;
    if (o->position != 0) {                               /* keep it $3C from a skater */
        d1 = (int16_t)(d1 - o->Xpos);
        if (d1 < 0) {
            d1 = (int16_t)(d1 + 0x3C);
            if (d1 >= 0) g.onetimertargetx = (uint16_t)(g.onetimertargetx - d1);
        } else {
            d1 = (int16_t)(d1 - 0x3C);
            if (d1 < 0) g.onetimertargetx = (uint16_t)(g.onetimertargetx - d1);
        }
    }
    g.onetimertargety = (uint16_t)d2;
}

/* onetimer94.asm:assonetimer. Assignment $23, player a3 shooting a one-timer: take control of
 * him, start the animation, then shoot when the puck arrives (EndOneTimer). */
void assonetimer(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        int was = o->pflags3 & (1 << pf3onetimer);
        o->pflags3 |= 1 << pf3onetimer;
        if (!was) {
            g.onetimerflags &= (uint16_t)~HI(0);
            g.sflags8 &= (uint8_t)~0x20;
            g.onetimerflags = 0;
            g.onetimerclock[0] = 0;
            ST_W(g.passplayer);
            uint16_t d0 = (uint16_t)o->SCnum;
            g.onetimerplayer = d0;
            if (!PFLAG(o, pfjoycon) && (int16_t)g.inputjoy >= 0) {
                if (g.inputjoy != 0) {
                    if (g.TmpJoyPuckCarrier == 1) {       /* pad 4 borrows pad 2's variables */
                        uint16_t t = g.cont2team, p = g.c2playernum;
                        g.cont2team = g.cont4team;
                        g.c2playernum = g.c4playernum;
                        setc2player(d0);
                        g.c4playernum = g.c2playernum;
                        g.cont4team = g.cont2team;
                        g.c2playernum = p;
                        g.cont2team = t;
                    } else
                        setc2player(d0);
                } else if (g.TmpJoyPuckCarrier == 0) {    /* pad 3 borrows pad 1's */
                    uint16_t t = g.cont1team, p = g.c1playernum;
                    g.cont1team = g.cont3team;
                    g.c1playernum = g.c3playernum;
                    setc1player(d0);
                    g.c3playernum = g.c1playernum;
                    g.cont3team = g.cont1team;
                    g.c1playernum = p;
                    g.cont1team = t;
                } else
                    setc1player(d0);
            }
            uint16_t d1 = setonetimeranim(o);
            o->SPAnum = 0;
            SetSPA(o, d1);
            o->pflags |= 1 << pfalock;
            o->pflags2 |= 2;
            return;
        }
    }
    if (!(g.onetimerflags & HI(0))) {                     /* is the puck coming? */
        int16_t d0 = (int16_t)(puckx() - o->Xpos);
        if (d0 > 0x3C || d0 <= -0x3C) {
            if ((int16_t)(d0 ^ puckvx()) >= 0) goto cancel;
        }
        d0 = (int16_t)(pucky() - o->Ypos);
        if (d0 > 0x3C || d0 <= -0x3C) {
            if ((int16_t)(d0 ^ puckvy()) >= 0) goto cancel;
        }
    }
    if (puckc() < 0) goto loose;
cancel:
    g.sflags8 &= (uint8_t)~0x80;
    goto shoot;
loose:
    if (g.gmode & (1 << gmclock)) goto shoot;
    if ((int16_t)o->SPAnum >= 0x10 || (g.onetimerflags & HI(2))) {
        g.onetimerflags |= HI(2);                         /* windup */
        if (!(g.onetimerflags & HI(1)) && o->SPAnum == 0x18) {
            o->SPAcnt += 0x30;
            g.onetimerflags |= HI(1);
        }
    } else {
        int16_t dx = (int16_t)(o->Xpos - puckx());
        if (dx < 0) dx = (int16_t)-dx;
        int16_t dy = (int16_t)(o->Ypos - pucky());
        if (dy < 0) dy = (int16_t)-dy;
        int16_t v = puckvx();
        int16_t d = dx;
        if (v == 0 || !(dy <= dx)) { v = puckvy(); d = dy; }
        uint32_t frames = (uint32_t)(uint16_t)d << 16;
        if (v < 0) v = (int16_t)-v;
        uint16_t d1 = g.PalMode != 0 ? 0x16 : 0x11;
        if (v == 0) v = 1;
        uint16_t t = divu16(frames, (uint16_t)v);
        t = divu16(t, d1);
        int16_t d2 = (int16_t)(o->SPAnum >> 2);
        d2 = (int16_t)((-(d2 - 6)) << 2);
        if ((int16_t)t > d2) {
            g.onetimerclock[0] = (uint16_t)(g.onetimerclock[0] + elapsed);   /* wait */
            goto chkhold;
        }
        o->SPAnum = (uint16_t)(0x18 - o->SPAnum);         /* swing now */
        goto chkanim;
    }
chkhold:
    if ((g.onetimerflags & HI(1)) && (int16_t)o->SPAnum > 0x18 && !(g.onetimerflags & HI(0)))
        goto shoot;
    if ((g.onetimerflags & HI(0)) && o->SPAnum == 0x18 && (int16_t)o->SPAcnt > 1)
        o->SPAcnt = 1;
chkanim:
    if ((o->pflags2 & 2) && (g.onetimerflags & HI(0)))
        o->pflags &= (uint8_t)~(1 << pfalock);
    if (o->pflags2 & 2) return;
shoot:
    EndOneTimer(o);
}

/* onetimer94.asm:setonetimeranim. The one-timer animation: SPAshotf or SPAshotb from the angle
 * to the goal. */
uint16_t setonetimeranim(struct SortObj *o)
{
    int16_t d0 = (int16_t)-o->Xpos;
    int16_t d1 = PFLAG(o, pfgoal) ? 0x108 : -0x108;
    d1 = (int16_t)(d1 - o->Ypos);
    uint16_t dir = vtoa(d0, d1);
    return CheckOneTimerFacing(dir, o) ? SPAshotf : SPAshotb;
}

/* onetimer94.asm:PuckOnAttackHalf. Returns the Z flag: set when the puck is not on the half of
 * the goal player a3 shoots at. */
int PuckOnAttackHalf(struct SortObj *o)
{
    int16_t d0 = pucky();
    if (!PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
    return d0 < 0;
}

/* onetimer94.asm:onetimershot. Do the one-timer shot, credit the last two passers as the
 * assists, excite the crowd and count the attempt. */
void onetimershot(struct SortObj *a2, struct SortObj *o)
{
    g.passdir = 4;
    doshot(o);
    rp tm = team_struct(PFLAG(o, pfteam));
    uint16_t d0 = o->pnum;
    if (!rules[RULE_ASSIST_FIX] || d0 != r16(tm, tmassist1))     /* (rules: a give-and-go keeps the second assist) */
        w16(tm, tmassist2, r16(tm, tmassist1));
    w16(tm, tmassist1, r16(tm, tmlasttouch));
    w16(tm, tmlasttouch, d0);
    g.sflags8 |= 0x80;
    g.crowdlevel += 0x96;
    g.CwdExciteLvl += 0xA;
    w16(tm, tmonetimers, (uint16_t)(r16(tm, tmonetimers) + 1));       /* one-timer attempts */
    g.onetimerflags |= HI(0);
    o->pflags2 |= 2;
    g.sflags6 |= HI(1);
}

/* onetimer94.asm:CheckOneTimerFacing. Only its Z flag is used: the shot is a forehand when the
 * sprite's flip bit matches the side the goal is on. */
int CheckOneTimerFacing(uint16_t dir, const struct SortObj *o)
{
    uint16_t d0 = (uint16_t)((8 - dir) & 7);
    uint16_t d1 = (uint16_t)((o->facedir + d0) & 7);
    uint16_t v = (int16_t)d1 > 4 ? (o->attribute & 0x800) : ((o->attribute ^ 0xFFFF) & 0x800);
    return v == 0;
}
