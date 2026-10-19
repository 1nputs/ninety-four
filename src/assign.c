/* assign94.asm: player assignments. Each computer player is given a job for the moment:
 * skating to the bench or the penalty box and back, lining up for a faceoff, celebrating a
 * goal, or playing his spot as a defenseman or a wing. There is no fighting in this game, so
 * the fight assignments do nothing. */
#include "game.h"
#include "anim_gen.h"
#include "symbols_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
#define HIB(w) ((int8_t)((uint16_t)(w) >> 8))
#define SET_HIB(f, b) ((f) = (int16_t)(((uint16_t)(uint8_t)(b) << 8) | ((uint16_t)(f) & 0xFF)))
#define TICK_HIB(f) (SET_HIB(f, (int8_t)(HIB(f) - elapsed)), HIB(f))
static inline int16_t puckx(void) { return (int16_t)g.puckx[0]; }
static inline int16_t pucky(void) { return (int16_t)g.pucky; }
static inline int16_t puckvy(void) { return (int16_t)g.puckvy; }
static inline int16_t puckc(void) { return (int16_t)puck_carrier(); }
static inline rp own_team(const struct SortObj *o) { return team_struct(PFLAG(o, pfteam)); }
static inline struct SortObj *opponents(const struct SortObj *o) { return sortobj(o->SCnum >= 6 ? 0 : 6); }
/* the other team has the puck carrier (puckc - 6 and SCnum - 6 differ in sign) */
static inline int carrier_is_opponent(int16_t carrier, const struct SortObj *o) { return (int16_t)((carrier - 6) ^ (o->SCnum - 6)) < 0; }

/* the common prologue of the positional assignments: nothing while locked in an animation,
 * donothing with the clock stopped, the bench check, nothing for a human; a new assignment
 * resets the timers. Returns 0 when the caller is done. */
static int assignment_prologue(struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return 0;
    if (g.gmode & (1 << gmclock)) { donothing(o); return 0; }
    if (check4bench(o)) return 0;
    if (PFLAG(o, pfjoycon)) return 0;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = 0;
        o->temp2 = 8;
    }
    return 1;
}
/* the timer reload from aidef, one less while the crowd record is broken; 1 when it ran out */
static int aidef_timer(struct SortObj *o)
{
    if (TICK_HIB(o->temp1) >= 0) return 0;
    SET_HIB(o->temp1, o->aidef);
    if ((g.sflags7 & 0x40) && HIB(o->temp1) != 0) SET_HIB(o->temp1, HIB(o->temp1) - 1);
    return 1;
}

/* assign94.asm:check4bench. Check if player a3 should go to the bench: with a line change
 * pending (newpnum) and the puck not on him, switch him to assbench; when the new player is
 * himself, just take his new position. Both drop the caller's return address: returns 1 when
 * the calling assignment is over. */
int check4bench(struct SortObj *o)
{
    if (g.BA_PS_flags & HI(2)) return 0;
    if (PFLAG(o, pfjoycon)) return 0;
    if (o->pflags2 & 0x10) return 0;
    if ((int8_t)o->newpos < 0 && (int8_t)o->newpnum < 0) return 0;
    if (o->newpnum == o->pnum) {
        o->pflags2 &= (uint8_t)~4;
        o->pflags &= (uint8_t)~(1 << pfnc);
        o->newpnum = 0xFF;
        o->newpos = 0xFF;
        Setplass(o);
        return 1;
    }
    if (o->asslist[o->assnum] == 0xB) return 0;           /* already assbench */
    if (o->SCnum == puckc()) return 0;
    o->pflags2 |= 4;                                      /* pf2unav */
    o->temp1 = 0;
    assreplace(0xB, o);                                   /* assbench */
    return 1;
}

/* the new player takes over the object: position from newpos, his first assignment */
static void take_new_position(struct SortObj *o)
{
    o->position = (int8_t)o->newpos;
    Setplass(o);
    o->newpnum = 0xFF;
    o->newpos = 0xFF;
}

/* assign94.asm:assbench. Player a3 should go to the bench: skate to the gate, glide through
 * it (SPAwallleft), then the new player comes on (setplayer). */
