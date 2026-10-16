/* checks94.asm: skater and goalie assignments. Plays the wing and center positions and the
 * goalie, whether a player or the computer controls him, with his saves and breakaways; the
 * puck carrier and the computer's choices of when to change lines, shoot and pass; the puck's
 * own assignments (faceoff, penalty shot, normal play); and the acceleration code every
 * skater goes through. */
#include "game.h"
#include "anim_gen.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "checks_tables_gen.h"
#include "checks_strings_gen.h"

int16_t elapsed;   /* d7: frames since the last game loop pass */

/* The assignment code keeps byte timers in the HIGH byte of the temp words (sub.b d7,temp1(a3))
 * and a direction in the low byte of temp2. */
#define HIB(w) ((int8_t)((uint16_t)(w) >> 8))
#define LOB(w) ((int8_t)(w))
#define SET_HIB(f, b) ((f) = (int16_t)(((uint16_t)(uint8_t)(b) << 8) | ((uint16_t)(f) & 0xFF)))
#define SET_LOB(f, b) ((f) = (int16_t)(((uint16_t)(f) & 0xFF00) | (uint8_t)(b)))
/* sub.b d7,temp(a3); bpl ...: count the high byte down, result as the 68k sees it */
#define TICK_HIB(f) (SET_HIB(f, (int8_t)(HIB(f) - elapsed)), HIB(f))

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
static inline int16_t puckx(void) { return (int16_t)g.puckx[0]; }
static inline int16_t pucky(void) { return (int16_t)g.pucky; }
static inline int16_t puckz(void) { return (int16_t)g.puckz[0]; }
static inline int16_t puckvx(void) { return (int16_t)g.puckvx; }
static inline int16_t puckvy(void) { return (int16_t)g.puckvy; }
static inline int16_t puckvz(void) { return (int16_t)g.puckvz[0]; }
static inline int16_t puckc(void) { return (int16_t)g.puckc; }
static inline struct SortObj *puck_obj(void) { return sortobj(puckscnum); }
/* the first sort object of the other team (6 home, 6 away) */
static inline struct SortObj *opponents(const struct SortObj *o) { return sortobj(o->SCnum >= 6 ? 0 : 6); }
static inline struct SortObj *teammates(const struct SortObj *o) { return sortobj(o->SCnum >= 6 ? 6 : 0); }
static inline uint8_t *own_team(const struct SortObj *o) { return team_struct(PFLAG(o, pfteam)); }
static inline int32_t sq(int16_t v) { return (int32_t)v * v; }
/* divs.w with the overflow flag */
static int32_t divs_ovf(int32_t dividend, int16_t divisor, int *ovf)
{
    *ovf = 0;
    if (divisor == 0) return dividend;
    int32_t q = dividend / divisor;
    if (q < -32768 || q > 32767) { *ovf = 1; return dividend; }
    return (int32_t)(((uint32_t)(uint16_t)(dividend % divisor) << 16) | (uint16_t)q);
}

/* The timer / aidef prologue shared by the assignments: when the timer runs out reload it from
 * aidef, one less while the crowd meter record is broken (sflags7 bit 6). Returns 1 when it ran
 * out this pass. */
static int aidef_timer(struct SortObj *o)
{
    if (TICK_HIB(o->temp1) >= 0)
        return 0;
    SET_HIB(o->temp1, o->aidef);
    if ((g.sflags7 & 0x40) && HIB(o->temp1) != 0)
        SET_HIB(o->temp1, HIB(o->temp1) - 1);
    return 1;
}
/* the goalie variant: aidef / 4, one less with the crowd record broken */
static void goalie_timer_reload(struct SortObj *o)
{
    uint8_t d0 = o->aidef;
    if (d0 != 0 && (g.sflags7 & 0x40))
        d0--;
    SET_HIB(o->temp1, d0 >> 2);
}
/* count the breakaway and excite the crowd (asspuckc, assnearest, breakaway) */
static void breakaway_crowd(struct SortObj *o)
{
    uint8_t *tm = own_team(o);
    wr16(tm + tmbreakaways, (uint16_t)(rd16(tm + tmbreakaways) + 1));
    g.CwdExciteLvl += 0x14;
    g.crowdlevel += 0xC8;
}
/* a random point inside skating zone z of a zone table: {x base, x range, y base, y range} */
static void zone_target(const uint16_t *zones, unsigned z, struct SortObj *o)
{
    const uint16_t *e = zones + z * 4;
    o->temp3 = (int16_t)(randomd0s(e[1]) + e[0]);
    o->temp4 = (int16_t)(randomd0s(e[3]) + e[2]);
}

/* checks94.asm:asswingo. Player a3 is a winger on offense: skate to a random spot in the zone
 * the puck is in (or will be), evading the puck carrier. a2 = his team. */
void asswingo(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (g.gmode & (1 << gmclock)) { donothing(o); return; }
    if (check4bench(o)) return;
    if (PFLAG(o, pfjoycon)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        ST_W(o->temp5);                                   /* old zone number */
        o->temp1 = 0;
        o->temp2 = 8;
    }
    if (aidef_timer(o)) {
        int16_t d1 = puckc();
        if (d1 >= 0 && (int16_t)((d1 - 6) ^ (o->SCnum - 6)) < 0) {   /* the other team has the puck */
            assreplace(3, o);                             /* asswingd */
            return;
        }
        int16_t d0 = pucky();
        int16_t d3 = (int16_t)((puckvy() >> 6) + d0);
        if (!PFLAG(o, pfgoal)) { d0 = (int16_t)-d0; d3 = (int16_t)-d3; }
        int16_t zone = 0;                                 /* byte offset into the zone table */
        if (d3 >= -0x58) {
            zone = 8;
            if (d0 >= 0x58 && !(tm[tmflags] & 0x10)) {
                zone = 0x10;
                if (d3 >= 0x108) zone = 0x18;
            }
        }
        if (zone != o->temp5 || (vdp_hv16() & 0x7F) == 0) {   /* a new zone, or now and then while waiting */
            o->temp5 = zone;
            zone_target(wingo_zones, (unsigned)zone / 8, o);
        }
    }
    int16_t d0 = o->temp3;
    if (o->position != 5) d0 = (int16_t)-d0;              /* left wing mirrors */
    int16_t d1 = o->temp4;
    if (!PFLAG(o, pfgoal)) { d0 = (int16_t)-d0; d1 = (int16_t)-d1; }
    skateto(d0, d1, EvadePC, o);
}

/* checks94.asm:asscenterd. Player a3 is the center on defense: stay between the puck and his
 * own high slot. */
void asscenterd(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (g.gmode & (1 << gmclock)) { donothing(o); return; }
    if (check4bench(o)) return;
    if (PFLAG(o, pfjoycon)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = 0;
        o->temp2 = 8;
    }
    if (aidef_timer(o)) {
        int16_t d1 = puckc();
        if (d1 >= 0 && (int16_t)((d1 - 6) ^ (o->SCnum - 6)) >= 0) {  /* his team has the puck */
            assreplace(6, o);                             /* asscentero */
            return;
        }
    }
    int16_t d0 = (int16_t)(puckx() >> 1);
    int16_t d2 = pucky();
    if (!PFLAG(o, pfgoal)) d2 = (int16_t)-d2;
    int16_t d1 = -128;                                    /* own high slot */
    if (d2 >= -0x58)                                      /* puck not in the defensive zone */
        d1 = (int16_t)((int16_t)(d1 + pucky()) >> 1);
    if (!PFLAG(o, pfgoal)) d1 = (int16_t)-d1;
    skateto(d0, d1, NULL, o);
}

/* checks94.asm:asscentero. Player a3 is the center on offense. */
void asscentero(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (g.gmode & (1 << gmclock)) { donothing(o); return; }
    if (check4bench(o)) return;
    if (PFLAG(o, pfjoycon)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        ST_W(o->temp5);
        o->temp1 = 0;
        o->temp2 = 8;
    }
    if (aidef_timer(o)) {
        int16_t d1 = puckc();
        if (d1 >= 0 && (int16_t)((d1 - 6) ^ (o->SCnum - 6)) < 0) {
            assreplace(5, o);                             /* asscenterd */
            return;
        }
        int16_t d0 = pucky();
        if (!PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
        int16_t zone = 0;
        if (d0 >= -0x58) {
            zone = 8;
            if (d0 >= 0x58 && !(tm[tmflags] & 0x10)) {
                zone = 0x10;
                if (d0 >= 0x108) zone = 0x18;
            }
        }
        if (zone != o->temp5 || (vdp_hv16() & 0x7F) == 0) {
            o->temp5 = zone;
            zone_target(centero_zones, (unsigned)zone / 8, o);
        }
    }
    int16_t d0 = o->temp3, d1 = o->temp4;
    if (!PFLAG(o, pfgoal)) d1 = (int16_t)-d1;             /* (only Y, unlike the wings: the center's X is kept) */
    skateto(d0, d1, EvadePC, o);
}

/* checks94.asm:assgoaliectrl. Joypad controlled goalie: out of the crease area the pad takes
 * over (pf3gctrl) and checkanim runs; else time his hold on the puck and his timer. (The
 * beq.w *+4 at the end targets the rts: it never falls into assgoaliecpu.) */
void assgoaliectrl(uint8_t *tm, struct SortObj *o)
{
    if (!PFLAG(o, pfjoycon)) { assexit(o); return; }
    if (!PFLAG(o, pfna)) {
        int16_t d0 = o->Xpos, d1 = o->Ypos;
        if (d1 <= 0xAA && d1 >= -0xAA && !(g.gmode & (1 << gmclock)))
            AddPenalty2(8, o);                            /* stoppage: goalie out of range */
        d0 = (int16_t)(d0 - (int16_t)g.Hpos);
        d1 = (int16_t)(d1 - (int16_t)g.Vpos);
        o->pflags3 &= (uint8_t)~(1 << pf3gctrl);
        if (d0 >= 0x74 || d0 <= -0x74 || d1 >= 0x64 || d1 <= -0x64)
            o->pflags3 |= 1 << pf3gctrl;
        if (o->pflags3 & (1 << pf3gctrl)) { checkanim(tm, o); return; }
    }
    if (PFLAG(o, pfalock)) return;
    if (check4bench(o)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = 0;
        o->temp2 = 8;
        ST_W(o->temp4);
    }
    if (o->pflags2 & 2) return;                           /* animation in progress */
    if (g.gmode & (1 << gmclock)) return;
    if (o->temp5 >= 0) {
        if (o->SCnum == puckc()) {
            o->temp5 = (int16_t)(o->temp5 - elapsed);
            if (o->temp5 < 0) {                           /* held the puck too long */
                if (g.BA_PS_flags & HI(2)) g.BA_PS_flags |= HI(4);
                else AddPenalty2(8, o);
            }
        } else
            ST_W(o->temp5);
    }
    if (TICK_HIB(o->temp1) >= 0) return;
    goalie_timer_reload(o);
}

/* checks94.asm:assgoaliecpu. a3 = goalie. */
void assgoaliecpu(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfjoycon)) { assgoaliectrl(tm, o); return; }
    checkanim(tm, o);
}

/* checks94.asm:checkanim. The goalie's own play: the stick slam after a goal, skating back to
 * the crease, passing the puck when he has it, diving on a loose puck, and reading the shot
 * (puckcross) to pick a save or move across the crease. Ends in playeracc or stopna. */