void assbench(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (o->temp1 == 0x64) {                               /* through the gate */
        o->frame = 0;
        rp a0 = own_team(o);
        w16(a0, tmpdst + (int8_t)o->pnum * 2, 0xFFFE);    /* he is on the bench */
        uint8_t d3 = o->newpnum;
        take_new_position(o);
        setplayer(d3, o);
        return;
    }
    if (check4bench(o)) return;
    if (o->pflags2 & 0x10) { assexit(o); return; }
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp2 = 8;
        o->temp4 = PFLAG(o, pfteam) ? 0x50 : -0x50;
        o->temp3 = -0x88 - 8;                             /* the gate */
        o->temp1 = 0;
    }
    if (o->newpnum == o->pnum) {                          /* the change was cancelled */
        o->pflags2 &= (uint8_t)~4;
        take_new_position(o);
        return;
    }
    o->temp1 = (int16_t)(o->temp1 - elapsed);
    if (o->temp1 >= 0) goto nodec;
    o->temp1 = (int16_t)(o->temp1 + 8);
    int16_t d0 = (int16_t)(o->Ypos - o->temp4);
    if (d0 > 0x28 || d0 < -0x28) goto nodec;
    d0 = (int16_t)(o->Xpos - o->temp3);
    if (d0 > 0x20) goto nodec;
    SetSPA(o, o->position != 0 ? SPAglide : SPAgready);
    o->pflags |= 1 << pfnc;
    if (o->facedir != 4) o->facedir = (int16_t)((o->facedir + 1) & 7);
    o->Yvel = 0;
    o->Xvel = -0x800;
    if (d0 > 0x10) return;
    o->Xvel = 0;
    if (o->facedir != 4) return;
    o->Xvel = -0x800;
    o->facedir = 2;
    SetSPA(o, SPAwallleft);
    o->pflags |= 1 << pfalock;
    o->temp1 = 0x64;
    return;
nodec:
    if (PFLAG(o, pfnc)) return;
    skateto(o->temp3, o->temp4, EvadePC, o);
}

/* assign94.asm:asseben. Player a3 should exit the bench area: hop the boards at his slot. */
void asseben(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->Xvel = 0;
        o->Yvel = 0;
        int16_t d0 = (int16_t)(o->SCnum - 6);
        if (d0 >= 0) d0 = (int16_t)(d0 + 1);
        o->Ypos = (int16_t)(d0 * 0xE);
        o->Xpos = -0x88;
        o->facedir = 2;
        o->pflags |= 1 << pfalock;
        SetSPA(o, SPAwallright);
        return;
    }
    o->facedir = 4;
    o->pflags &= (uint8_t)~(1 << pfnc);
    o->pflags2 &= (uint8_t)~0x20;
    o->pflags2 &= (uint8_t)~4;
    o->SPA = 0;
    o->Xvel = 0x1000;
    assexit(o);
}

/* assign94.asm:asspenalty. Player a3 should go to the penalty box: skate to the box on his
 * team's side (stacked by how many are in it), turn to the boards and hop over. */
void asspenalty(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->pflags2 |= 4;                                  /* unavailable */
        if (PFLAG(o, pfjoycon))                           /* take the joystick off him */
            changeplayer(o->SCnum == (int16_t)g.c1playernum ? 0 : 2, o);
        setpads(-2, o);
        o->temp2 = 8;
        int16_t d0 = 0xB;
        uint16_t d1 = (uint8_t)(g.PBnum >> 8);            /* players in the box: 00HV */
        if (!PFLAG(o, pfteam)) { d1 >>= 4; d0 = (int16_t)-d0; }
        d1 &= 0xF;
        if (d1 > 2) d1 = 2;
        d1 += 3;
        o->temp4 = (int16_t)((int16_t)d1 * d0);
        o->temp3 = 0x88;
        o->temp1 = 0;
        o->pflags2 |= 0x20;                               /* no player collisions */
        o->Wallcos = 0;
        o->Wallsin = 0;
    }
    int16_t d0 = (int16_t)(o->Ypos - o->temp4);
    if (d0 > 0xC || d0 < -0xC) goto st;
    d0 = (int16_t)(o->Xpos - o->temp3);
    if (d0 < -0x18) goto st;
    o->temp1 = (int16_t)(o->temp1 - elapsed);
    if (o->temp1 >= 0) return;
    o->temp1 = (int16_t)(o->temp1 + 8);
    o->pflags |= 1 << pfnc;
    SetSPA(o, SPAglide);
    {
        int16_t d2 = o->handed[0] ? 2 : 6;
        if (d2 != o->facedir) o->facedir = (int16_t)((o->facedir + 1) & 7);
        o->Yvel = 0;
        o->Xvel = 0x1000;
        if (d0 < -8) return;
        o->Xvel = 0;
        if (d2 != o->facedir) return;
    }
    o->pflags |= 1 << pfalock;
    o->facedir = 2;
    SetSPA(o, SPAwallright);
    o->pflags2 &= (uint8_t)~0x10;
    assreplace(0xD, o);                                   /* assdopen */
    return;