void checkanim(uint8_t *tm, struct SortObj *o)
{
    g.TmpPuckX = (uint16_t)puckx();
    if ((g.sflags4 & HI(0)) && !(o->pflags2 & 2)) {
        if (g.sflags4 & HI(1)) {
            if (!(g.sflags4 & HI(3)) && o->frame == SPFslamtop && o->frame == SPFslambot) {   /* never both: dead */
                g.sflags4 |= HI(3);
                sfx(SYM_SFXcheck);
            }
        } else if ((int16_t)(pucky() ^ o->Ypos) >= 0 && o->Xpos <= 0x14 && o->Xpos > -0x14) {
            g.sflags4 |= HI(1);
            g.sflags4 &= (uint16_t)~HI(3);
            SetSPA(o, PFLAG(o, pfgoal) ? SPAgslambot : SPAgslamtop);   /* stick slam after a goal */
            o->pflags2 |= 2;
        }
    }
    if (PFLAG(o, pfalock)) return;
    if (check4bench(o)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = 0;
        o->temp2 = 8;
        ST_W(o->temp4);
    }
    int in_crease = o->Xpos <= 0x34 && o->Xpos >= -0x34 && o->Ypos <= 0x10E && o->Ypos >= -0x10E
                    && (o->Ypos > 0xD2 || o->Ypos < -0xD2);
    if (!in_crease) {                                     /* skate back to the middle */
        int16_t d0 = o->Xpos >= 0 ? 4 : -4;
        int16_t d1 = PFLAG(o, pfgoal) ? -0xE4 : 0xE4;
        skateto(d0, d1, NULL, o);
        return;
    }
    if (o->pflags2 & 2) return;
    if (g.sflags4 & HI(0)) {
        if (o->frame == SPFslamtop + 1) o->facedir = 0;
        else if (o->frame == SPFslambot + 1) o->facedir = 4;
    }
    SetSPA(o, SPAgready);
    if (!(g.BA_PS_flags & HI(2)) && (g.gmode & (1 << gmclock)))
        return;
    if (o->temp5 >= 0) {
        if (o->SCnum == puckc()) {
            o->temp5 = (int16_t)(o->temp5 - elapsed);
            if (o->temp5 < 0) {
                if (g.BA_PS_flags & HI(2)) g.BA_PS_flags |= HI(4);
                else AddPenalty2(8, o);                   /* PenGhold: whistle for the faceoff */
            }
        } else
            ST_W(o->temp5);
    }
    int16_t d0, d1, d2, d3;
    const uint8_t *pc;
    if (TICK_HIB(o->temp1) >= 0)
        goto nodec;
    goalie_timer_reload(o);
    if (PFLAG(o, pfjoycon) && !(o->pflags3 & (1 << pf3gctrl))) {
        assinsert(0x1D, o);                               /* assgoaliectrl */
        return;
    }
    d0 = pucky(); d1 = o->Ypos;
    if ((int16_t)(d0 ^ d1) < 0) {                         /* puck in the other half */
        d0 = 0;
        d2 = PFLAG(o, pfgoal) ? -0xF4 : 0xF4;
        goto de5;
    }
    o->temp4 = (int16_t)(o->temp4 - 1);
    if (o->temp4 < 0) o->temp4 = -1;
    if (o->SCnum == puckc()) {
        if (o->temp5 < 0) o->temp5 = 0x5A;
        ST_W(o->temp4);
        if (o->temp5 <= 0x5A && (vdp_hv16() & 3) == 0) {
            struct SortObj *a0 = sortobj(PFLAG(o, pfteam) ? 0 : 6);   /* the other team */
            for (int i = 0; i < 6; i++, a0++) {
                if (a0->pflags2 & 4) continue;
                d1 = (int16_t)(pucky() - a0->Ypos);
                if (d1 > 0x1C || d1 < -0x1C) continue;
                d1 = (int16_t)(puckx() - a0->Xpos);
                if (d1 > 0x19) continue;
                if (d1 > -0x19) goto de1;                 /* an opponent is on him */
            }
            g.threat = 1;
            if (chk4pass(o)) return;
        }
        goto de1;
    }
    /* nobody has the puck: dive on it when it is loose and slow beside him */
    if (o->temp4 != 0) goto de1;
    if (puckc() >= 0) goto de1;
    d0 = (int16_t)(puckx() - o->Xpos);
    if (d0 > 0x14 || d0 < -0x14) goto de1;
    d1 = pucky();
    if (d1 > 0x108 || d1 < -0x108) goto de1;
    d1 = (int16_t)(d1 - o->Ypos);
    if (d1 > 0x1E || d1 < -0x1E) goto de1;
    {
        int16_t vx = puckvx(), vy = puckvy();
        if (vx < 0) vx = (int16_t)-vx;
        if (vy < 0) vy = (int16_t)-vy;
        if ((int16_t)(vx + vy) > 0x1000) goto de1;
    }
    o->facedir = (int16_t)vtoa(d0, d1);                   /* face the puck */
    o->nopuck[0] = 8;
    SetSPA(o, SPAgdive);
    o->pflags2 |= 2;
    g.crowdlevel += 0x96;
    return;
de1:
    pc = ram_ptr(RAM_BASE + RAM_OFF_puckcross);           /* x / frames to the goal line, top then bottom */
    d3 = 0x104;
    if (PFLAG(o, pfgoal)) { pc += 4; d3 = (int16_t)-d3; }
    if (o->Ypos > 0x104 || o->Ypos <= -0x104) {          /* behind the goal line: back to the post */
        d2 = 0;
        d0 = 0;
        if (o->Xpos > 0x2C || o->Xpos < -0x2C) goto de5;
        d0 = o->Xpos >= 0 ? 0x88 : -0x88;
        goto de5;
    }
    if (puckc() >= 0 && o->SCnum != puckc()) {
        const struct SortObj *car = sortobj(puckc());
        d0 = (int16_t)(puckx() - car->Xpos);
        d0 = (int16_t)((d0 >> 1) + car->Xpos);
        g.TmpPuckX = (uint16_t)d0;
    }
    d0 = (int16_t)g.TmpPuckX;
    d1 = pucky();
    ClampYPosition(&d0, &d1, d3, o);
    d0 = (int16_t)vtoa(d0, d1);
    AdjustFacingDirection(d0, o);
    d0 = (int16_t)(((g.gameclock & 7) << 4) + 0xA0);     /* frames ahead to look */
    if (pucky() > 0xDB || pucky() <= -0xDB)
        d0 = (int16_t)(d0 - 0x40);
    d1 = d0;
    d0 = (int16_t)((int16_t)(((int32_t)d0 * puckvx()) >> 16) + (int16_t)g.TmpPuckX);   /* where the puck will be */
    d1 = (int16_t)((int16_t)(((int32_t)d1 * puckvy()) >> 16) + pucky());
    ClampYPosition(&d0, &d1, d3, o);
    {
        int32_t dsq = sq(d0) + sq(d1);
        if ((uint32_t)dsq > 0x384) {                      /* far: scale the move down */
            d2 = (int16_t)(1 + (int16_t)sroot((uint32_t)dsq));
            int16_t d4 = 0x12;
            if (g.sflags & HI(sfssdir)) d4 = (int16_t)(d4 + 8);
            int32_t p1 = (int32_t)d1 * d4;
            d4 = (int16_t)(d4 + 8);
            int32_t p0 = (int32_t)d0 * d4;
            d0 = (int16_t)m68k_divs(p0, d2);
            d1 = (int16_t)m68k_divs(p1, d2);
        }
    }
    d1 = (int16_t)(d1 + d3);
    d2 = d1;
    if (!(g.gmode2 & HI(0)) && !(g.BA_PS_flags & HI(2)) && (g.sflags & HI(sfssdir))
        && (pucky() > 0xD0 || pucky() < -0xD0)
        && o->Xpos <= 0x14 && o->Xpos >= -0x14 && !(g.gameclock & 2))
        goto save;
    if (rd16(pc + 2) > 0x22) goto de5;                    /* not reaching the line soon (or never) */
    if ((int16_t)rd16(pc) > 0x18 || (int16_t)rd16(pc) < -0x18) {   /* wide of the net */
        if (puckc() >= 0 || (g.iflags & 4)) goto de5;
        {
            uint8_t *a1 = team_struct(0), *a2 = team_struct(1);
            if (PFLAG(o, pfteam)) { uint8_t *t = a1; a1 = a2; a2 = t; }
            if ((int32_t)rd32(a1 + tmneardist) < 0x1324) goto de5;
            if ((int32_t)rd32(a2 + tmneardist) < 0x9C4) goto de5;
        }
        if (pucky() <= 0xE0 && pucky() > -0xE0) goto de5;
        {
            int n = puckvy() < 0;
            if (PFLAG(o, pfgoal)) n = !n;
            if (n) goto de5;                              /* moving away from the net */
            int16_t vx = puckvx(), vy = puckvy();
            if (vx < 0) vx = (int16_t)-vx;
            if (vy < 0) vy = (int16_t)-vy;
            if (vy < vx) goto de5;
        }
        assinsert(0xF, o);                                /* go get it behind the net */
        return;
    }
    if (rd16(pc + 2) > 0xC) goto twenty;
save:
    if (pucky() <= 0x108 && pucky() >= -0x108 && !(o->pflags2 & 2)) {
        o->pflags2 |= 2;
        goaliesave(pc, d3, o);
    }
twenty:
    d0 = (int16_t)rd16(pc);
    if (pucky() > 0xFC || pucky() <= -0xFC)
        d0 = (int16_t)g.TmpPuckX >= 0 ? 0x18 : -0x18;
de5:
    d1 = d2;
    d0 = (int16_t)(-(int16_t)HIB(o->Xvel) + d0 - o->Xpos);
    d1 = (int16_t)(-(int16_t)HIB(o->Yvel) + d1 - o->Ypos);
    if (d0 <= 4 && d0 >= -4 && d1 <= 4 && d1 >= -4) { d0 = 0; d1 = 0; }
    d0 = (int16_t)vtoa(d0, d1);
    SET_LOB(o->temp2, d0);
nodec:
    d2 = LOB(o->temp2);
    if (d2 <= 7) playeracc(d2, o);
    else stopna(o);
}

/* checks94.asm:ClampYPosition. d1 = y clamped to +-$103, minus goal line d3. d0 = 0 if a3 has
 * the puck. */
void ClampYPosition(int16_t *d0, int16_t *d1, int16_t goalline, const struct SortObj *o)
{
    if (puckc() == o->SCnum) *d0 = 0;
    if (*d1 >= 0x103) *d1 = 0x103;
    if (*d1 <= -0x103) *d1 = -0x103;
    *d1 = (int16_t)(*d1 - goalline);
}

/* checks94.asm:AdjustFacingDirection. Turn facedir one step toward direction d0. A computer
 * goalie facing 1 or 6 cannot turn through the directions away from his net. */
void AdjustFacingDirection(int16_t dir, struct SortObj *o)
{
    int16_t d1 = o->facedir;
    int16_t d0 = (int16_t)(dir - d1);
    if (d0 == 0) return;
    d0 = (int16_t)(((-d0) & 4) >> 1);
    d0 = (int16_t)(d0 - 1);                               /* +1 / -1 */
    if (!PFLAG(o, pfjoycon) && ((0x42 >> (d1 & 7)) & 1)) {
        d1 = (int16_t)(d1 + d0);
        unsigned mask = PFLAG(o, pfgoal) ? 0x38 : 0x83;
        if ((mask >> (d1 & 7)) & 1)
            d1 = (int16_t)(d1 - d0 - d0);                 /* turn the other way */
        o->facedir = (int16_t)(d1 & 7);
        return;
    }
    o->facedir = (int16_t)((d1 + d0) & 7);
}

/* checks94.asm:goaliesave. Set the save animation for goalie a3 from where the puck will cross
 * (a0 = puckcross entry, d3 = his goal line): the direction of the shot relative to his facing
 * picks the side, the height and distance pick the type. Slows him down. */
void goaliesave(const uint8_t *pcross, int16_t goalline, struct SortObj *gk)
{
    int16_t d0 = (int16_t)(rd16(pcross) - gk->Xpos);
    int16_t d1 = (int16_t)(goalline - gk->Ypos);
    d0 = (int16_t)((vtoa(d0, d1) - gk->facedir) & 7);
    int16_t d3 = d0;
    d0 = (int16_t)(d0 >> 2);
    if (gk->attribute & HI(3)) d0 ^= 1;
    if (puckz() > 8 || puckvz() > 0x800) {                /* a high shot */
        int16_t dx = (int16_t)(gk->Xpos - rd16(pcross));
        if (dx <= 0x10 && dx >= -0x10) d0 = (int16_t)(d0 + 6);
    } else {
        d0 = (int16_t)(d0 + 4);
        if (rd16(pcross + 2) <= 8) {                      /* arriving now */
            int16_t dx = (int16_t)(gk->Xpos - rd16(pcross));
            if (dx > 0x10 || dx <= -0x10) d0 = (int16_t)(d0 + 4);
        } else if (gk->facedir != 2 && gk->facedir != 6 && puckc() >= 0) {
            d0 = (int16_t)(d0 - 2);
            gk->Xvel = 0x1000;
            d0 |= 1;
            if (!PFLAG(gk, pfgoal)) d0 ^= 1;
            int16_t dx = -6;
            if (gk->Xvel >= 0) gk->Xvel = (int16_t)-gk->Xvel;
            if (gk->Xpos < 0) {
                if (gk->Xvel < 0) gk->Xvel = (int16_t)-gk->Xvel;
                dx = 6;
                d0 ^= 1;
            }
            gk->Xpos = (int16_t)(gk->Xpos + dx);
            if (gk->handed[0] & 1) d0 ^= 1;               /* left handed goalie */
        }
    }
    d1 = (int16_t)saveanim[d0];
    if (d1 == SPAgglovel && (d3 & 3) != 0 && (int8_t)gk->aggress >= 0xB)
        d1 = SPAgglovel2;                                 /* reach out glove save */
    SetSPA(gk, (uint16_t)d1);
    g.crowdlevel += 0x96;
    g.CwdExciteLvl += 0xA;
    gk->Xvel = (int16_t)(gk->Xvel >> 2);
    gk->Yvel = (int16_t)(gk->Yvel >> 2);
}

/* checks94.asm:assgoalietopuck. The goalie skates to a loose puck coming his way when no
 * skater of either team is close to it. */
void assgoalietopuck(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfjoycon)) { assexit(o); return; }
    if (g.gmode & (1 << gmclock)) { assexit(o); return; }
    if (check4bench(o)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = 0;
    }
    if (TICK_HIB(o->temp1) < 0) {
        goalie_timer_reload(o);
        if (puckc() >= 0) { assexit(o); return; }
        int n = puckvy() < 0;
        if (PFLAG(o, pfgoal)) n = !n;
        if (n) { assexit(o); return; }
        uint8_t *a1 = team_struct(0), *a2 = team_struct(1);
        if (PFLAG(o, pfteam)) { uint8_t *t = a1; a1 = a2; a2 = t; }
        if ((int32_t)rd32(a1 + tmneardist) < 0xE10) { assexit(o); return; }
        if ((int32_t)rd32(a2 + tmneardist) < 0x640) { assexit(o); return; }
    }
    skatetopuck(o);
}

/* checks94.asm:breakaway. Puck carrier a3 on a breakaway: beep on a new assignment and check
 * the skating direction toward the net. Returns the N flag: set when he is not skating at it. */
int breakaway(struct SortObj *o)
{
    g.sflags5 |= 4;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        sfx(SYM_SFXbeep1);
        breakaway_crowd(o);
    }
    if (o->Yvel == 0) return 1;
    return (int16_t)(o->Ypos ^ o->Yvel) < 0;
}

/* checks94.asm:BreakawayOffsidesFlagSet. On a new assignment clear the offsides and breakaway
 * bits of a3. Behind the goal line or not yet in the attack zone set pf3oside; in the zone with
 * no defender between him and the net set pf3break. Returns d0 = 1 for a breakaway. */
int BreakawayOffsidesFlagSet(struct SortObj *o)
{
    if (PFLAG(o, pfna))
        o->pflags3 &= (uint8_t)~((1 << pf3oside) | (1 << pf3break));
    int16_t d1 = o->Ypos;
    if (d1 < 0) d1 = (int16_t)-d1;
    if (d1 >= 0x108) return 0;                            /* behind the goal line */
    d1 = o->Ypos;
    int inzone;
    if (PFLAG(o, pfgoal)) inzone = 0x58 < d1;
    else inzone = !(-0x58 < d1);
    if (!inzone) {
        o->pflags3 |= 1 << pf3oside;
        return 0;
    }
    int was = o->pflags3 & (1 << pf3oside);
    o->pflags3 &= (uint8_t)~(1 << pf3oside);
    if (!was) return 0;                                   /* on the blue line */
    const struct SortObj *a0 = sortobj(11);
    for (int i = 0; i < 12; i++, a0--) {                  /* any skater of either team nearer the net? */
        if (d1 < 0) { if (d1 <= a0->Ypos) continue; }
        else { if (d1 >= a0->Ypos) continue; }
        if (a0->position == 0) continue;                  /* goalies do not count */
        return 0;
    }
    o->pflags3 |= 1 << pf3break;
    return 1;
}

/* checks94.asm:chkpuckc. Puck carrier a3: check for a breakaway (not in a shootout or penalty
 * shot), else assign asspuckc. */
void chkpuckc(uint8_t *tm, struct SortObj *o)
{
    if (!(g.gmode2 & HI(0)) && !(g.BA_PS_flags & HI(2)) && !breakaway(o)) {
        asspuckc(tm, o);                                  /* still skating at the net in y */
        return;
    }
    o->pflags3 &= (uint8_t)~(1 << pf3break);
    assreplace(0x10, o);                                  /* asspuckc */
    asspuckc(tm, o);
}

/* the a0 callback of skateto in asspuckc: count the opponents converging on the puck (threat)
 * and turn away from one that is right in front */
uint16_t asspuckc_chkdir(uint16_t dir, struct SortObj *o)
{
    int16_t d0 = (int8_t)dir;
    int16_t d2 = (int16_t)(HIB(o->Xvel) + puckx());
    int16_t d3 = (int16_t)(HIB(o->Yvel) + pucky());
    g.threat = 0;
    const struct SortObj *a0 = opponents(o);
    for (int i = 0; i < 6; i++, a0++) {
        int16_t d1 = (int16_t)(HIB(a0->Xvel) + a0->Xpos - d2);
        if (d1 > 0x14 || d1 < -0x14) continue;
        d1 = (int16_t)(HIB(a0->Yvel) + a0->Ypos - d3);
        if (d1 > 0x14 || d1 < -0x14) continue;
        g.threat++;
        d0 = (int16_t)vtoa((int16_t)(o->Xpos - a0->Xpos), (int16_t)(o->Ypos - a0->Ypos));
        d1 = (int16_t)(o->facedir ^ 4);
        if (d1 != d0) continue;
        d1 = (int16_t)(vdp_hv16() & 1);
        d0 = (int16_t)((d0 + d1) & 7);
    }
    return (uint16_t)d0;
}

/* checks94.asm:asspuckc. Player a3 is the puck handler: carry it up the ice to a spot from
 * .postab2 (the shootout path instead), looking for a shot or a pass now and then. */