st:
    skateto(o->temp3, o->temp4, NULL, o);
}

/* assign94.asm:assdopen. Add player a3 to the penalty box. */
void assdopen(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    uint8_t d0 = PFLAG(o, pfteam) ? 1 : 0x10;
    g.PBnum = (uint16_t)(((uint8_t)((g.PBnum >> 8) + d0) << 8) | (g.PBnum & 0xFF));
    ST_W(o->position);
    o->frame = 0;
}

/* assign94.asm:assepen. Player a3 should exit the penalty area: hop back onto the ice. */
void assepen(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->pflags |= 1 << pfnc;
        o->pflags2 |= 4;
        uint8_t d0 = 0x10;
        int16_t d1 = -0x3C;
        if (PFLAG(o, pfteam)) { d0 = 1; d1 = 0x3C; }
        g.PBnum = (uint16_t)(((uint8_t)((g.PBnum >> 8) - d0) << 8) | (g.PBnum & 0xFF));
        o->Ypos = d1;
        o->Xvel = 0;
        o->Yvel = 0;
        o->Xpos = 0x86;
        o->facedir = 2;
        o->pflags |= 1 << pfalock;
        SetSPA(o, SPAwallleft);
        SprSort();
        return;
    }
    o->facedir = 4;
    o->newpnum = 0xFF;
    o->newpos = 0xFF;
    o->pflags &= (uint8_t)~(1 << pfjoycon);
    o->pflags &= (uint8_t)~(1 << pfnc);
    o->pflags2 &= (uint8_t)~0x20;
    o->pflags2 &= (uint8_t)~4;
    o->Xvel = -0x1000;
    assexit(o);
}

/* assign94.asm:assgoaliebreakwait. In a shootout or penalty shot park the idle goalie off
 * the ice, else exit. */
void assgoaliebreakwait(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (!(g.BA_PS_flags & HI(2)) && !(g.gmode2 & HI(0))) { assexit(o); return; }
    o->Xvel = 0;
    o->Yvel = 0;
    const struct SortObj *gk = sortobj((int16_t)g.BA_Goalie_SCnum);
    o->Ypos = PFLAG(gk, pfgoal) ? 0x191 : -0x191;
}

/* assign94.asm:assfaceoff. Players do nothing until the faceoff is over. */
void assfaceoff(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (!(g.sflags2 & HI(sf2faceoff))) assexit(o);
}

/* assign94.asm:assfaceoffpl. The two players taking the faceoff: his stick position (from
 * the animation frame and his hand) goes to fofdata2 for the drop; a computer player starts
 * swinging, sometimes early. */
void assfaceoffpl(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (!(g.sflags2 & HI(sf2faceoff))) {
        o->nopuck[0] = 0x14;
        assexit(o);
        return;
    }
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = 0;
    }
    rp fof = RAM(fofdata2);
    if (PFLAG(o, pfteam)) fof = rp_add(fof, 4);
    uint16_t d0 = (uint16_t)((o->SPAnum >> 2) + 1);
    if (o->handed[0] == 0) d0 += 3;
    w16(fof, 0, d0);
    if (PFLAG(o, pfjoycon)) return;
    if (o->pflags2 & (1 << pf2aip)) return;
    o->pflags2 |= 1 << pf2aip;
    uint16_t d1 = SPAfaceoff;
    if ((uint16_t)sortobj(puckscnum)->temp1 > 0x10 && randomd0(8) != 0)
        d1 = SPAfaceoffr;                                 /* jumps the drop */
    o->pflags2 |= 2;
    SetSPA(o, d1);
}

/* assign94.asm:assfight, assfwatch. rts only: there is no fighting. */
void assfight(rp tm, struct SortObj *o) { }
void assfwatch(rp tm, struct SortObj *o) { }

/* assign94.asm:donothing. The player should do nothing: coast. */
void donothing(struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfjoycon)) return;
    doplayeracc(8, o);
}

/* assign94.asm:assstanley. The player skates with the Stanley Cup overhead to the middle
 * of the screen. */
void assstanley(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp3 = 0x5A;
        if ((int16_t)g.Hpos < 0) o->temp3 = (int16_t)-o->temp3;
        o->temp4 = (int16_t)g.Vpos;
        SetSPA(o, SPAstanley);
    }
    uint16_t d0 = vtoa((int16_t)(o->temp3 - o->Xpos), (int16_t)(o->temp4 - o->Ypos));
    if ((int16_t)d0 > 7) return;
    o->facedir = (int16_t)d0;
    playeracc((int16_t)d0, o);
}

/* assign94.asm:assscore. Player a3 celebrates a goal: skate toward the middle, then
 * celebrate (the scorer pumps his arm) at a random moment. */
void assscore(rp tm, struct SortObj *o)
{
    if (PFLAG(o, pfalock)) return;
    if (PFLAG(o, pfna)) {
        o->pflags &= (uint8_t)~(1 << pfna);
        o->temp1 = (int16_t)randomd0(0x78);
        o->temp2 = 8;
        o->temp3 = 0x5A;
        if ((int16_t)g.Hpos < 0) o->temp3 = (int16_t)-o->temp3;
        o->temp4 = (int16_t)g.Vpos;
    }
    int16_t d0 = o->temp3, d1 = o->temp4;
    if (g.gmode2 & HI(0)) {                               /* shootout */
        if (g.shootoutdelay != 0) {
            g.shootoutdelay--;
            if (g.shootoutdelay == 0) g.sflags2 |= HI(sf2drec);
        }
        if (o->Xpos > 0x88 || o->Xpos <= -0x88) d1 = 0;
    }
    o->temp1 = (int16_t)(o->temp1 - elapsed);
    if (o->temp1 >= 0) {
        if (!PFLAG(o, pfjoycon)) skateto(d0, d1, NULL, o);
        return;
    }
    o->pflags |= 1 << pfalock;
    SetSPA(o, g.shotplayer == (uint16_t)o->SCnum ? SPApump : SPAcelebrate);
    o->temp1 = (int16_t)randomd0(0x78);
}

/* assign94.asm:assdefo. Player a3 is a defensive player on offense: hold the point just
 * inside the blue line on his side, following the puck across. */
void assdefo(rp tm, struct SortObj *o)
{
    if (!assignment_prologue(o)) return;
    if (TICK_HIB(o->temp1) < 0) {
        SET_HIB(o->temp1, o->aioff);
        if ((ReadGoaliePulled(o) & CCR_N) || ((g.sflags7 & 0x40) && HIB(o->temp1) != 0))
            SET_HIB(o->temp1, HIB(o->temp1) - 1);
        if (r8(tm, tmflags) & 0x10) { assreplace(2, o); return; }   /* assdefd */
        int16_t d1 = pucky();
        if (!PFLAG(o, pfgoal)) d1 = (int16_t)-d1;
        if (d1 < 0x5D) { assreplace(2, o); return; }      /* puck out of the attack zone */
        d1 = puckc();
        if (d1 >= 0 && carrier_is_opponent(d1, o)) { assreplace(2, o); return; }
    }
    int16_t d0 = o->position == 2 ? 0x50 : -0x50;
    int16_t d1 = 0x62;
    if (!PFLAG(o, pfgoal)) { d0 = (int16_t)-d0; d1 = (int16_t)-d1; }
    if ((int16_t)(puckx() ^ d0) >= 0) d0 = puckx();      /* the puck is on his side */
    else d0 = (int16_t)(d0 + (puckx() >> 1));
    skateto(d0, d1, EvadePC, o);
}