void asspuckc(uint8_t *tm, struct SortObj *o)
{
    if (g.sflags5 & 4) g.sflags5 &= (uint8_t)~4;
    else o->pflags3 &= (uint8_t)~(1 << pf3break);
    if (puckc() != o->SCnum) { assexit(o); return; }
    if ((g.BA_PS_flags & HI(2)) && (int16_t)g.msgtimer > 1) return;
    if (!(o->pflags3 & (1 << pf3break)) && !(g.BA_PS_flags & HI(2)) && BreakawayOffsidesFlagSet(o))
        assreplace(0x21, o);                              /* chkpuckc */
    if (PFLAG(o, pfalock)) return;
    if (!(g.BA_PS_flags & HI(2)) && (g.gmode & (1 << gmclock))) { donothing(o); return; }
    if (PFLAG(o, pfjoycon)) { assexit(o); return; }
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        if (o->pflags3 & (1 << pf3break)) sfx(SYM_SFXbeep1);
        if (o->pflags3 & (1 << pf3break)) breakaway_crowd(o);
        o->temp1 = 0;
        o->temp2 = 8;
        o->temp3 = (int16_t)(vdp_hv16() & 3);
    }
    if (TICK_HIB(o->temp1) < 0) {
        SET_HIB(o->temp1, o->aioff);
        if ((ReadGoaliePulled(o) & CCR_N) || ((g.sflags7 & 0x40) && HIB(o->temp1) != 0))
            SET_HIB(o->temp1, HIB(o->temp1) - 1);
        if ((g.BA_PS_flags & HI(2)) || (g.gmode2 & HI(0))) {
            if (!ShootoutShootCheck(o)) goto nodec;
            CompShoot(o);                                 /* ends the assignment */
            return;
        }
        checkob(o);
        if (chk4lc(tm, o)) return;
        if ((g.gmode2 & HI(0)) || (g.BA_PS_flags & HI(2))) {
            if (!ShootoutShootCheck(o)) goto nodec;
            CompShoot(o);
            return;
        }
        int shot = 1;
        if (!(o->pflags3 & (1 << pf3break))) {
            uint16_t d0 = PFLAG(o, pfteam) ? 2 : 1;
            if (d0 != g.cont1team && d0 != g.cont2team && (g.vcount & 2))
                shot = 0;
        }
        if (shot && chk4shot(o)) return;
        if (chk4pass(o)) return;
    }
nodec:;
    int16_t d0 = (int16_t)(6 + o->temp3);
    if (g.sflags2 & HI(sf2offsig)) d0 = o->position;
    int16_t d1 = (int16_t)puckc_postab[d0 * 2 + 1];
    d0 = (int16_t)puckc_postab[d0 * 2];
    int path = (g.BA_PS_flags & HI(2)) || (g.gmode2 & HI(0));
    if (path) {
        SkatePath(&d0, &d1, o);
        if ((g.sopathx >> 8) == 0x80) return;
    }
    if (!PFLAG(o, pfgoal)) { d0 = (int16_t)-d0; d1 = (int16_t)-d1; }
    skateto(d0, d1, path ? NULL : asspuckc_chkdir, o);
}

/* checks94.asm:chk4lc. See if the computer should call a line change: with the puck in the
 * neutral zone, now and then, when the line's energy is low. Returns 1 when it did (CompShoot
 * ends the puck carrier's assignment). */
int chk4lc(uint8_t *tm, struct SortObj *o)
{
    if (g.sflags7 & 0x10) return 0;
    int16_t d0 = pucky();
    if (!PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
    if (d0 < 0) return 0;
    if (d0 > 0x58) return 0;
    if (vdp_hv16() & 3) return 0;
    if (o->pflags2 & 8) return 0;
    uint8_t *a2 = team_struct(0), *a1 = team_struct(1);
    if (PFLAG(o, pfteam)) { uint8_t *t = a2; a2 = a1; a1 = t; }
    if (AvgCline(a2) > 0xC00) return 0;
    CompLine(a2, a1);
    setpersonel(a2);
    printscores1();
    CompShoot(o);
    return 1;
}

/* checks94.asm:chkpk. Z set if killing a penalty. */
int chkpk(struct SortObj *o)
{
    if (g.sflags2 & HI(sf2pwrplay)) return chkpk2(o);
    return 0;
}

/* checks94.asm:chkpk2. Z set when sf2pwrtm (team on the power play) differs from pfteam. */
int chkpk2(struct SortObj *o)
{
    return ((g.sflags2 & HI(sf2pwrtm)) != 0) != (PFLAG(o, pfteam) != 0);
}

/* checks94.asm:chk4shot. Computer puck carrier a3 looks for a shot. Returns 1 when he shoots
 * (CompShoot ends the assignment). */
int chk4shot(struct SortObj *o)
{
    if (chkpk(o) && g.threat != 0 && (vdp_hv16() & 3) == 0) {   /* on the power play, pressured */
        int16_t d0 = 0x58;
        if (PFLAG(o, pfgoal)) {
            d0 = -0x58;
            if (d0 < o->Ypos) { CompShoot(o); return 1; } /* clear the puck */
        } else if (d0 > o->Ypos) { CompShoot(o); return 1; }
        g.sflags5 |= 8;
        dopass(o);
        return 0;
    }
    if (o->pflags3 & (1 << pf3break)) {
        int16_t d0 = (int16_t)(randomd0(0x1E) + 0x82);
        int shoot = d0 < pucky();
        if (!shoot) { d0 = (int16_t)-d0; shoot = d0 > pucky(); }
        if (shoot) {
            d0 = (int16_t)(randomd0s(2) + o->facedir);
            o->facedir = (int16_t)(d0 & 7);
            CompShoot(o);
            return 1;
        }
    }
    uint16_t d4 = (uint16_t)((0x20 - (o->spodds >> 1)) & 0xFF);
    d4 = (uint16_t)(d4 << 4);
    int16_t d1 = 0x108;
    if (!PFLAG(o, pfgoal)) d1 = (int16_t)-d1;
    d1 = (int16_t)(d1 - pucky());
    int16_t d0 = (int16_t)-puckx();
    if ((uint32_t)(sq(d0) + sq(d1)) <= 0x2710) {          /* close to the net: odds per open lane */
        d4 = (uint16_t)(d4 >> 4);
        int16_t d5 = (int16_t)vtoa(d0, d1);
        const struct SortObj *a1 = opponents(o);
        for (int d3 = 5; d3 >= 0; d3--, a1++) {
            if (a1->position != 0) {
                if (vtoa((int16_t)(a1->Xpos - puckx()), (int16_t)(a1->Ypos - pucky())) == (uint16_t)d5)
                    d4 = (uint16_t)(d4 << 1);             /* a defender in the lane */
            } else if (a1->pflags2 & 2) {
                d3 = 0;                                   /* the goalie is down: shoot */
                d4 = 1;
            }
        }
    }
    d0 = (int16_t)randomd0(d4);
    if (o->spodds & 1) { if (d0 > 7) return 0; }
    else if (d0 > 8) return 0;
    d0 = pucky();
    if (!PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
    if (d0 < 0) return 0;
    d1 = (int16_t)(0x108 - d0);
    if (d1 < 0) return 0;
    if (g.sflags2 & HI(sf2offsig)) return 0;
    CompShoot(o);
    return 1;
}

/* checks94.asm:CompShoot. Computer shot: drop the return address (the caller's assignment
 * ends) and aim from the puck's distance to the goal line. */
void CompShoot(struct SortObj *o)
{
    int16_t d0 = pucky();
    if (!PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
    uint16_t d1 = (uint16_t)(0x108 - d0) >> 3;
    if ((int16_t)d1 >= 0x14) d1 = 0x14;
    o->temp2 = (int16_t)d1;
    assreplace(0x12, o);                                  /* assshoot */
}

/* checks94.asm:chk4pass. Computer puck carrier a3 looks for a good pass to a random teammate.
 * Returns 1 when a skater passed (the assignment ends). */
int chk4pass(struct SortObj *o)
{
    if (g.threat == 0) {
        if (randomd0((uint16_t)((0x10 + o->spodds) & 0xFF)) > 0xC) return 0;
    }
    int16_t d0 = (int16_t)randomd0(6);
    if (o->SCnum >= 6) d0 = (int16_t)(d0 + 6);
    if (o->position != 0 && puckx() <= 0x28 && puckx() >= -0x28) {
        if (pucky() > 0xCC) return 0;                     /* in the slot: no pass */
        if (pucky() < -0xCC) return 0;
    }
    if (d0 == o->SCnum) return 0;
    const struct SortObj *a0 = sortobj(d0);
    if (a0->position <= 0) return 0;
    if (a0->pflags2 & 4) return 0;
    if (PFLAG(a0, pfalock)) return 0;
    d0 = a0->Ypos;
    int16_t d1 = o->Ypos;
    if (!PFLAG(o, pfgoal)) { d0 = (int16_t)-d0; d1 = (int16_t)-d1; }
    if (g.gmode & (1 << gmoffs)) {
        if ((int16_t)((int16_t)(d0 - 0x58) ^ (int16_t)(d1 - 0x58)) < 0) return 0;   /* across the blue line */
    }
    if (!(d0 > 0x58)) {
        d0 = (int16_t)(d0 - d1);
        if (d0 < -0xF) return 0;                          /* too far behind him */
    }
    int16_t dx = (int16_t)(a0->Xpos - puckx()), dy = (int16_t)(a0->Ypos - pucky());
    g.passdir = vtoa(dx, dy);
    int32_t d2 = sq(dx) + sq(dy);
    const struct SortObj *a1 = opponents(o);
    for (int d3 = 5; d3 >= 0; d3--, a1++) {              /* an opponent nearer in that direction? */
        int16_t ox = (int16_t)(a1->Xpos - puckx()), oy = (int16_t)(a1->Ypos - pucky());
        if ((uint32_t)(sq(ox) + sq(oy)) > (uint32_t)d2) continue;
        if (vtoa(ox, oy) == g.passdir) return 0;
    }
    dopass(o);
    if (o->position == 0) return 0;
    assexit(o);
    return 1;
}

/* checks94.asm:EvadePC. Player a3 should avoid the puck carrier if he's on their team: with the
 * carrier close, skate away from him; with offsides on, along the blue line around the puck. */
uint16_t EvadePC(uint16_t dir, struct SortObj *o)
{
    if (puckc() < 0) return dir;
    int16_t d2 = (int16_t)(int8_t)((uint8_t)HIB(o->Xvel) - (uint8_t)HIB(puckvx()));
    d2 = (int16_t)(d2 + o->Xpos - puckx());
    if (d2 > 0x28 || d2 < -0x28) return dir;
    int16_t d1 = (int16_t)(int8_t)((uint8_t)HIB(o->Yvel) - (uint8_t)HIB(puckvy()));
    d1 = (int16_t)(d1 + o->Ypos - pucky());
    if (d1 > 0x28 || d1 < -0x28) return dir;
    uint16_t d0 = vtoa((int16_t)(o->Xpos - puckx()), (int16_t)(o->Ypos - pucky()));
    if (!(g.gmode & (1 << gmoffs))) return d0;
    d1 = o->Ypos;
    if (!PFLAG(o, pfgoal)) d1 = (int16_t)-d1;
    d1 = (int16_t)(d1 - 0x58);
    if (d1 > 0xA) return d0;
    if (d1 < -50) return d0;
    if (o->Xpos > puckx()) return 2;
    return 6;
}

/* checks94.asm:checkob. Bring up the little warning ref when a player on a3's team is over
 * the blue line ahead of the puck (offsides on). */
void checkob(struct SortObj *o)
{
    if (!(g.gmode & (1 << gmoffs))) return;
    if (g.BA_PS_flags & HI(2)) return;
    const struct SortObj *a0 = teammates(o);
    int16_t d1 = 0x5C;
    int over = 0;
    if (!PFLAG(o, pfgoal)) {
        d1 = (int16_t)-d1;
        if (!(d1 > pucky()))
            for (int i = 0; i < 6; i++, a0++)
                if (a0->position >= 0 && d1 > a0->Ypos) { over = 1; break; }
    } else if (!(d1 < pucky()))
        for (int i = 0; i < 6; i++, a0++)
            if (a0->position >= 0 && d1 < a0->Ypos) { over = 1; break; }
    uint16_t d0;
    if (over) {
        d0 = 6;
        if (g.sflags2 & HI(sf2offsig)) return;
        g.sflags2 |= HI(sf2offsig);
    } else {
        d0 = 0x40;                                        /* clears the ref window */
        if (!(g.sflags2 & HI(sf2offsig))) return;
        g.sflags2 &= (uint16_t)~HI(sf2offsig);
    }
    if ((int16_t)g.RefCnt >= 0) return;
    PushRef(d0);
}

/* checks94.asm:assbreakaway. Assignment for a breakaway: the carrier keeps it while he skates
 * at the net, everyone else (and the carrier once he turns) goes to assnearest. */
void assbreakaway(uint8_t *tm, struct SortObj *o)
{
    if (o->SCnum == puckc() && !breakaway(o)) {
        assnearest(tm, o);
        return;
    }
    o->pflags3 &= (uint8_t)~(1 << pf3break);
    assreplace(0x11, o);                                  /* assnearest */
    assnearest(tm, o);
}

/* checks94.asm:assnearest. A special assignment used for the player who is nearest the puck
 * but doesn't have it: find his team's nearest skater (tmneardist), hand the assignment to him
 * if it is someone else, else chase the puck (or the carrier) and look for a check. */
void assnearest(uint8_t *tm, struct SortObj *o)
{
    if (g.sflags5 & 4) g.sflags5 &= (uint8_t)~4;
    else o->pflags3 &= (uint8_t)~(1 << pf3break);
    if (o->SCnum == puckc() && !(o->pflags3 & (1 << pf3break)) && !(g.BA_PS_flags & HI(2))
        && BreakawayOffsidesFlagSet(o))
        assreplace(0x22, o);                              /* assbreakaway */
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        if (o->pflags3 & (1 << pf3break)) sfx(SYM_SFXbeep1);
        if (o->pflags3 & (1 << pf3break)) breakaway_crowd(o);
        o->temp3 = 0;
        o->temp2 = 8;
        o->temp1 = 0;
    }
    if (o->SCnum == puckc()) {                            /* he picked it up */
        if (o->position == 0) { assgoaliecpu(tm, o); return; }
        if (!PFLAG(o, pfjoycon)) assinsert(0x10, o);      /* asspuckc */
        return;
    }
    int16_t d0, d1;
    int32_t d2;
    if (TICK_HIB(o->temp1) < 0) {
        SET_HIB(o->temp1, o->aioff);
        if ((ReadGoaliePulled(o) & CCR_N) || ((g.sflags7 & 0x40) && HIB(o->temp1) != 0))
            SET_HIB(o->temp1, HIB(o->temp1) - 1);
        if (g.sflags6 & HI(5)) {                          /* the carrier is in the slot: react twice as fast */
            int away_carrier = puckc() > 5;
            if (away_carrier != (PFLAG(o, pfteam) != 0)) {
                d1 = (int16_t)(HIB(o->temp1) >> 1);
                SET_HIB(o->temp1, d1);
                if ((uint8_t)d1 == 0) SET_HIB(o->temp1, 1);
            }
        }
    wr32(tm + tmneardist, 0);
    struct SortObj *a1 = NULL;
    d2 = (int32_t)assign_d2;                              /* (the caller's d2, as the 68k leaves it below) */
    d1 = puckc();
    if (d1 >= 0 && !((sortobj(d1)->pflags ^ o->pflags) & (1 << pfteam)))
        a1 = sortobj(d1);                                 /* our carrier is the nearest */
    else {
        d2 = -1;
        struct SortObj *a0 = sortobj_at(rd16(tm + tmsort));
        for (int i = 0; i < 6; i++, a0++) {
            if (a0->position <= 0) continue;
            if (a0->nopuck[0] != 0) continue;
            if (a0->pflags2 & 4) continue;
            if (a0->asslist[a0->assnum] == 0x13) continue;   /* going to receive a pass */
            int16_t hx, hy;
            GetHot(a0, &hx, &hy);
            d0 = (int16_t)(hx + a0->Xpos - puckx());
            d1 = (int16_t)(hy + a0->Ypos - pucky());
            d0 = (int16_t)(d0 - (puckvx() >> 6));
            d1 = (int16_t)(d1 - (puckvy() >> 6));
            int32_t dsq = sq(d0) + sq(d1);
            if ((uint32_t)dsq > (uint32_t)d2) continue;
            d2 = dsq;
            a1 = a0;
        }
        if (d2 < 0) goto de1;                             /* nobody can take it */
        wr32(tm + tmneardist, (uint32_t)d2);
    }
    if (a1 != o && !(g.gmode & (1 << gmclock)) && !(a1->pflags2 & 4) && !(a1->pflags3 & (1 << pf3onetimer))) {
        a1->pflags &= (uint8_t)~(1 << pfdoff);
        assinsert(0x11, a1);                              /* he is nearest now */
        assexit(o);
        return;
    }
de1:
    if (PFLAG(o, pfalock)) return;
    {
        int16_t sh = 2;
        int far = (uint16_t)d2 > 0x190;
        if (!far) sh = 0;
        if (far || (uint16_t)o->position > 2) {           /* defensemen stay back when close */
            if (g.sflags2 & HI(sf2pwrplay)) {
                sh = (int16_t)(sh - 2);
                if (chkpk2(o)) sh = (int16_t)(sh + 4);
            }
            uint16_t odds = (uint16_t)((0x28 - o->checking) & 0xFF);
            unsigned cnt = (unsigned)sh & 63;
            odds = cnt >= 16 ? 0 : (uint16_t)(odds << cnt);
            if (randomd0(odds) <= 2)
                o->temp3 = 0xF0;                          /* chase the carrier for a while */
        }
    }
    }                                                     /* end of the timer block (bpl .nodec) */
    if (PFLAG(o, pfalock)) return;
    if (!(g.BA_PS_flags & HI(2)) || (g.BA_PS_flags & HI(5))) {
        if (g.gmode & (1 << gmclock)) { donothing(o); return; }
    }
    if (PFLAG(o, pfjoycon)) return;
    if (!(tm[tmflags] & 4)) {
        if (puckc() < 0) goto topuck;
        o->temp3 = (int16_t)(o->temp3 - elapsed);
        if (o->temp3 >= 0) goto topuck;
        o->temp3 = 0;
    }
    skatetopuckinit(&d0, &d1);
    {
        int16_t fx = d0, fy = d1;
        d0 = (int16_t)-d0;
        d1 = (int16_t)(-d1 + 0xF4);                       /* between the puck and his own net */
        if (PFLAG(o, pfgoal)) d1 = (int16_t)(d1 - 0x1E8);
        d0 = (int16_t)((d0 >> 1) + fx);
        d1 = (int16_t)((d1 >> 1) + fy);
    }
    if (tm[tmflags] & 4) {
        if (PFLAG(o, pfgoal)) { if (d1 >= 0x53) d1 = 0x44; }
        else if (d1 <= -0x53) d1 = -0x44;
    }
    if (g.sflags6 & HI(5)) {                              /* the carrier is in the slot: a burst of speed */
        g.TempLegSpd = o->legspd;
        o->legspd = (uint8_t)(o->legspd + 6);
        if ((g.sflags8 & 2) || (g.sflags7 & 0x40)) o->legspd = (uint8_t)(o->legspd + 2);
        if ((int8_t)o->legspd > 0x1E) o->legspd = 0x1E;
        skateto(d0, d1, NULL, o);
        o->legspd = g.TempLegSpd;
    } else
        skateto(d0, d1, NULL, o);
    if (g.sflags6 & HI(5)) {
        d0 = pucky();
        int16_t d2w = o->Ypos;
        if (d0 < 0) { d0 = (int16_t)-d0; d2w = (int16_t)-d2w; }
        d2w = (int16_t)(d2w - d0);
        if (d2w < 0xA) return;                            /* not between the net and the carrier */
        d0 = (int16_t)(o->Xpos - puckx());
        if (d0 < 0) d0 = (int16_t)-d0;
        if (d0 >= 0xF) return;                            /* nor in line with him */
        d0 = o->checking;
        uint8_t saved = o->checking;
        int16_t dbl = (int16_t)(int8_t)(d0 + d0);
        if ((int8_t)dbl >= 0x1E) dbl = 0x1E;
        o->checking = (uint8_t)dbl;                       /* twice as eager to check */
        check4check(o);
        o->checking = saved;
        return;
    }
    check4check(o);
    return;
topuck:
    skatetopuck(o);
    check4check(o);
}

/* checks94.asm:check4check. Look for a good opportunity to check an opponent: now and then
 * (less with penalties on), an opponent within $1E in the direction he faces. */
void check4check(struct SortObj *o)
{
    if (o->position == 0) return;
    uint16_t d0 = (uint16_t)((0x28 - o->checking) & 0xFF);
    if (g.OptPen != 0) d0 = (uint16_t)(d0 << 1);
    if (randomd0(d0) > 6) return;
    struct SortObj *a0 = opponents(o);
    for (int i = 0; i < 6; i++, a0++) {
        if (a0->position == 0) continue;
        if (PFLAG(a0, pfalock)) continue;
        if (a0->pflags2 & (1 << pf2fight)) continue;
        int16_t dx = (int16_t)(a0->Xpos - o->Xpos);
        if (dx > 0x1E || dx < -0x1E) continue;
        int16_t dy = (int16_t)(a0->Ypos - o->Ypos);
        if (dy > 0x1E || dy < -0x1E) continue;
        if (vtoa(dx, dy) != (uint16_t)o->facedir) continue;
        if (g.sflags6 & HI(5)) { Acheck(a0, o); return; }
        if (vdp_hv16() & 3) { burst(o); return; }
        Acheck(a0, o);
        return;
    }
}

/* checks94.asm:asspassrec. Assignment for catching a pass: coast until the puck is loose,
 * then either wait for it or (not a human team, puck on the attack half) go for a one-timer. */
void asspassrec(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (g.gmode & (1 << gmclock)) { donothing(o); return; }
    if (PFLAG(o, pfjoycon)) { assexit(o); return; }
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->pflags |= 1 << pfdoff;                         /* no deceleration */
        o->temp2 = 8;
        o->temp4 = -2;
    }
    if (puckc() >= 0) goto exit;
    o->temp4 = (int16_t)(o->temp4 + 1);
    if (o->temp4 <= 0) {
        if ((int16_t)g.onetimerplayer >= 0) goto ex2;
        if (o->position == 0) goto ex2;
        o->temp4 = 0;
        uint16_t d0 = PFLAG(o, pfteam) ? 2 : 1;
        if (d0 == g.cont1team || d0 == g.cont2team) goto ex2;
        if (PuckOnAttackHalf(o)) goto ex2;
        int16_t y = o->Ypos;
        if (!PFLAG(o, pfgoal)) y = (int16_t)-y;
        if (y < 0x58 || y > 0x108) {                      /* outside the attack zone: only sometimes */
            if (randomd0(8) != 0) goto ex2;
        }
        g.inputjoy = 0xFFFF;
        assreplace(0x23, o);                              /* assonetimer */
        return;
    }
ex2:
    if (TICK_HIB(o->temp1) >= 0) return;
exit:
    o->pflags &= (uint8_t)~(1 << pfdoff);
    assexit(o);
}

/* checks94.asm:SkateToTempTarget. Unreferenced: skate to temp3 / temp4. */
void SkateToTempTarget(struct SortObj *o)
{
    skateto(o->temp3, o->temp4, NULL, o);
}

/* checks94.asm:assshoot. Assignment for computer shooting: wind up (SetShotMode) and release
 * (ShotMode with the C button) when temp2 runs out. */
void assshoot(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        SetShotMode(o);
        return;
    }
    if (!(g.sflags & HI(sfssdir))) { assexit(o); return; }
    uint16_t d2 = 0;
    o->temp2 = (int16_t)(o->temp2 - elapsed);
    if (o->temp2 < 0) d2 |= 0x20;                         /* cbut */
    ShotMode((uint16_t)o->assnum, d2, o);                /* d0 is the assignment index at the call */
}

/* checks94.asm:puckshootout. Puck logic for the shootout: on a new assignment clear the pads
 * and select the skater, then run the attempt. */
void puckshootout(uint8_t *tm, struct SortObj *o)
{
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        g.sflags7 |= 0x80;
        g.padcont[0] = 0; g.padcont[1] = 0; g.padcont[2] = 0;
        g.gmode2 &= (uint16_t)~HI(7);
        if (g.gmode2 & HI(0))
            NextShooter(o);
        else {
            StartShootoutPath(o);
            uint8_t *t = team_struct(g.BA_Team != 0);
            wr16(t + tmsoattempts, (uint16_t)(rd16(t + tmsoattempts) + 1));
        }
        g.BA_PS_flags &= (uint16_t)~(HI(2) | HI(4) | HI(5) | HI(6));
        g.sflags6 |= HI(1);
        if (g.gmode & 8) { Stop4Pen(rom_ptr(SYM_puckshootout), o); return; }   /* a0 = this routine */
        o->pflags &= (uint8_t)~(1 << pfna);
        int play = 1;
        if (g.gmode2 & HI(0)) {
            if (g.shootoutteam != 0 || g.homeshootnum != 0) play = 0;
        }
        if (play) {
            g.HmTeam = g.HomeTeam;
            g.SongIndex = 2;
            ChooseSong();
            song((int16_t)g.SongNum);
        }
        g.gmode |= 1 << gmclock;
        g.gmode2 |= HI(1);
        g.puckvx = 0;
        g.puckvy = 0;
        g.shootoutclock = 0x19;
        ReturnGoalies();
        ST_W(o->temp1);
        ST_W(o->temp2);
    }
    g.BA_PS_flags |= HI(2);
    if (SelectPenaltyShotSkater()) {
        g.BA_PS_flags &= (uint16_t)~HI(2);
        assreplace(0x1B, o);                              /* puckfaceoff */
        return;
    }
    int16_t d0 = (int16_t)g.BA_Goalie_SCnum;
    const struct SortObj *gk = sortobj(d0);
    if (gk->position != 0 || (gk->pflags2 & 4)) {         /* find an available goalie on that side */
        for (int d2 = 5; d2 >= 0; d2--) {
            d0 = (int16_t)(d0 - 1);
            if (d0 < 0) d0 = 5;
            else if (d0 == 5) d0 = 0xB;
            gk = sortobj(d0);
            if (gk->position != 0) continue;
            if (gk->pflags2 & 4) continue;
            g.BA_Goalie_SCnum = (uint16_t)d0;
            goto found;
        }
        g.BA_PS_flags &= (uint16_t)~HI(2);
        assreplace(0x1B, o);
        return;
    }
found:
    assreplace(0x1F, o);                                  /* puckpenshot */
}

/* checks94.asm:SelectPenaltyShotSkater. Pick the skater for the penalty shot from team BA_Team:
 * the best rated (sum of the attribute nibbles) player on the ice or bench (any player in a
 * shootout). Returns the N flag: set when nobody is eligible. Registers are restored. */
int SelectPenaltyShotSkater(void)
{
    const uint8_t *a0 = team_struct(g.BA_Team != 0);
    const uint8_t *a2 = rom_ptr(rd32(a0 + tmdata));
    a2 += rd16(a2);
    int16_t d6 = -1, d5 = -1;
    for (uint16_t d1 = 0; rd16(a2) != 2; ) {
        a2 += rd16(a2);                                   /* past the name */
        uint16_t d7 = (uint16_t)(d1 * 2);
        uint8_t d0 = a2[5] & 0xF;
        if ((int8_t)d0 > 1) {
            if (g.gmode2 & HI(0)) goto setskater;
            uint16_t st = rd16(a0 + tmpdst + d7);
            if (st == 0xFFFE || st == 0xFFFF) {           /* bench or ice */
                int16_t d3 = (int16_t)((a2[1] & 0xF) + (a2[2] & 0xF) + (a2[2] >> 4) + (a2[3] & 0xF) + (a2[3] >> 4)
                                       + (a2[5] & 0xF) + (a2[5] >> 4) + (a2[6] >> 4) + (a2[7] >> 4));
                if (d1 == g.BA_Skater_Offset) d3 = 0x7FFF;
                if (!(d3 < d6)) { d6 = d3; d5 = (int16_t)d1; }
            }
        }
        a2 += 8;
        d1++;
        if (!(d1 < 0x1A)) break;
    }
    if (d5 < 0) return 1;
    g.BA_Skater_Offset = (uint16_t)d5;
setskater:
    g.BA_Sktr_SCnum = g.BA_Team != 0 ? 6 : 0;
    return 0;
}

/* place every object for the faceoff / penalty shot: the two nets, the puck shadow, scroll to
 * the spot, reset the benches and lines */
static void reset_ice_objects(void)
{
    struct SortObj *net = sortobj(12);
    net->Xvel = 0; net->Yvel = 0; net->Xpos = 0; net->Ypos = 0x10C;
    net++;
    net->Xvel = 0; net->Yvel = 0; net->Xpos = 0; net->Ypos = -0x10C;
    struct SortObj *sh = sortobj(puckscnum + 1);
    sh->frame = SPFpuck;
    sh->SPA = 0;
    sh->attribute = 0;
    puck_obj()->attribute = 0;
    g.sflags &= (uint16_t)~HI(sfslock);
    for (int i = 0; i <= 0x64; i++) checkwindow();
    g.yleader = 0x3C;
    ResetBench();
}

/* checks94.asm:puckpenshot. Puck start for a penalty shot / shootout attempt: black out, put
 * the skater on the puck at center ice and the goalie in his net, everyone else off the ice. */
void puckpenshot(uint8_t *tm, struct SortObj *o)
{
    if (!PFLAG(o, pfna)) {                                /* resume play */
        g.gmode2 |= HI(2);
        assreplace(0x18, o);                              /* pucknorm */
        o->Xvel = 0;
        o->Yvel = 0;
        return;
    }
    o->pflags &= (uint8_t)~(1 << pfna);
    forceblack();
    while (g.disflags & HI(dfok)) hal_wait_vblank();      /* wait for dfok clear */
    if (g.crowdlevel > 0x258) {
        g.crowdlevel = 0x258;
        g.CwdExciteLvl += 0x14;
    }
    DecompressGraphicsPlain(SYM_RefsMap + 8, g.ExtraChars);
    g.disflags |= HI(3);
    g.sflags &= (uint16_t)~HI(0);
    g.sflags3 &= (uint8_t)~1;
    g.glovecords = 0;
    g.iflags = 0;
    ST_W(g.RefCnt);
    g.sflags2 &= (uint16_t)~HI(1);
    ST_W(g.passplayer);
    ClrHor();
    g.Vpos = 0;
    g.Hpos = 0;
    g.puckx[0] = 0;
    g.pucky = 0;
    g.puckz[0] = 0;
    g.puckvx = 0;
    g.puckvy = 0;
    g.puckvz[0] = 0;
    ST_W(g.puckc);
    reset_ice_objects();
    setpersonel(team_struct(0));
    SetupPenaltyShot(team_struct(0));
    setpersonel(team_struct(1));
    SetupPenaltyShot(team_struct(1));
    resetplstuff();
    struct SortObj *pl = sortobj(11);
    for (int d2 = 11; d2 >= 0; d2--, pl--) {
        if (d2 == (int16_t)g.BA_Sktr_SCnum) {
            if (pl->position < 0 && (pl->pflags2 & 4)) {
                pl->pflags2 &= (uint8_t)~4;
                setplayer(pl->pnum, pl);
            }
            pl->Xpos = puckx();
            int16_t y = pucky();
            if (!PFLAG(pl, pfgoal)) { y = (int16_t)(y + 0x10); pl->facedir = 4; }
            else { y = (int16_t)(y - 0x10); pl->facedir = 0; }
            pl->Ypos = y;
            pl->Xvel = 0; pl->Yvel = 0; pl->Zvel = 0;
            assinsert(0x11, pl);                          /* assnearest */
        } else if (d2 == (int16_t)g.BA_Goalie_SCnum) {
            if (g.gmode2 & HI(0)) {                       /* shootout: the goalie of the shooting order */
                pl->pnum = *(ram_ptr(RAM_BASE + RAM_OFF_shootoutteam) - 1);
                if (g.shootoutteam != 0) pl->pnum = *(ram_ptr(RAM_BASE + RAM_OFF_homeshootnum) - 1);
                setplayer(pl->pnum, pl);
            } else if (pl->position < 0 && (pl->pflags2 & 4)) {
                pl->pflags2 &= (uint8_t)~4;
                setplayer(pl->pnum, pl);
            }
            pl->pflags2 &= (uint8_t)~4;
            int16_t d1 = PFLAG(pl, pfgoal) ? 0xE5 : -0xE5;
            pl->Xpos = 0;
            pl->Ypos = d1;
            pl->Xvel = 0; pl->Yvel = 0; pl->Zpos = 0;
            pl->facedir = (int16_t)vtoa(puckx(), (int16_t)(pucky() - d1));
            pl->pflags2 &= (uint8_t)~4;
            pl->pflags &= (uint8_t)~(1 << pfalock);
            SetSPA(pl, SPAglide);
        } else {
            pl->Xpos = -0xF0;
            pl->Ypos = 0;
            pl->frame = 0;
            assinsert(0x20, pl);
        }
    }
    SprSort();
    ResetAndSelectPlayers(o);
    struct SortObj *gk = sortobj((int16_t)g.BA_Goalie_SCnum);
    gk->Xpos = 0;
    gk->Ypos = PFLAG(gk, pfgoal) ? -0xFA : 0xFA;
    uint16_t d1 = PFLAG(gk, pfteam) ? 2 : 1;
    if (d1 == g.cont1team) setc1player(g.BA_Goalie_SCnum);
    else if (d1 == g.cont2team) setc2player(g.BA_Goalie_SCnum);
    assreplace(0xE, gk);
    g.BA_PS_flags |= HI(7);
    g.sflags2 |= HI(2);
    g.msgtimer = 0x190;
    if (g.cont1team == 0 && g.cont2team == 0) g.msgtimer = 0x64;
    g.palcount = 0x18;
}