/* assign94.asm:assdefd. Player a3 is a defensive player on defense: stay between the
 * attackers and his net, on the puck's side. */
void assdefd(rp tm, struct SortObj *o)
{
    if (!assignment_prologue(o)) return;
    if (aidef_timer(o)) {
        int16_t d1 = pucky();
        int16_t d3 = (int16_t)(puckvy() >> 6);
        if (!PFLAG(o, pfgoal)) { d1 = (int16_t)-d1; d3 = (int16_t)-d3; }
        if (d3 < 0) d3 = 0;
        d3 = (int16_t)(d3 + d1);
        if (d3 >= 0x58 && !(r8(tm, tmflags) & 0x10)) {
            d1 = puckc();
            if (d1 >= 0 && !chkpk(o) && !carrier_is_opponent(d1, o)) {
                assreplace(1, o);                         /* assdefo */
                return;
            }
        }
        o->temp3 = o->position == 2 ? 0x41 : -0x41;
        const struct SortObj *a0 = opponents(o);
        int16_t d0 = -1000;
        if (PFLAG(o, pfgoal)) {
            d0 = 1000;
            for (int i = 0; i < 6; i++, a0++) {           /* the deepest attacker */
                if (a0->pflags2 & 4) continue;
                if (a0->position < 0) continue;
                int16_t dy = a0->Yvel;
                if (dy >= 0) dy = 0;
                dy = (int16_t)((dy >> 4) + a0->Ypos);
                if (dy > d0) continue;
                d0 = dy;
            }
            d0 = (int16_t)(d0 - 0x32);
            if (!(d0 > -0xBE)) d0 = -0xDC;
            o->temp4 = d0;
        } else {
            for (int i = 0; i < 6; i++, a0++) {
                if (a0->pflags2 & 4) continue;
                if (a0->position < 0) continue;
                int16_t dy = a0->Yvel;
                if (dy < 0) dy = 0;
                dy = (int16_t)((dy >> 4) + a0->Ypos);
                if (dy < d0) continue;
                d0 = dy;
            }
            d0 = (int16_t)(d0 + 0x32);
            if (!(d0 < 0xBE)) d0 = 0xDC;
            o->temp4 = d0;
            o->temp3 = (int16_t)-o->temp3;
        }
        if ((int16_t)(puckx() ^ o->temp3) < 0) o->temp3 = 0;   /* the puck is on the other side */
    }
    skateto(o->temp3, o->temp4, EvadePC, o);
    check4check(o);
}

/* assign94.asm:asswingd. Player a3 is a winger on defense: cover the far side, level with
 * the puck in the defensive zone. */
void asswingd(rp tm, struct SortObj *o)
{
    if (!assignment_prologue(o)) return;
    if (aidef_timer(o)) {
        int16_t d1 = puckc();
        if (d1 >= 0 && !carrier_is_opponent(d1, o)) { assreplace(4, o); return; }   /* asswingo */
    }
    int16_t d0 = o->position == 5 ? -0x64 : 0x64;
    int16_t d1 = 0x9E;
    int16_t d2;
    if (PFLAG(o, pfgoal)) {
        d0 = (int16_t)-d0;
        d1 = (int16_t)-d1;
        d2 = -0x4E;
        if (r8(tm, tmflags) & 0x10) d1 = d2;
        else if (d2 > pucky()) {                          /* puck in the defensive zone */
            d2 = -0x108;
            if (!(d2 < pucky())) d1 = d2;                 /* behind the goal line */
        } else
            d1 = pucky();
    } else {
        d2 = 0x4E;
        if (r8(tm, tmflags) & 0x10) d1 = d2;
        else if (d2 < pucky()) {
            d2 = 0x108;
            if (!(d2 > pucky())) d1 = d2;
        } else
            d1 = pucky();
    }
    if ((int16_t)(puckx() ^ d0) < 0) skateto(d0, d1, NULL, o);
    else skateto(puckx(), d1, NULL, o);
}