/* checks94.asm:UpdatePenaltyShotEnd. End the penalty shot when the goalie has the puck, the
 * puck is loose or the timer runs out. */
void UpdatePenaltyShotEnd(struct SortObj *o)
{
    if (g.puckc == g.BA_Goalie_SCnum) goto ended;
    if (!(g.BA_PS_flags & HI(5))) goto timer;
    if (!(g.gmode2 & HI(5))) {
        if (puckc() >= 0) goto countdown;
        g.gmode2 |= HI(5);
    }
    if (puckc() >= 0) goto ended;                         /* picked up again */
countdown:
    if ((int16_t)g.passmodetimer < 0) goto ended;
    g.passmodetimer--;
    goto timer;
ended:
    g.BA_PS_flags |= HI(4);
timer:
    if (g.shootoutclock == 0) g.BA_PS_flags |= HI(4);
    if (!(g.BA_PS_flags & HI(4))) return;
    if (g.BA_PS_flags & HI(6)) return;
    g.BA_PS_flags |= HI(6);
    AddPenalty2(0xA, o);
    EndPenaltyShotPlay();
}

/* checks94.asm:EndPenaltyShotPlay. Stop play after a penalty shot or shootout attempt: freeze
 * the window, stop the clock, clear the penalty shot flags. */
void EndPenaltyShotPlay(void)
{
    freezewindow();
    if (!(g.gmode2 & HI(0))) {
        g.replaydelay = 0xA;
        g.sflags2 |= HI(2);
    }
    g.gmode |= 1 << gmclock;
    g.gmode2 &= (uint16_t)~HI(2);
    g.BA_PS_flags &= (uint16_t)~(HI(2) | HI(3) | HI(5) | HI(4) | HI(6));
    sortobj((int16_t)g.BA_Sktr_SCnum)->newpnum = g.savednewpnum;
    CountShootoutGoals();
}

/* checks94.asm:puckfaceoff. This is where the action starts: on a new assignment settle the
 * period end, delayed penalties, pulled goalies and the song, then start the line changes and
 * wait for them (WaitForFaceoffLineChanges) before puckfaceoff2. */
void puckfaceoff(uint8_t *tm, struct SortObj *o)
{
    g.sflags8 &= (uint8_t)~0x10;
    if (g.gmode2 & HI(0)) {
        g.puckvx = 0;
        g.puckvy = 0;
        assreplace(0x1E, o);                              /* puckshootout */
        return;
    }
    if (!PFLAG(o, pfna)) { WaitForFaceoffLineChanges(o); return; }
    o->pflags &= (uint8_t)~(1 << pfna);
    if (g.sflags8 & 1) {
        g.sflags8 &= (uint8_t)~1;
        g.fox = 0;
        g.foy = 0;
    }
    g.sflags7 &= (uint8_t)~0x80;
    g.padcont[0] = 0; g.padcont[1] = 0; g.padcont[2] = 0;
    g.sflags7 &= (uint8_t)~0x40;
    g.sflags7 &= (uint8_t)~4;
    CheckScoreLeader();
    if ((int16_t)g.faceoffanim[0] >= 0)
        SetFaceoffAnim(g.faceoffanim[0]);
    else {
        uint16_t d0 = RandomFaceoffAnim();
        if ((int16_t)d0 >= 0) SetFaceoffAnim(d0);
    }
    g.sflags6 &= (uint16_t)~HI(1);
    g.gmode2 &= (uint16_t)~HI(1);
    if (g.gameclock == 0) { PeriodOver(); return; }
    if (g.gmode & 0x40) { PeriodOver(); return; }
    if (g.gsp == 3 && g.HmGoals[0] != g.AwGoals[0]) { clockcont_0(); return; }   /* overtime goal */
    if (g.gmode & (1 << gmpendel)) { Stop4Pen(rom_ptr(SYM_puckfaceoff), o); return; }   /* a0 = this routine */
    {
        uint16_t half = (uint16_t)((int16_t)g.PerTimeTotal >> 1);
        if (!(half <= g.gameclock) && !((int16_t)g.gameclock < 0x3C) && (g.sflags3 & 0x80)) {
            g.sflags3 &= (uint8_t)~0x80;                 /* once in the second half of a period */
            if ((g.gmode & (1 << gmclock)) && !(g.sflags8 & 0x40)) {
                g.HmTeam = g.HomeTeam;
                g.SongIndex = 2;
                ChooseSong();
                g.sflags8 |= 0x10;
            }
        }
    }
    ReturnGoalies();
    ST_W(o->temp1);
    ST_W(o->temp2);
    if (g.OptLine != 0) { WaitForFaceoffLineChanges(o); return; }
    team_struct(0)[tmflags] &= (uint8_t)~2;
    team_struct(1)[tmflags] &= (uint8_t)~2;
    for (int i = 0; i < 12; i++) sortobj(i)->pflags2 &= (uint8_t)~8;
    if ((g.cont1team | g.cont2team) != 0 && (g.sflags & HI(sfhor))) {
        forceblack();
        g.sflags |= HI(sfslock);
        ClrHor();
        g.palcount = 0x18;
        if (g.OptLine == 0 && (g.sflags7 & 0x10)) g.palcount = 0;
    }
    if ((int16_t)g.c1playernum >= 0) StartFaceoffLineChange(g.c1playernum, o);
    if ((int16_t)g.c2playernum >= 0) StartFaceoffLineChange(g.c2playernum, o);
    SetFaceoffComputerLine(2, team_struct(1), team_struct(0));
    WaitForFaceoffLineChanges(o);
}

/* checks94.asm:SetFaceoffComputerLine. Faceoff: start a line change for team d0 when the
 * computer controls it (line changes on). */
void SetFaceoffComputerLine(uint16_t team, uint8_t *a2, uint8_t *a1)
{
    if (g.OptLine != 0) return;
    if (!(g.sflags7 & 0x10)) {
        if (team == g.cont1team) return;
        if (team == g.cont2team) return;
    }
    CompLine(a2, a1);
    setpersonel(a2);
    printscores1();
    g.crowdnoisedelay = 0x2710;
}

/* checks94.asm:StartFaceoffLineChange. Start the line change box for team d0's sort object
 * before a faceoff; the puck's temp1/temp3 (home) or temp2/temp4 (away) time it. */
void StartFaceoffLineChange(uint16_t obj, struct SortObj *puck)
{
    struct SortObj *pl = sortobj((int16_t)obj);
    pl->pflags2 &= (uint8_t)~8;
    SetLCmode(pl);
    if (!(pl->pflags2 & 8)) return;
    if (PFLAG(pl, pfteam)) {
        puck->temp2 = 0x168;
        puck->temp4 = pl->SCnum;
    } else {
        puck->temp1 = 0x258;
        puck->temp3 = pl->SCnum;
    }
}

/* checks94.asm:WaitForFaceoffLineChanges. Run both teams' faceoff line change timers (temp1 /
 * temp2); continue when both have run out. */
void WaitForFaceoffLineChanges(struct SortObj *puck)
{
    UpdateFaceoffLineChangeTimer(0x40, puck);
    UpdateFaceoffLineChangeTimer(0x42, puck);
    if (puck->temp1 >= 0) return;
    if (puck->temp2 >= 0) return;
    SetFaceoffComputerLine(1, team_struct(0), team_struct(1));
    g.facelcwait = 0xFFFF;
    assreplace(0x1C, puck);                               /* puckfaceoff2 */
}

/* checks94.asm:UpdateFaceoffLineChangeTimer. Count down one team's faceoff line change timer
 * at offset d0 ($40 = temp1, $42 = temp2) of a3; the object number sits 4 bytes on. */
void UpdateFaceoffLineChangeTimer(uint16_t off, struct SortObj *puck)
{
    int16_t timer = off == 0x40 ? puck->temp1 : puck->temp2;
    int16_t obj = off == 0x40 ? puck->temp3 : puck->temp4;
    if (timer < 0) return;
    struct SortObj *pl = sortobj(obj);
    uint8_t *a2 = team_struct(PFLAG(pl, pfteam));
    if (!(pl->pflags2 & 8)) {
        ST_W(timer);
        if (off == 0x40) puck->temp1 = timer; else puck->temp2 = timer;
        SetLCmode2(a2);
        return;
    }
    timer = (int16_t)(timer - elapsed);
    if (off == 0x40) puck->temp1 = timer; else puck->temp2 = timer;
    if (timer >= 0) return;
    if (pl->pflags2 & 8) {
        lcfound(0, a2, pl);
        SetLCmode2(a2);
    }
}

/* the body of ChkGoalies for one team: d0 = team number, a2 = the team, a1 = the other */
static void chkgoalie(uint16_t d0, uint8_t *a2, uint8_t *a1)
{
    if ((int16_t)rd16(a2 + tmgoalie) < 0) return;         /* no goalie */
    int16_t d1 = puckc();
    if (d1 < 0) return;
    d1 = (int16_t)(d1 - 6);
    if (a2 != team_struct(0)) d1 = (int16_t)~d1;
    if (d1 >= 0) return;                                  /* the other team has the puck */
    if (g.gmode & (1 << gmpendel)) {
        a2[tmgoalie] = 0xFF;                              /* pull him on the delayed penalty */
        setpersonel(a2);
        return;
    }
    if (d0 == g.cont1team || d0 == g.cont2team) return;
    CPgoalie(pucky(), a1, a2);
}

/* checks94.asm:ChkGoalies. Computer pulls the goalie on a delayed penalty (or late in the
 * game when behind). */
void ChkGoalies(void)
{
    if (g.gmode & (1 << gmclock)) return;
    chkgoalie(1, team_struct(0), team_struct(1));
    chkgoalie(2, team_struct(1), team_struct(0));
}

static void returngoalie(uint16_t d0, uint8_t *a2, uint8_t *a1)
{
    if (rd16(a2 + tmgoalie) == 0xFFFF) return;            /* still no goalie */
    a2[tmgoalie] = 0;                                     /* the goalie returns */
    if (d0 == g.cont1team || d0 == g.cont2team) return;
    CPgoalie((int16_t)g.foy, a1, a2);
}

/* checks94.asm:ReturnGoalies. If the computer pulled his goalie, look to see if he should
 * return him (he stays out for a faceoff in the attack zone). */
void ReturnGoalies(void)
{
    returngoalie(1, team_struct(0), team_struct(1));
    returngoalie(2, team_struct(1), team_struct(0));
}

/* checks94.asm:CPgoalie. See if the computer should pull his goalie: third period, two goals
 * behind, under a minute left, play (d1 = y) in the attack half. */
void CPgoalie(int16_t y, uint8_t *a1, uint8_t *a2)
{
    if (g.gsp != 2) return;
    int16_t d0 = (int16_t)(rd16(a1 + tmscore) - rd16(a2 + tmscore));
    if (d0 < 0) return;
    if (d0 != 2) return;
    if ((int16_t)g.gameclock > 0x3C) return;
    const struct SortObj *a0 = sortobj_at(rd16(a2 + tmsort));
    if (!PFLAG(a0, pfgoal)) y = (int16_t)-y;
    if (y < 0) return;
    a2[tmgoalie] = 0xFF;
    setpersonel(a2);
}

/* checks94.asm:CompLine. Find a good line for the computer team a2 to switch to (a1 = the
 * other team): the power play / penalty kill line pair, else a rested line matched to the
 * opponent's. */
void CompLine(uint8_t *a2, uint8_t *a1)
{
    int16_t d1 = (int16_t)(rd16(a2 + tmap) - rd16(a1 + tmap));
    if (d1 != 0) {
        uint16_t d0 = 3;
        if (d1 < 0) d0 = 5;
        uint16_t e1 = getlinee(d0, a2);
        uint16_t e2 = getlinee((uint16_t)(d0 + 1), a2);
        if ((int16_t)e1 < (int16_t)e2) d0++;
        wr16(a2 + tmline, d0);
        return;
    }
    if (a2 == team_struct(0)) {
        const uint16_t *tab = homelines;
        if (g.gsp == 2) {
            int16_t d2 = (int16_t)rd16(a2 + tmscore);
            if (d2 != (int16_t)rd16(a1 + tmscore)) {
                tab += 7;
                if (!(d2 > (int16_t)rd16(a1 + tmscore))) tab += 7;
            }
        }
        uint16_t idx = rd16(a1 + tmline);
        uint16_t d0 = tab[idx];
        if (getlinee(d0, a2) > 0xC00) {
            wr16(a2 + tmline, tab[idx]);
            return;
        }
    }
    const uint16_t *tab = awaylines;
    if (g.gsp == 2) {
        int16_t d2 = (int16_t)rd16(a2 + tmscore);
        if (d2 != (int16_t)rd16(a1 + tmscore)) {
            tab += 3;
            if (!(d2 > (int16_t)rd16(a1 + tmscore))) tab += 3;
        }
    }
    int16_t d1c = 2;
    for (;;) {
        uint16_t d0 = *tab++;
        if (getlinee(d0, a2) > 0xC00) break;
        if (--d1c == -1) break;
    }
    wr16(a2 + tmline, *--tab);
}

/* checks94.asm:puckfaceoff2. Face off control logic and general setup for action: on a new
 * assignment black out, reset the rink and every object around the faceoff spot, draw the
 * faceoff window and pick the drop time; then count the drop time down (updatefaceoff) and
 * drop the puck (Endfaceoff). */
void puckfaceoff2(uint8_t *tm, struct SortObj *o)
{
    if (!PFLAG(o, pfna)) {
        o->temp1 = (int16_t)(o->temp1 - 1);
        if (o->temp1 >= 0) { updatefaceoff(o); return; }
        EndArenaAnim();
        Endfaceoff(o);
        return;
    }
    o->pflags &= (uint8_t)~(1 << pfna);
    g.BA_PS_flags &= (uint16_t)~(HI(2) | HI(5));
    g.sflags4 &= (uint16_t)~(HI(0) | HI(1));
    g.DelayedPen &= (uint8_t)~7;
    forceblack();
    while (g.disflags & HI(dfok)) hal_wait_vblank();      /* wait for dfok clear */
    uint16_t sr = cpu_get_sr();
    g.holdreset = 0x3C;
    if (g.fox == 0 && g.foy == 0) g.faceoffanim[0] = 0xFFFF;   /* center ice: no arena animation */
    else cpu_set_sr(0x2700);                              /* (masked until the end of the setup) */
    if (g.crowdlevel > 0x258) g.crowdlevel = 0x258;
    g.disflags |= HI(3);                                  /* dfclock */
    g.sflags &= (uint16_t)~HI(sfpz);
    g.sflags3 &= (uint8_t)~(1 << sf3llcs);                /* no line changes */
    g.glovecords = 0;
    g.iflags = 0;
    ST_W(g.RefCnt);
    g.puckcross2 = 0xFF;
    g.puckcross6 = 0xFF;
    g.sflags2 &= (uint16_t)~HI(sf2refref);
    g.sflags2 |= HI(sf2faceoff);
    g.sflags2 |= HI(sf2drec);
    ST_W(g.passplayer);
    ST_W(g.onetimerplayer);
    g.disflags |= HI(4);
    ClrHor();
    g.crowdnoisedelay = 0x2710;
    g.Vpos = 0;
    g.Hpos = 0;
    g.puckx[0] = g.fox;
    g.pucky = g.foy;
    ST_W(g.puckz[0]);                                     /* no visible puck */
    g.puckvx = 0;
    g.puckvy = 0;
    g.puckvz[0] = 0;
    ST_W(g.puckc);
    reset_ice_objects();
    setpersonel(team_struct(0));
    forcepldata(team_struct(0));
    setpersonel(team_struct(1));
    forcepldata(team_struct(1));
    resetplstuff();
    struct SortObj *pl = sortobj(0);
    for (int d2 = 11; d2 >= 0; d2--, pl++) {
        pl->Xpos = -0xF0;
        pl->Ypos = 0;
        pl->frame = 0;
        int16_t d1 = pl->position;
        if (d1 < 0) continue;
        if (d1 != 0) {
            uint16_t d0 = 0x16;                           /* assfaceoff */
            if (d1 == 4) {                                /* the center takes the draw */
                uint8_t *tc = own_team(pl);
                wr16(tc + tmlasttouch, 0);
                tc[tmlasttouch + 1] = pl->pnum;
                g.sflags8 &= (uint8_t)~0x80;
                tc[tmassist1] = 0xFF;
                tc[tmassist2] = 0xFF;
                tc[tmflags] &= (uint8_t)~8;
                d0 = 0x17;                                /* assfaceoffpl */
            }
            assinsert(d0, pl);
        }
        int16_t d4 = (int16_t)rd16(team_struct(PFLAG(pl, pfteam)) + tmap);   /* skaters on the ice 4-6 */
        d4 = (int16_t)((6 - d4) << 3);
        d4 = (int16_t)(fo_apl[d4 + d1] << 2);
        int16_t d0 = (int16_t)fo_ptab[d4 / 2];
        d1 = (int16_t)fo_ptab[d4 / 2 + 1];
        if (!PFLAG(pl, pfgoal)) { d0 = (int16_t)-d0; d1 = (int16_t)-d1; }
        if (pl->position != 0) {
            if (d4 <= 8) {
                if ((int16_t)(g.fox ^ d0) < 0)
                    d1 = (int16_t)(d1 - ((int16_t)g.foy >> 3));
                d0 = (int16_t)(d0 - ((int16_t)g.fox >> 2));
            }
            d0 = (int16_t)(d0 + (int16_t)g.fox);
            d1 = (int16_t)(d1 + (int16_t)g.foy);
        }
        pl->Xpos = d0;
        pl->Ypos = d1;
        pl->Xvel = 0;
        pl->Yvel = 0;
        pl->facedir = (int16_t)vtoa((int16_t)(puckx() - d0), (int16_t)(pucky() - d1));
        pl->pflags2 &= (uint8_t)~4;
        pl->pflags &= (uint8_t)~(1 << pfalock);
        SetSPA(pl, SPAglide);
    }
    SprSort();
    ResetAndSelectPlayers(o);
    uint16_t d4 = DecompressGraphicsPlain(SYM_FaceOffMap + 8, g.ExtraChars);
    g.faceoffvrcset = d4;
    DecompressGraphicsPlain(SYM_FaceOffSprites + 8, d4);
    g.arenaanim = 0xFFFF;
    if ((g.sflags7 & 1) && (int16_t)g.faceoffanim[0] >= 0)
        StartArenaAnim(g.faceoffanim[0]);
    printz(str_puckfaceoff2_1, sizeof str_puckfaceoff2_1);
    int16_t d0 = 0x36;                                    /* x of the faceoff window */
    if ((int16_t)g.fox < 0) d0 = 0xBE;
    g.fodropx = (uint16_t)d0;
    g.printx = (uint16_t)((int16_t)(d0 - 0x2E) >> 3);
    d0 = 0x5C;
    g.fodropy = (uint16_t)d0;
    g.printy = (uint16_t)((int16_t)(d0 - 0x44) >> 3);
    {
        const uint8_t *map = rom_ptr(SYM_FaceOffMap);
        uint32_t pal = SYM_FaceOffMap + rd32(map), mp = SYM_FaceOffMap + rd32(map + 4);
        g.sflags6 |= HI(0);
        dobitmap(pal, mp, SYM_FaceOffMap + 8, 0, 0, 0xC, 0xA, g.ExtraChars, 0);
        g.sflags6 &= (uint16_t)~HI(0);
    }
    if (g.OptLine == 0) {                                 /* the two lines on the ice */
        printsmallz(str_puckfaceoff2_2, sizeof str_puckfaceoff2_2);
        Framer(0xC, 3);
        uint16_t l0 = rd16(team_struct(0) + tmline), l1 = rd16(team_struct(1) + tmline);
        if (!(g.gmode & (1 << gmdir))) { uint16_t t = l0; l0 = l1; l1 = t; }
        printsmallz(str_puckfaceoff2_3, sizeof str_puckfaceoff2_3);
        PrintStringFromList(SYM_linelist, l0);
        g.printx += 4;
        PrintStringFromList(SYM_linelist, l1);
    }
    d0 = (int16_t)(randomd0(0x78) + 0xB4);                /* time to the drop */
    {
        uint16_t a = g.arenaanim;
        if ((a == 0 || a == 1 || a == 5 || a == 4 || a == 2) && !(d0 > 0x10E))
            d0 = 0x10E;                                   /* long enough for the arena animation */
    }
    o->temp1 = d0;
    g.palcount = 0x18;
    uint8_t *fof = ram_ptr(RAM_BASE + RAM_OFF_fofdata2);  /* the two faceoff men: frame, attribute */
    wr16(fof, 1);
    wr16(fof + 2, 0x8000);
    wr16(fof + 4, 4);
    wr16(fof + 6, 0xA800);
    wr16(fof + 8, 7);                                     /* frame of the ref */
    wr16(fof + 0xA, 0x8000);
    if (!(g.gmode & (1 << gmdir))) {
        wr16(fof + 2, (uint16_t)(rd16(fof + 2) ^ 0x800));
        wr16(fof + 6, (uint16_t)(rd16(fof + 6) ^ 0x800));
    }
    g.fodir1 = 0xFFFF;
    g.fodir2 = 0xFFFF;
    LeadSong();
    int play = 0;
    if (g.sflags8 & 0x40) { g.sflags8 &= (uint8_t)~0x40; play = 1; }
    else if (g.sflags8 & 0x10) play = 1;
    if (play) {
        g.sflags8 &= (uint8_t)~0x10;
        song((int16_t)g.SongNum);
    }
    g.palcount = 0x18;
    cpu_set_sr(sr);
}

/* checks94.asm:ResetAndSelectPlayers. c1playernum / c2playernum = -1, then changeplayer for
 * each human team (pads 3 and 4 borrow the pad 1 and 2 variables). */
void ResetAndSelectPlayers(struct SortObj *o)
{
    g.c1playernum = 0xFFFF;
    g.c2playernum = 0xFFFF;
    if (g.cont1team != 0) changeplayer(0, o);
    if (g.cont2team != 0) changeplayer(2, o);
    if (g.cont3team != 0) {
        uint16_t t = g.cont1team, p = g.c1playernum;
        g.cont1team = g.cont3team;
        g.c1playernum = 0xFFFF;
        changeplayer(0, o);
        g.c3playernum = g.c1playernum;
        g.cont3team = g.cont1team;
        g.c1playernum = p;
        g.cont1team = t;
    }
    if (g.cont4team != 0) {
        uint16_t t = g.cont2team, p = g.c2playernum;
        g.cont2team = g.cont4team;
        g.c2playernum = 0xFFFF;
        changeplayer(2, o);
        g.c4playernum = g.c2playernum;
        g.cont4team = g.cont2team;
        g.c2playernum = p;
        g.cont2team = t;
    }
}

/* checks94.asm:updatefaceoff. a3 = puck. Drop animation frame from temp1; at 0 erases the
 * faceoff box. */
void updatefaceoff(struct SortObj *puck)
{
    int16_t d0 = puck->temp1;
    if (d0 != 0) {
        d0 = (int16_t)((uint16_t)(d0 + 6) >> 3);
        if (d0 > 2) return;
        g.fofdata = (uint16_t)(0xA - d0);
        return;
    }
    g.disflags &= (uint16_t)~HI(4);
    printz(str_updatefaceoff_1, sizeof str_updatefaceoff_1);
    g.printx = (uint16_t)((int16_t)(g.fodropx - 0x2E) >> 3);
    g.printy = (uint16_t)((int16_t)(g.fodropy - 0x44) >> 3);
    eraser(0xC, 0xD, 0x7FF);
}

/* checks94.asm:Endfaceoff. Drop the puck: pick the faceoff winner from the two men's button
 * timing (fofdata2), send the puck off in his direction (or a random one) with a hop. */
void Endfaceoff(struct SortObj *puck)
{
    sfx(SYM_SFXpuckice + 3);
    g.sflags2 &= (uint16_t)~HI(sf2faceoff);
    g.holdreset = 0x3C;
    DecompressGraphicsPlain(SYM_RefsMap + 8, g.ExtraChars);
    g.sflags2 &= (uint16_t)~HI(sf2drec);
    g.gmode &= (uint8_t)~(1 << gmclock);
    puck->pflags2 &= (uint8_t)~(1 << pf2fight);
    g.crowdnoisedelay = 0;
    g.sflags3 |= 0x10;
    int16_t d3 = (int16_t)g.fodir1;
    int16_t d4 = 0x800;
    const uint8_t *a1 = ram_ptr(RAM_BASE + RAM_OFF_fofdata2);
    uint8_t d2 = 0x10;
    d2 = (uint8_t)(d2 - fo_ftab[(int16_t)rd16(a1) - 1]);
    d2 = (uint8_t)(d2 + fo_ftab[(int16_t)rd16(a1 + 4) - 1]);
    uint16_t d0 = randomd0(0x21);
    if (!(d2 <= (uint8_t)d0))
        a1 += 4;                                          /* the second man won */
    if (rd16(a1 + 2) & 0x800) {
        d3 = (int16_t)g.fodir2;
        d4 = (int16_t)-d4;
    }
    d0 = (uint16_t)d3;
    int random = 1;
    if (!(d0 & 8)) {                                      /* he pushed a direction */
        d0 &= 7;
        if ((vdp_hv16() & 3) != 0) random = 0;
    }
    if (random) {
        d0 = (uint16_t)((randomd0(5) - 2) & 7);
        if (d4 >= 0) d0 ^= 4;
    }
    d0 = (uint16_t)(d0 << 2);
    puck->Xvel = (int16_t)(dirtab[d0 / 2] << 5);
    puck->Yvel = (int16_t)((int16_t)(dirtab[d0 / 2 + 1] << 5) + d4);
    puck->Zvel = (int16_t)randomd0(0x800);
    g.puckz[0] = 0;
    puck->pflags &= (uint8_t)~(1 << pfnc);
    assreplace(0x18, puck);                               /* pucknorm */
}

/* checks94.asm:pucknorm. Assignment for the puck most of the time: age the crossing table,
 * bungie the puck to its carrier's hot spot, icing and offsides, the delay of game whistle
 * when it sits still, then the collisions. */
void pucknorm(uint8_t *tm, struct SortObj *puck)
{
    if (g.BA_PS_flags & HI(2)) UpdatePenaltyShotEnd(puck);
    if (PFLAG(puck, pfna)) {
        puck->pflags &= (uint8_t)~(1 << pfna);
        puck->temp1 = 0;
        puck->temp2 = 0x78;
    }
    uint8_t *pc = ram_ptr(RAM_BASE + RAM_OFF_puckcross);
    wr16(pc + 2, (uint16_t)(rd16(pc + 2) - elapsed));
    wr16(pc + 6, (uint16_t)(rd16(pc + 6) - elapsed));
    puck->temp1 = (int16_t)(puck->temp1 - elapsed);
    if (puck->temp1 < 0) {
        puck->temp1 = (int16_t)(puck->temp1 + 5);
        findpc();
    }
    int16_t d0 = puckc();
    if (d0 >= 0) {
        struct SortObj *a2 = sortobj(d0);
        a2touchpuck(a2, puck);
        int16_t hx, hy;
        GetHot(a2, &hx, &hy);
        d0 = (int16_t)(hx + a2->Xpos - puck->Xpos);
        d0 = (int16_t)(d0 >> 2);
        puck->Xpos = (int16_t)(puck->Xpos + d0);
        int16_t d1 = (int16_t)(hy + a2->Ypos - puck->Ypos);
        d1 = (int16_t)(d1 >> 2);
        puck->Ypos = (int16_t)(puck->Ypos + d1);
        puck->Xvel = a2->Xvel;
        puck->Yvel = a2->Yvel;
    }
    d0 = puckIChk(d0);
    ChkOffsides(d0, puck);
    if (!(g.gmode & (1 << gmclock))) {
        if (puckc() < 0 && puck->Xpos == puck->OldXpos && puck->Ypos == puck->OldYpos) {
            puck->temp2 = (int16_t)(puck->temp2 - 1);
            if (puck->temp2 < 0) AddPenalty2(6, puck);    /* the puck sat still too long */
        } else
            puck->temp2 = 0x78;
    }
    if (HIB(puck->Zvel) == 0 && puck->Zpos == 0)
        puckunflip(puck);
    checkpuckcoll(puck);
}

/* checks94.asm:ChkOffsides. Check for offsides: a3 = puck crossing a blue line with a skater
 * of the attacking team already over it flags the whole team (tmflags bit 4). d0 passes
 * through ClearOffsidesIfAllPlayers. */
void ChkOffsides(int16_t d0, struct SortObj *puck)
{
    if (!(g.gmode & (1 << gmoffs))) return;
    if (g.BA_PS_flags & HI(2)) return;
    uint8_t *a1 = team_struct(0), *a2 = team_struct(1);
    ClearOffsidesIfAllPlayers(a2, &d0);
    { uint8_t *t = a1; a1 = a2; a2 = t; }
    ClearOffsidesIfAllPlayers(a2, &d0);
    d0 = 0x54;
    if (d0 > puck->Ypos) {                                /* the bottom blue line */
        d0 = (int16_t)-d0;
        if (d0 < puck->Ypos) return;
        if (d0 >= puck->OldYpos) return;
        d0 = (int16_t)(d0 - 0xA);
        if (!(g.gmode & (1 << gmdir))) { uint8_t *t = a1; a1 = a2; a2 = t; }
        const struct SortObj *a0 = sortobj_at(rd16(a2 + tmsort));
        for (int i = 0; i < 6; i++, a0++) {
            if (a0->position < 0) continue;
            if (d0 <= a0->Ypos) continue;
            a2[tmflags] |= 0x10;
            return;
        }
        return;
    }
    if (d0 <= puck->OldYpos) return;                      /* did not just cross it */
    d0 = (int16_t)(d0 + 0xA);
    if (g.gmode & (1 << gmdir)) { uint8_t *t = a1; a1 = a2; a2 = t; }
    const struct SortObj *a0 = sortobj_at(rd16(a2 + tmsort));
    for (int i = 0; i < 6; i++, a0++) {
        if (a0->position < 0) continue;
        if (d0 >= a0->Ypos) continue;
        a2[tmflags] |= 0x10;
        return;
    }
}

/* checks94.asm:ClearOffsidesIfAllPlayers. a2 = team struct: clear the team offsides flag once
 * no skater is past the line. d0 is left over from the caller for objects with no position. */
void ClearOffsidesIfAllPlayers(uint8_t *tm, int16_t *d0)
{
    if (!(tm[tmflags] & 0x10)) return;
    const struct SortObj *a0 = sortobj_at(rd16(tm + tmsort));
    for (int d1 = 5; ; a0++) {
        if (a0->position >= 0) {
            *d0 = a0->Ypos;
            if (!PFLAG(a0, pfgoal)) *d0 = (int16_t)-*d0;
        }
        if (*d0 > 0x58) return;
        if (--d1 == -1) break;
    }
    tm[tmflags] &= (uint8_t)~0x10;
}

/* checks94.asm:a2offsides. Player a2 touched the puck: offsides penalty if his team is flagged
 * and the puck is past the blue line. */
void a2offsides(struct SortObj *pl, struct SortObj *puck)
{
    if (!(g.gmode & (1 << gmoffs))) return;
    int16_t d0 = pucky();
    if (!PFLAG(pl, pfgoal)) d0 = (int16_t)-d0;
    if (d0 < 0x68) return;
    const uint8_t *a0 = own_team(pl);
    if (!(a0[tmflags] & 0x10)) return;
    if (g.gmode & (1 << gmhl)) return;
    AddPenalty(0x10, pl);
}

/* checks94.asm:a2touchpuck. Player a2 touches the puck: remember him for the scoring, look for
 * offsides and icing. */
void a2touchpuck(struct SortObj *pl, struct SortObj *puck)
{
    g.ltx = (uint16_t)pl->Xpos;
    g.lty = (uint16_t)pl->Ypos;
    g.ltplayer = (uint16_t)pl->SCnum;
    uint8_t *a0 = own_team(pl);
    uint16_t d0 = pl->pnum;
    if (pl->pflags3 & (1 << pf3onetimer)) g.sflags8 |= 0x80;
    if (d0 != rd16(a0 + tmlasttouch)) {
        int was = a0[tmflags] & 8;
        a0[tmflags] &= (uint8_t)~8;
        if (!was) {
            wr16(a0 + tmassist2, rd16(a0 + tmassist1));
            wr16(a0 + tmassist1, rd16(a0 + tmlasttouch));
        }
        wr16(a0 + tmlasttouch, d0);
        if (d0 == rd16(a0 + tmassist2)) a0[tmassist2] = 0xFF;   /* no assist to himself */
    }
    g.sflags2 &= (uint16_t)~HI(sf2shot);
    a2offsides(pl, puck);
    if ((g.iflags & 4) && (g.iflags & 1) && pl->position != 0) {
        int icing = (g.iflags & 2) ? !PFLAG(pl, pfgoal) : PFLAG(pl, pfgoal) != 0;
        if (icing) {
            AddPenalty(0xC, sortobj(g.icingPlayer[0]));   /* PenIcing on the player who shot it */
            return;
        }
    }
    g.iflags = 0;
    g.icingPlayer[0] = (uint8_t)pl->SCnum;
    int16_t y = pucky();
    if (PFLAG(pl, pfgoal)) {
        g.iflags |= 2;
        y = (int16_t)-y;
    }
    if (y < 0) return;                                    /* past center: icing possible */
    int16_t d = (int16_t)(rd16(team_struct(0) + tmap) - rd16(team_struct(1) + tmap));
    if (PFLAG(pl, pfteam)) d = (int16_t)-d;
    if (d < 0) return;                                    /* short handed: no icing */
    g.iflags |= 4;
}

/* checks94.asm:puckIChk. Check for icing of the puck: a loose puck crossing the far goal line
 * outside the crease sets ifcgl. d0 is the goal line on that path, else unchanged. */
int16_t puckIChk(int16_t d0)
{
    if (!(g.iflags & 4)) return d0;
    if (g.iflags & 1) return d0;
    if (puckc() >= 0) return d0;
    d0 = 0x108;
    if (!(g.iflags & 2)) {
        d0 = (int16_t)-d0;
        if (!(d0 > pucky())) return d0;
    } else if (d0 > pucky())
        return d0;
    if (puckx() > 0x2C || puckx() < -0x2C) {
        g.iflags |= 1;
        return d0;
    }
    g.iflags &= (uint8_t)~4;                              /* through the crease: no icing */
    return d0;
}

/* checks94.asm:puckunflip. Stop the spinning puck. */
void puckunflip(struct SortObj *puck)
{
    if (puck->SPAnum >= 8 && puck->SPAnum < 0x18)
        puck->facedir ^= 2;
    puck->facedir |= 4;
    puck->SPAnum = 0;
    ST_W(puck->SPAcnt);
}

/* checks94.asm:puckflip. Start the puck spinning; d0 bit 0 picks the direction. */
void puckflip(int16_t d0, struct SortObj *puck)
{
    puck->facedir = (int16_t)((puck->facedir ^ (d0 & 1)) & 3);
    ST_W(puck->SPAcnt);
    SetSPA(puck, SPApflip);
}

/* checks94.asm:puckshadow. Assignment for the puck shadow: follow the puck, one below; when
 * its frame is not the puck it is the goal siren instead. */
void puckshadow(uint8_t *tm, struct SortObj *shadow)
{
    const struct SortObj *puck = shadow - 1;
    if (shadow->frame != SPFpuck) {
        if (shadow->frame == 0) return;
        shadow->Xpos = 0;
        shadow->Ypos = 0x12C;
        shadow->Zpos = 0xE;
        if (puck->Ypos < 0) {
            shadow->attribute = 0x8000;
            shadow->Ypos = (int16_t)-shadow->Ypos;
            shadow->Zpos = (int16_t)(shadow->Zpos - 1);
        }
        return;
    }
    shadow->Xpos = puck->Xpos;
    shadow->Ypos = puck->Ypos;
    shadow->Zpos = 0;
    if (g.sflags & HI(sfhor)) shadow->Xpos = (int16_t)(shadow->Xpos + 1);
    else shadow->Ypos = (int16_t)(shadow->Ypos + 1);
}

/* one goal line of findpc: a1 = the puckcross entry, d1 = the sideline, d4 = the goal line */
static void findpc_calc(uint8_t *a1, int16_t d1, int16_t d4)
{
    int16_t d0 = (int16_t)(d4 - pucky());
    int ovf;
    if (puckvy() != 0) {
        int32_t d2 = ((int32_t)d0 << 16) >> 4;
        int32_t q = divs_ovf(d2, puckvy(), &ovf);
        if ((int16_t)q >= 0 && !ovf) {
            wr16(a1 + 2, (uint16_t)q);                    /* frames until it crosses */
            int32_t p = (int32_t)d0 * puckvx();
            q = divs_ovf(p, puckvy(), &ovf);
            if (!ovf) {
                d0 = (int16_t)((int16_t)q + puckx());
                if (!(d0 < d1)) d0 = (int16_t)(-d0 + d1 + d1 + d1);   /* off the boards */
                d1 = (int16_t)-d1;
                if (!(d0 > d1)) d0 = (int16_t)(-d0 + d1 + d1 + d1);
                wr16(a1, (uint16_t)d0);
                return;
            }
        }
    }
    wr16(a1 + 2, 0xFFFF);
}

/* checks94.asm:findpc. Find where and when the puck will cross each goal line (if at all),
 * into puckcross (x, frames) x 2. */
void findpc(void)
{
    uint8_t *a1 = ram_ptr(RAM_BASE + RAM_OFF_puckcross);
    findpc_calc(a1, 0x88, 0x108);
    findpc_calc(a1 + 4, 0x88, -0x108);
}

/* checks94.asm:skateto. Skate player a3 toward d0/d1, every 12 frames: the direction to the
 * point (9 = there), through the aux routine a0 (collision avoidance), then the acceleration.
 * Standing at the point turn to face the puck. */
void skateto(int16_t x, int16_t y, skate_aux aux, struct SortObj *o)
{
    if (TICK_HIB(o->temp2) < 0) {
        SET_HIB(o->temp2, HIB(o->temp2) + 0xC);
        avdgoal(&x, &y, o);
        int16_t d0 = (int16_t)(-(o->Xvel >> 8) + x - o->Xpos);
        int16_t d1 = (int16_t)(-(o->Yvel >> 8) + y - o->Ypos);
        if (d0 <= 0xC && d0 >= -0xC && d1 <= 0xC && d1 >= -0xC)
            d0 = 9;
        else
            d0 = (int16_t)vtoa(d0, d1);
        if (aux) d0 = (int16_t)aux((uint16_t)d0, o);
        SET_LOB(o->temp2, d0);
        if (d0 > 7 && (o->Xvel | o->Yvel) == 0) {         /* standing there: face the puck */
            d0 = puckx();
            d1 = pucky();
            if (g.puck_pflags2[0] & (1 << pf2fight)) { d0 = (int16_t)g.xc1; d1 = (int16_t)g.yc1; }
            d0 = (int16_t)(d0 - o->Xpos);
            d1 = (int16_t)(d1 - o->Ypos);
            d0 = (int16_t)(vtoa(d0, d1) - o->facedir);
            if (d0 != 0) {
                d0 = (int16_t)((((-d0) & 4) >> 1) - 1);
                o->facedir = (int16_t)((o->facedir + d0) & 7);
            }
        }
    }
    doplayeracc((uint8_t)o->temp2, o);
}

/* the bar check of avdgoal: d2 / d3 = y of the player and the target relative to a goal line;
 * crossing it within the net's width moves the target x off the post */
static void avdgoal_ch1(int16_t d0, int16_t d2, int16_t d3, const struct SortObj *o)
{
    if ((int16_t)(d3 ^ d2) >= 0) return;
    int32_t d4 = (int32_t)(int16_t)(d0 - o->Xpos) * d2;
    d2 = (int16_t)(d2 - d3);
    d4 = (int16_t)m68k_divs(d4, d2);
    d4 = (int16_t)(d4 + o->Xpos);
    if (d4 > 0x50) return;
    if (d4 < -0x50) return;
    int16_t w = 0x50;
    int neg = d4 != 0 ? d4 < 0 : o->Xpos < 0;
    if (neg) w = (int16_t)-w;
    g.deltax = (uint16_t)(w - d4);
}

/* checks94.asm:avdgoal. Don't try to skate through the goal: if the line from a3 to d0/d1
 * crosses a net, move the point around it (deltax / deltay). */
void avdgoal(int16_t *x, int16_t *y, struct SortObj *o)
{
    g.deltax = 0;
    g.deltay = 0;
    if (o->position == 0) return;
    int16_t d0 = *x, d1 = *y;
    if ((int16_t)(o->Xpos ^ d0) < 0) {                    /* crossing x = 0: where in y? */
        int32_t d2 = (int32_t)o->Xpos * (int16_t)(d1 - o->Ypos);
        int16_t d3 = (int16_t)(o->Xpos - d0);
        int16_t cy = (int16_t)((int16_t)m68k_divs(d2, d3) + o->Ypos);
        if (cy <= 0x121) {
            if (cy >= 0xDB) {
                int16_t t = 0x144;
                if (!(cy > 0xFE) && (cy < 0xFE || !(o->Ypos > 0xFE))) t = 0xB8;
                g.deltay = (uint16_t)(t - cy);
            } else if (cy <= -0xDB && cy >= -0x121) {
                int16_t t = -0xB8;
                if (!(cy > -0xFE) && (cy < -0xFE || !(o->Ypos > -0xFE))) t = -0x144;
                g.deltay = (uint16_t)(t - cy);
            }
        }
    }
    avdgoal_ch1(d0, (int16_t)(o->Ypos - 0xFE), (int16_t)(d1 - 0xFE), o);
    avdgoal_ch1(d0, (int16_t)(o->Ypos + 0xFE), (int16_t)(d1 + 0xFE), o);
    *x = (int16_t)(d0 + (int16_t)g.deltax);
    *y = (int16_t)(d1 + (int16_t)g.deltay);
}

/* checks94.asm:skatetopuckinit. Find where the puck is going to be. */
void skatetopuckinit(int16_t *x, int16_t *y)
{
    *x = (int16_t)((HIB(puckvx()) >> 1) + puckx());
    *y = (int16_t)((HIB(puckvy()) >> 1) + pucky());
}

/* checks94.asm:skatetopuck. Player a3 should skate to the puck, every 10 frames; a skater
 * facing a puck coming straight at him from a little way off sweep checks. */
void skatetopuck(struct SortObj *o)
{
    int16_t x, y;
    skatetopuckinit(&x, &y);
    if (TICK_HIB(o->temp2) < 0) {
        SET_HIB(o->temp2, HIB(o->temp2) + 0xA);
        avdgoal(&x, &y, o);
        int16_t hx, hy;
        GetHot(o, &hx, &hy);
        int16_t d0 = (int16_t)(int8_t)((uint8_t)-(int8_t)hx - (uint8_t)HIB(o->Xvel));
        d0 = (int16_t)(d0 + x - o->Xpos);
        int16_t d1 = (int16_t)(int8_t)((uint8_t)-(int8_t)hy - (uint8_t)HIB(o->Yvel));
        d1 = (int16_t)(d1 + y - o->Ypos);
        d0 = (int16_t)vtoa(d0, d1);
        SET_LOB(o->temp2, d0);
        if (o->position != 0) {
            d0 = (int16_t)(vtoa(puckvx(), puckvy()) ^ 4);
            if (d0 != o->facedir) {
                d0 = (int16_t)(puckx() - o->Xpos);
                d1 = (int16_t)(pucky() - o->Ypos);
                if (vtoa(d0, d1) == (uint16_t)o->facedir) {
                    uint32_t dsq = (uint32_t)(sq(d0) + sq(d1));
                    if (dsq > 0x384 && dsq <= 0x5A4) { Sweepcheck(o); return; }
                }
            }
        }
    }
    doplayeracc((uint8_t)o->temp2, o);
}

/* checks94.asm:assexit. Exit the current assignment on player a3. */
void assexit(struct SortObj *o)
{
    o->assnum = (int16_t)((o->assnum + 1) & 7);
    o->pflags |= 1 << pfna;
}

/* checks94.asm:assinsert. Insert a new assignment on player a3 (the current one resumes
 * after it). */
void assinsert(uint16_t ass, struct SortObj *o)
{
    o->assnum = (int16_t)((o->assnum - 1) & 7);
    assreplace(ass, o);
}

/* checks94.asm:assreplace. Replace the current assignment on player a3. */
void assreplace(uint16_t ass, struct SortObj *o)
{
    o->asslist[o->assnum] = (uint8_t)ass;
    o->pflags |= 1 << pfna;
}

/* checks94.asm:vtoa. d0/d1 = x/y vector -> d0 = facing direction 0-7 (0 = up, clockwise),
 * 8 = no direction. Classifies the vector by the signs and by whether |x| is within half /
 * double of |y|, then looks the octant up. */
uint16_t vtoa(int16_t x, int16_t y)
{
    static const uint8_t dt[16] = { 1, 7, 3, 5, 0, 0, 4, 4, 2, 6, 2, 6, 1, 7, 3, 5 };
    if ((uint16_t)(x | y) == 0)
        return 8;
    unsigned q = 0;
    uint16_t ax = (uint16_t)x, ay = (uint16_t)y;
    if (x < 0) { ax = (uint16_t)-x; q |= 1; }
    if (y < 0) { ay = (uint16_t)-y; q |= 2; }
    if (!(ax > (uint16_t)(ay << 1))) q |= 4;        /* |x| <= 2|y| */
    if (!(ay > (uint16_t)(ax << 1))) q |= 8;        /* |y| <= 2|x| */
    return dt[q];
}

/* checks94.asm:GetHot. Long on the stack = object; returns d0/d1 = the hot spot (where the
 * puck sits) of the object's current frame, from HotList, flipped with the sprite and swapped
 * for the horizontal rink. */
void GetHot(const struct SortObj *o, int16_t *hx, int16_t *hy)
{
    int16_t x = 0, y = 0;
    if ((int16_t)o->frame > 0) {
        const uint8_t *hot = rom_ptr(SYM_HotList) + o->frame * 2;
        x = (int8_t)hot[0];
        y = (int8_t)hot[1];
        if (o->attribute & HI(3)) x = (int16_t)-x;          /* x flip */
        if (!(o->attribute & HI(4))) y = (int16_t)-y;       /* no y flip */
        if (g.sflags & HI(sfhor)) { int16_t t = x; x = y; y = (int16_t)-t; }
    }
    *hx = x; *hy = y;
}

/* checks94.asm:SetSPA. a3 = object, d1 = animation. Starts the animation from its first
 * frame unless it is already playing. */
void SetSPA(struct SortObj *o, uint16_t spa)
{
    if (o->SPA == spa)
        return;
    o->SPAnum = 0;
    o->SPA = spa;
    o->SPAcnt = (uint16_t)(0xFF00 | (o->SPAcnt & 0xFF));
}

/* checks94.asm:doplayeracc. Player a3 gets acceleration in direction d0 (8 = none, 9 = stop):
 * decide whether to skate backwards (defending in his own zone, or already going that way),
 * turn toward the direction with the turning animation when moving, then playeracc. */
void doplayeracc(uint16_t dir, struct SortObj *o)
{
    if (o->position == 0) { goalieacc(dir, o); return; }
    uint16_t d1 = PFLAG(o, pfrev) ? SPAglideback : SPAglide;
    uint16_t d0 = dir & 0xF;
    if (d0 > 7) {
        if (d0 == 9 && (o->Xvel | o->Yvel) != 0) { dostop(o); return; }
        if (!(o->pflags2 & (1 << pf2aip))) SetSPA(o, d1);
        return;
    }
    {
        int16_t d2 = (int16_t)d0;
        int rev = 0, decided = 0;
        if ((int16_t)o->SCnum != puckc()) {
            int16_t px = (int16_t)(puckx() - o->Xpos);
            int16_t d3 = o->Ypos;
            int16_t py = (int16_t)(HIB(puckvy()) + pucky() - d3);
            if (!PFLAG(o, pfgoal)) { px = (int16_t)-px; py = (int16_t)-py; d3 = (int16_t)-d3; d2 ^= 4; }
            if (!PFLAG(o, pfrev)) {
                if (d3 < 0 && d2 == 4) {                  /* in his own zone, skating toward his net */
                    uint16_t v = (uint16_t)((vtoa(px, py) + 1) & 7);
                    if (v <= 2) rev = 1;                  /* with the puck ahead of him */
                }
            } else {
                d2 = (int16_t)((d2 - 3) & 7);
                if ((uint16_t)d2 <= 2) {
                    uint16_t v = (uint16_t)((vtoa(px, py) + 2) & 7);
                    if (v <= 4) rev = 1;
                }
            }
            if (rev) {
                if (!PFLAG(o, pfrev)) {
                    int set = 1;
                    if ((o->Xvel | o->Yvel) != 0) {
                        uint16_t v = (uint16_t)((vtoa(o->Xvel, o->Yvel) - d0 + 1) & 7);
                        if (v > 2) set = 0;               /* only when already moving that way */
                    }
                    if (set) o->pflags |= 1 << pfrev;
                }
                decided = 1;
            }
        }
        if (!decided) o->pflags &= (uint8_t)~(1 << pfrev);
    }
    int16_t d2 = o->facedir;
    d0 = (uint16_t)((d0 - d2) & 7);
    if (turn_ftab[d0] == 0) { noturn0((uint16_t)(d0 * 2), d2, o); return; }
    int32_t spd = sq(o->Xvel) + sq(o->Yvel);
    int16_t d3 = (int16_t)(spd >> 16);
    int16_t d4 = (int16_t)(0x300 - d3);
    if (d4 < 0x180) d4 = 0x180;
    int32_t turn = (int32_t)d4 * (int16_t)turn_ftab[d0];
    if (PFLAG(o, pfrev)) turn = -turn;
    uint32_t v = (((uint32_t)(uint16_t)o->facedir << 16) | o->_pad56) + (uint32_t)turn;
    o->facedir = (int16_t)((v >> 16) & 7);
    o->_pad56 = (uint16_t)v;
    d2 = o->facedir;
    if ((uint16_t)d3 > 0x14) {                            /* fast enough for the turning animation */
        d1 = 0;
        if ((int16_t)turn_ftab[d0] < 0) d1 ^= SPAturnl - SPAturnr;
        if (o->attribute & HI(3)) d1 ^= SPAturnl - SPAturnr;
        d1 = (uint16_t)(d1 + SPAturnr);
        o->pflags2 |= 1 << pf2aip;
    }
    SetSPA(o, d1);
    if ((uint16_t)d3 > 2) noturn(d2, o);
}

/* checks94.asm:goalieacc. Goalie gets acceleration in direction d0: a human goalie in his
 * crease turns to face the puck (AdjustFacingDirection) and stops when idle; otherwise turn
 * one step toward d0 and skate. */
void goalieacc(uint16_t dir, struct SortObj *o)
{
    uint16_t d0 = dir;
    if (PFLAG(o, pfjoycon) && puckc() != o->SCnum && o->Xpos <= 0x30 && o->Xpos >= -0x30
        && (o->Ypos > -0xC0 ? (o->Ypos >= 0xC0 && o->Ypos <= 0x106) : o->Ypos >= -0xFA - 0xC)) {
        int16_t px = (int16_t)(puckx() - o->Xpos), py = (int16_t)(pucky() - o->Ypos);
        uint16_t face;
        if (px <= 0x10 && px >= -0x10 && py <= 0x10 && py >= -0x10) face = (uint16_t)o->facedir;
        else face = vtoa(px, py);
        if (!(g.sflags4 & HI(0)))
            AdjustFacingDirection((int16_t)face, o);
        else if (d0 != 8)
            AdjustFacingDirection((int16_t)d0, o);
        if (o->pflags2 & 2) return;
        SetSPA(o, SPAgready);
        if (d0 == 8 && o->position == 0 && PFLAG(o, pfjoycon))
            stopna2(o);                                   /* in the crease or not (.topstay), he slows down */
        playeracc((int16_t)d0, o);
        return;
    }
    uint16_t d1 = SPAgready;
    if (PFLAG(o, pfjoycon)) {
        if (PFLAG(o, pfnc)) goto cgl;
        if (d0 != 8) goto turn;
        if (o->position != 0) goto cgl;
        {
            int16_t y = o->Ypos;
            if (PFLAG(o, pfgoal)) y = (int16_t)-y;         /* his own end: the other way from the net he shoots at */
            if (y >= 0xD8 && o->Xpos <= 0x20 && o->Xpos >= -0x20)
                stopna(o);                                /* idle in the crease */
        }
        goto cgl;
    }
    d0 &= 0xF;
    if (d0 <= 7) goto turn;
    if (d0 != 9) goto cgl;
    if ((o->Xvel | o->Yvel) == 0) goto cgl;
    stopna2(o);
    return;
cgl:
    if (!(o->pflags2 & (1 << pf2aip))) SetSPA(o, d1);
    return;
turn:
    {
        int16_t d = (int16_t)(d0 - o->facedir);
        if (d != 0) {
            d = (int16_t)((((-d) & 4) >> 1) - 1);
            o->facedir = (int16_t)((o->facedir + d) & 7);
        }
    }
    SetSPA(o, SPAgskate);
    playeracc(o->facedir, o);
}

/* checks94.asm:noturn0. doplayeracc when the wanted direction needs no turn (.ftab entry 0).
 * d0 = direction minus facedir, times 2; d2 = facedir. Facing the other way: stop or glide
 * round; facing it: the skating animation. */
void noturn0(uint16_t d0, int16_t d2, struct SortObj *o)
{
    int16_t d4 = 2;
    if (PFLAG(o, pfrev)) { d4 = (int16_t)(d4 + 4); d0 ^= 8; }
    if (d0 != 0) {
        uint16_t v = vtoa(o->Xvel, o->Yvel);
        if (!(v & 8)) {
            v = (uint16_t)((v - o->facedir + d4) & 7);
            if (v < 4) { dostop(o); return; }
        }
        o->facedir = (int16_t)(o->facedir + 1);
        if (o->attribute & HI(3)) o->facedir = (int16_t)(o->facedir - 2);
        o->facedir &= 7;
        SetSPA(o, PFLAG(o, pfrev) ? SPAglideback : SPAglide);
        return;
    }
    uint16_t d1 = SPAskateback;
    if (!PFLAG(o, pfrev)) {
        d1 = SPAskate;
        if (o->pflags2 & 0x40) d1 = SPAstumble;           /* hurt in a fight */
        if (puckc() == o->SCnum) d1 = SPAskatewp;
    }
    if (!(o->pflags2 & (1 << pf2aip))) SetSPA(o, d1);
    noturn(d2, o);
}

/* checks94.asm:noturn. Reverse the acceleration direction when skating backwards, then
 * playeracc. */
void noturn(int16_t d2, struct SortObj *o)
{
    if (PFLAG(o, pfrev)) d2 ^= 4;
    playeracc(d2, o);
}

/* checks94.asm:playeracc. Accelerate player a3 in direction d2: the direction's unit vector
 * scaled by his agility and weight (goalies double), not into a wall, over the elapsed frames;
 * capped at his top speed, which his energy, speed rating and a fight injury set; then now and
 * then burn energy (line changes on). */
void playeracc(int16_t dir, struct SortObj *o)
{
    /* dirtab is read from the ROM: a human goalie's pad value can carry extra bits (d0 = $21), and
     * the 68k then reads the code that follows the table */
    const uint8_t *dt = rom_ptr(SYM_dirtab + ((uint16_t)dir << 2));
    int16_t d1 = (int16_t)rd16(dt + 2);
    int16_t d0 = (int16_t)rd16(dt);
    if (o->Wallsin != 0 && (int16_t)(d0 ^ o->Wallsin) < 0) d0 = 0;   /* don't push the wall */
    if (o->Wallcos != 0 && (int16_t)(d1 ^ o->Wallcos) >= 0) d1 = 0;
    int16_t d2 = (int16_t)(o->weight >> 2);
    d2 = (int16_t)(0x40 - d2);
    d2 = (int16_t)((d2 & 0xFF00) | ((d2 + o->legstr) & 0xFF));
    if ((g.sflags8 & 2) || (g.sflags7 & 0x40))
        d2 = (int16_t)((d2 & 0xFF00) | ((d2 + 2) & 0xFF));   /* crowd meter boost */
    if (o->position == 0) {
        d2 = (int16_t)((d2 & 0xFF00) | ((d2 + o->legstr) & 0xFF));   /* goalie gets double agility */
        d2 = (int16_t)(d2 + 0x10);
    }
    d2 = (int16_t)(d2 >> 1);
    int32_t ax = ((int32_t)d0 * d2) >> 5;
    int32_t ay = ((int32_t)d1 * d2) >> 5;
    ax = (int32_t)(int16_t)ax * elapsed;
    ay = (int32_t)(int16_t)ay * elapsed;
    d0 = (int16_t)ax;
    d1 = (int16_t)ay;
    if (o->position == 0 && PFLAG(o, pfjoycon) && puckc() == o->SCnum) {
        d0 = (int16_t)(d0 >> 1);                          /* a human goalie carrying the puck */
        d1 = (int16_t)(d1 >> 1);
    }
    d0 = (int16_t)(d0 + o->Xvel);
    d1 = (int16_t)(d1 + o->Yvel);
    uint32_t d3 = (uint32_t)(sq(d0) + sq(d1));
    int16_t e = getpde(o);
    if (g.sflags7 & 0x10) e = 0x1000;
    uint16_t spd = o->legspd;
    if ((g.sflags8 & 2) || (g.sflags7 & 0x40)) {
        spd = (uint16_t)((spd + 2) & 0xFF);
        if ((int8_t)spd > 0x1E) spd = 0x1E;
    }
    g.TempRawSpd = spd;
    uint32_t prod = (uint32_t)(spd >> 1) * (uint16_t)e;
    prod <<= 4;
    uint16_t d2w = (uint16_t)(prod >> 16);
    d2w = (uint16_t)((d2w << 2) & 0x3F);
    g.TempEnergySpd = d2w;
    uint32_t cap = MaxSpeed[d2w / 4];
    g.TempMaxSpd = cap;
    if ((int16_t)g.TempEnergySpd < 0x3C && (g.TempRawSpd & 1)) {   /* odd rating: halfway to the next */
        int32_t nxt = (int32_t)MaxSpeed[(g.TempEnergySpd + 4) / 4];
        cap = (uint32_t)((((int32_t)nxt - (int32_t)g.TempMaxSpd) >> 1) + (int32_t)g.TempMaxSpd);
    }
    if (o->pflags2 & 0x40) cap >>= 3;                     /* injured in a fight */
    if (o->position == 0 && puckc() == o->SCnum) cap = (uint32_t)((int32_t)cap >> 1);
    if (!(d3 > cap)) {
        o->Xvel = d0;
        o->Yvel = d1;
    }
    if (g.OptLine != 0) return;
    if (vdp_hv16() & 0x7F) return;
    uint8_t *tm = own_team(o);
    int16_t pd1 = (int16_t)((int8_t)o->pnum * 2);
    e = (int16_t)(getpde(o) - 0x21);
    if (e < 0xC00) { setpde(tm, pd1, e); return; }
    e = (int16_t)(e + ((uint16_t)(int16_t)(int8_t)o->endurance >> 1));
    setpde(tm, pd1, e);
    if ((o->endurance & 1) && (g.vcount & 1))
        setpde(tm, pd1, (int16_t)(e + 1));
}

/* checks94.asm:dostop. Player a3 stops: the stopping animation when moving fast enough. */
void dostop(struct SortObj *o)
{
    if (o->Xvel > 0x1000 || o->Xvel < -0x1000 || o->Yvel > 0x1000 || o->Yvel < -0x1000) {
        uint16_t d1 = SPAglide;
        if (!PFLAG(o, pfrev)) {
            o->pflags2 |= 1 << pf2aip;
            d1 = SPAstop;
        }
        SetSPA(o, d1);
    }
    if (o->position != 0) stopna(o);
    else stopna2(o);
}

/* checks94.asm:stopna. Stop with no animation: slow the velocity toward 0 by $96. */
void stopna(struct SortObj *o)
{
    if (o->Xvel < 0) {
        o->Xvel = (int16_t)(o->Xvel + 0x96);
        if (o->Xvel >= 0) o->Xvel = 0;
    } else {
        o->Xvel = (int16_t)(o->Xvel - 0x96);
        if (o->Xvel < 0) o->Xvel = 0;
    }
    if (o->Yvel < 0) {
        o->Yvel = (int16_t)(o->Yvel + 0x96);
        if (o->Yvel >= 0) o->Yvel = 0;
    } else {
        o->Yvel = (int16_t)(o->Yvel - 0x96);
        if (o->Yvel < 0) o->Yvel = 0;
    }
}
