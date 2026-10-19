/* input94.asm: controller input for skaters. Moves the player under control and lets him
 * pass, shoot and check. Also reads the pads at faceoffs and line changes and decides when a
 * player should head to the bench. The shot code here is shared with the computer players. */
#include "game.h"
#include "anim_gen.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "input_tables_gen.h"
#include "checks_tables_gen.h"   /* dirtab */
#include "input_strings_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
#define HIB(w) ((int8_t)((uint16_t)(w) >> 8))
#define BUT_B 0x10
#define BUT_C 0x20
#define BUT_A 0x40
#define BUT_START 0x80
static inline int16_t puckx(void) { return (int16_t)g.puckx[0]; }
static inline int16_t pucky(void) { return (int16_t)g.pucky; }
static inline int16_t puckvx(void) { return (int16_t)g.puckvx; }
static inline int16_t puckvy(void) { return (int16_t)g.puckvy; }
static inline int16_t puckc(void) { return (int16_t)puck_carrier(); }
static inline struct SortObj *opponents(const struct SortObj *o) { return sortobj(o->SCnum >= 6 ? 0 : 6); }
static inline struct SortObj *teammates(const struct SortObj *o) { return sortobj(o->SCnum >= 6 ? 6 : 0); }
static inline int32_t sq(int16_t v) { return (int32_t)v * v; }
/* the B / A hold timers hold pad 1 in the high byte and pad 2 in the low byte */
static inline uint8_t timer_get(uint16_t w, int16_t pad) { return pad ? (uint8_t)w : (uint8_t)(w >> 8); }
#define TIMER_SET(w, pad, v) ((w) = (pad) ? (uint16_t)(((w) & 0xFF00) | (uint8_t)(v)) : (uint16_t)(((uint16_t)(uint8_t)(v) << 8) | ((w) & 0xFF)))
/* subq.b #1 then clamp at 0 */
static inline uint8_t timer_dec(uint8_t v) { int8_t r = (int8_t)(v - 1); return r < 0 ? 0 : (uint8_t)r; }
static inline uint16_t goaliemode(int16_t pad) { return pad ? g.goaliemode2 : g.goaliemode1; }

/* input94.asm:doinput. Process controller input for player a3: d0 = dpad, d1 = new buttons,
 * d2 = changed buttons, d3 = held buttons, d4 = controller 0 / 2 (d6 = his SCnum). B+C
 * together mark a one-timer; start pauses; then the faceoff, line change, one-timer, puck
 * carrier, player switch (B) and goalie branches. */
void doinput(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, int16_t d4, struct SortObj *o)
{
    if ((d1 & BUT_B) && (d1 & BUT_C)) {
        d1 &= (uint16_t)~BUT_B;
        o->pflags3 |= 0x40;
    } else {
        if (d1 & BUT_B) o->pflags3 &= (uint8_t)~0x40;
        if (d2 & BUT_B) {
            int was = o->pflags3 & 0x40;
            o->pflags3 &= (uint8_t)~0x40;
            if (was) {
                TIMER_SET(g.bholdtimer, d4, 0x11);
                d2 &= (uint16_t)~BUT_B;
            }
        }
    }
    if ((g.gmode2 & HI(7)) && (uint8_t)d0 != 8) ShortenMsgTimer(o);
    g.TempWord1 = d0 & 0xF;
    setpads(d4, o);
    if (d1 & BUT_START) {
        if ((int16_t)g.joypuckcarrier < 0) { if (d4 == 0) startpause1(); else startpause2(); }
        else { if (g.joypuckcarrier == 0) startpause3(); else startpause4(); }
        return;
    }
    if ((g.sflags & HI(sfhor)) && !(d0 & 8)) d0 = (uint16_t)((d0 + 2) & 7);
    if (g.sflags2 & HI(sf2faceoff)) { faceoffinput(d0, d1, o); return; }
    if (o->pflags2 & 8) { lineinput(d0, d1, d2, o); return; }
    if (!PFLAG(o, pfjoycon)) return;
    if ((uint16_t)o->SCnum == g.lastplayer && (int16_t)g.passplayer >= 0 && (int16_t)g.onetimerplayer < 0 && (d1 & BUT_C)) {
        struct SortObj *rcv = sortobj((int16_t)g.passplayer);   /* C after a pass: the receiver one-times it */
        if (rcv->position != 0 && !PuckOnAttackHalf(rcv) && !PFLAG(rcv, pfjoycon) && !(rcv->pflags3 & (1 << pf3onetimer))) {
            g.inputjoy = (uint16_t)d4;
            g.TmpJoyPuckCarrier = g.joypuckcarrier;
            assreplace(0x23, rcv);                        /* assonetimer */
            return;
        }
    }
    if (!(o->pflags3 & (1 << pf3onetimer)) && PFLAG(o, pfalock)) { doinput_islocked(d1, d4, o); return; }
    if (o->pflags2 & 1) return;                           /* fightinput: nothing */
    if (puckc() == o->SCnum) { doinput_ispc(d0, d1, d2, d3, d4, o); return; }
    if (o->position != 0 && !(o->pflags3 & (1 << pf3onetimer)) && (d1 & BUT_A)) { holdplayer(o); return; }
    if (o->position != 0) {
        if (g.BA_PS_flags & HI(2)) goto noswitch;
    } else if (!(g.sflags5 & 0x40) && goaliemode(d4) != 0 && o->SCnum != puckc()) {
        changeplayer((uint16_t)d4, o);                       /* the goalie plays himself: switch off him */
        return;
    }
    /* B held / pressed / released: switch player, or (held long) take the goalie */
    if ((d3 & BUT_B) && g.holdreset == 0) {
        uint8_t t = timer_get(g.bholdtimer, d4);
        if (t == 0) goto noswitch;
        t = timer_dec(t);
        TIMER_SET(g.bholdtimer, d4, t);
        if (d4 != 0) {
            if (t != 0) goto bpress;
            if (goaliemode(d4) != 0) goto bpress;
            goto goalie;
        }
        if (goaliemode(d4) != 0) goto bpress;
        if (t == 0) goto goalie;
    }
bpress:
    if (d1 & BUT_B) {
        TIMER_SET(g.bholdtimer, d4, 0x11);
        goto noswitch;
    }
    if (!(d2 & BUT_B)) goto noswitch;
    if (d3 & BUT_B) goto noswitch;
    {
        uint16_t t = g.bholdtimer;                        /* released: a short press switches */
        TIMER_SET(g.bholdtimer, d4, 0x11);
        t &= d4 ? 0xFF : 0xFF00;
        if (t != 0 || goaliemode(d4) != 0) { changeplayer((uint16_t)d4, o); return; }
    }
goalie:
    if (g.holdreset != 0) { changeplayer((uint16_t)d4, o); return; }
    {
        int16_t gk = getGoalieSCnum(o->SCnum <= 5 ? 5 : 0xB);
        if (gk < 0) return;
        if (PFLAG(sortobj(gk), pfjoycon)) return;
        if (d4 == 0) setc1player((uint16_t)gk); else setc2player((uint16_t)gk);
        return;
    }
noswitch:
    if (o->position != 0) { doinput_onetimer(d1, d4, o); return; }
    if (!(d1 & BUT_A)) { doinput_cbut(d1, d3, o); return; }
    d0 = g.TempWord1;
    if ((uint8_t)d0 == 8) { doinput_cbut(d1, d3, o); return; }
    o->facedir = (int16_t)d0;                             /* goalie dive (A + direction) */
    o->nopuck[0] = 8;
    SetSPA(o, SPAgdive);
    o->pflags2 |= 2;
    o->pflags |= 1 << pfalock;
    g.crowdlevel += 0x96;
}

/* input94.asm:ClampTargetY. Clamp the target y d1 for player a3 to the goal line d3 (d0
 * cleared when a3 has the puck). */
void ClampTargetY(int16_t *d0, int16_t *d1, int16_t d3, const struct SortObj *o)
{
    if (puckc() == o->SCnum) *d0 = 0;
    if (*d1 >= 0x103) *d1 = 0x103;
    if (*d1 <= -0x103) *d1 = -0x103;
    *d1 = (int16_t)(*d1 - d3);
}

/* input94.asm:doinput_cbut. C button (bit 5) for the goalie: a press reads the shot and
 * starts a save (goaliesave); held, it holds the save pose (SPAcnt). The shot reading repeats
 * checkanim's arithmetic for nothing but its sroot call. */
void doinput_cbut(uint16_t d1, uint16_t d3, struct SortObj *o)
{
    if (!(d1 & BUT_C)) {
        if (d3 & BUT_C) {
            if (!(o->pflags2 & 0x80)) { doinput_chkanim(o); return; }
            if ((int8_t)(o->SPAnum & 0xFF) < 0) o->SPAcnt = 0xA;
            return;
        }
        o->pflags2 &= (uint8_t)~0x80;
        doinput_chkanim(o);
        return;
    }
    if (PFLAG(o, pfalock)) { doinput_chkanim(o); return; }
    o->facedir = (int16_t)vtoa((int16_t)(puckx() - o->Xpos), (int16_t)(pucky() - o->Ypos));
    g.BA_PS_flags &= (uint16_t)~HI(0);
    {
        int16_t d0 = (int16_t)(((g.gameclock & 7) << 4) + 0xA0);
        if (pucky() > 0xDB || pucky() <= -0xDB) d0 = (int16_t)(d0 - 0x40);
        int16_t dy = d0;
        d0 = (int16_t)((int16_t)(((int32_t)d0 * puckvx()) >> 16) + puckx());
        dy = (int16_t)((int16_t)(((int32_t)dy * puckvy()) >> 16) + pucky());
        ClampTargetY(&d0, &dy, (int16_t)d3, o);           /* d3 is the held buttons here: dead values */
        if ((uint32_t)(sq(d0) + sq(dy)) > 0x384)
            sroot((uint32_t)(sq(d0) + sq(dy)));
    }
    if (o->pflags2 & 2) { doinput_chkanim(o); return; }
    o->pflags2 |= 2;
    const struct PuckCross *pc = &g.puckcross[0];
    int16_t line = 0x108;
    if (PFLAG(o, pfgoal)) { line = (int16_t)-line; pc = &g.puckcross[1]; }
    goaliesave(pc, line, o);
}

/* input94.asm:doinput_chkanim. Nothing while a3's animation runs; a goalie stops at the
 * edges of his crease area, else skate (doplayeracc). */
void doinput_chkanim(struct SortObj *o)
{
    if (o->pflags2 & 2) return;
    if (o->position != 0) { doplayeracc(g.TempWord1, o); return; }
    g.BA_PS_flags &= (uint16_t)~HI(1);
    if (o->Xpos > 0x24 && o->Xvel > 0) { g.BA_PS_flags |= HI(1); o->Xvel = 0; }
    if (o->Xpos < -0x24 && o->Xvel < 0) { g.BA_PS_flags |= HI(1); o->Xvel = 0; }
    if (o->Ypos <= 0xE7 && o->Ypos > -0xE7) {
        if ((int16_t)(o->Ypos ^ o->Yvel) < 0 && o->Yvel != 0) { g.BA_PS_flags |= HI(1); o->Yvel = 0; }
    }
    if (g.BA_PS_flags & HI(1)) return;
    doplayeracc(g.TempWord1, o);
}

/* input94.asm:doinput_onetimer. a3 is set for a one-timer: C (bit 5) starts the one-timer
 * shot (a pass from a skater, puck loose on the attack half), else a burst or skating. */
void doinput_onetimer(uint16_t d1, int16_t d4, struct SortObj *o)
{
    uint16_t d0 = g.TempWord1;
    if (o->pflags3 & (1 << pf3onetimer)) return;
    if (!(d1 & BUT_C)) { doplayeracc(d0, o); return; }
    if (sortobj((int16_t)g.lastplayer)->position == 0) { burst(o); return; }
    if ((uint16_t)o->SCnum != g.passplayer) { burst(o); return; }
    if (puckc() >= 0) { burst(o); return; }
    if (PuckOnAttackHalf(o)) { burst(o); return; }
    g.inputjoy = (uint16_t)d4;
    g.TmpJoyPuckCarrier = g.joypuckcarrier;
    assreplace(0x23, o);                                  /* assonetimer */
}

/* input94.asm:doinput_ispc. a3 has the puck: pass mode, shot mode, or start a pass / shot
 * by the buttons (B pass, A held = line change, C shot). */
void doinput_ispc(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, int16_t d4, struct SortObj *o)
{
    checkob(o);
    if (o->position == 0 && (o->pflags2 & 2)) return;
    if (g.sflags & HI(sfspdir)) { passmode(d0, d2, o); return; }
    if (g.sflags & HI(sfssdir)) { ShotMode(d0, d2, o); return; }
    if (d1 & BUT_B) { CountButtonPress(d4, o); setpassmode(o); return; }
    if (d1 & BUT_A) TIMER_SET(g.aholdtimer, d4, 0xF);
    if (d3 & BUT_A) {
        uint8_t t = timer_dec(timer_get(g.aholdtimer, d4));
        TIMER_SET(g.aholdtimer, d4, t);
        if (t == 0) {                                     /* A held: line change with a pass */
            g.sflags5 |= 8;
            setpassmode(o);
            SetLCmode(o);
            return;
        }
    } else if (d2 & BUT_A) {                              /* A released: a pass */
        g.sflags5 |= 8;
        setpassmode(o);
        return;
    }
    if (o->position == 0) { doplayeracc(d0, o); return; }
    if (!(d1 & BUT_C)) { doplayeracc(d0, o); return; }
    CountButtonPress(d4, o);
    SetShotMode(o);
}

/* input94.asm:doinput_islocked. a3 is locked in an animation: B (bit 4) changes the
 * controlled player, who then has to let go of B (pflags3 bit 6). */
void doinput_islocked(uint16_t d1, int16_t d4, struct SortObj *o)
{
    if (puckc() == o->SCnum) return;
    if (!(d1 & BUT_B)) return;
    if (g.sflags5 & 0x40) return;
    changeplayer((uint16_t)d4, o);
    int16_t d0 = (int16_t)(d4 ? g.c2playernum : g.c1playernum);
    if (d0 >= 0) sortobj(d0)->pflags3 |= 0x40;
}

/* input94.asm:getGoalieSCnum. d0 = SCnum of the goalie in the 6 sort objects up to object
 * d0, -1 if none. */
int16_t getGoalieSCnum(int16_t last)
{
    const struct SortObj *a0 = sortobj(last);
    for (int i = 0; i < 6; i++, a0--)
        if (a0->position == 0) return a0->SCnum;
    return -1;
}

/* input94.asm:faceoffinput. Controller processing for the faceoff: the dpad goes to fodir1 /
 * fodir2 (the pull), B swings early (SPAfaceoff) else the normal swing. */
void faceoffinput(uint16_t d0, uint16_t d1, struct SortObj *o)
{
    if (g.sflags5 & 0x40) return;
    if (o->asslist[o->assnum] != 0x17) return;           /* not a faceoff player */
    if (PFLAG(o, pfgoal)) g.fodir2 = d0; else g.fodir1 = d0;
    if (o->pflags2 & (1 << pf2aip)) return;
    if (d1 & BUT_B) {
        o->pflags2 |= 2;
        SetSPA(o, SPAfaceoff);
        return;
    }
    SetSPA(o, SPAfaceoffr);
}

/* input94.asm:SetLCmode. Initiate the line change option if available (line changes on, the
 * box not already up for his team), then draw the box. */
void SetLCmode(struct SortObj *o)
{
    if (g.OptLine != 0) return;
    if (g.sflags7 & 0x10) return;
    struct Team *tm = loadTeamStruct(o, NULL);
    int was = tm->flags & 2;
    tm->flags |= 2;
    if (was) return;
    if (!(g.sflags5 & 8)) g.sflags &= (uint16_t)~HI(sfspdir);
    g.sflags &= (uint16_t)~HI(sfssdir);
    o->pflags2 |= 8;
    SetLCmode2(tm);
}

/* input94.asm:SetLCmode2. a2 = team struct. Draw the line change box: the frame, the three
 * line choices with their energy bars (the current line while the choice is pending), and
 * the team name. */
void SetLCmode2(struct Team *tm)
{
    uint16_t d0, d1;
    setlccords(tm, &d0, &d1);
    if ((int16_t)g.printy >= 0xF)
        g.sflags3 |= 1 << sf3llcs;
    else {
        box();
        setlccords(tm, &d0, &d1);
    }
    Framer(d0, d1);
    g.printy -= 2;
    g.printx += 1;
    for (int16_t d4 = 2; d4 >= 0; d4--) {
        int16_t line = getlchoice(d4, tm);
        if (line < 0) continue;
        if (!(tm->flags & 2)) {
            if ((uint16_t)d4 != tm->lchoice) { g.printy -= 1; continue; }
            line = (int16_t)tm->line;
        }
        uint8_t *mes = g.mesarea;                             /* "A " .. "C " */
        wr32(mes, 0x00044120);
        mes[2] = (uint8_t)(mes[2] + d4);
        print(ram_addr(mes));
        PrintStringFromList(SYM_linelist, (uint16_t)line);
        linebar((uint16_t)line, tm);
        g.printx -= 5;
        g.printy -= 1;
    }
    g.printx += 2;
    print_p(lg_text(&lg(tm->team)->abbr));                /* the team's abbreviation */
}

/* input94.asm:setlccords. Set printx / printy to the coordinates of the line change box of
 * team a2 (the home side flips with gmode's gmdir). Returns d0 = 9 (width), d1 = 6 or 5. */
void setlccords(struct Team *tm, uint16_t *d0, uint16_t *d1)
{
    uint16_t y = 0;
    if (tm == team_struct(0)) y ^= 0x16;
    if (g.gmode & (1 << gmdir)) y ^= 0x16;
    printz(str_setlccords_1, sizeof str_setlccords_1);
    g.printy += y;
    *d1 = 6;
    if (getlchoice(2, tm) < 0) {
        *d1 = 5;
        g.printy += 1;
    }
    *d0 = 9;
}

/* input94.asm:getlchoice. d0 = 0-2 (a2 = team): the line number that choice means, from
 * the current line and the power play situation; -1 = no such choice. */
int16_t getlchoice(int16_t choice, struct Team *tm)
{
    int16_t d2 = (int16_t)team_struct(tm == team_struct(0))->ap;                 /* the other team's skaters */
    return getlchoice2(choice, d2, tm);
}

/* input94.asm:getlchoice2. Add $15 to d0 for each player more or fewer than the other team
 * has on the ice, 3 x the current line, then look it up. */
int16_t getlchoice2(int16_t d0, int16_t d2, struct Team *tm)
{
    d2 = (int16_t)(d2 - (int16_t)tm->ap);
    if (d2 != 0) {
        d0 = (int16_t)(d0 + 0x15);
        if (d2 >= 0) d0 = (int16_t)(d0 + 0x15);
    }
    int16_t d1 = (int16_t)tm->line;
    d0 = (int16_t)(d0 + d1 + d1 + d1);
    return (int8_t)lchoice_tab[d0];
}

/* input94.asm:lineinput. Process input for line changes: d1 = new button presses (A / B / C
 * pick a line); a pass in progress goes on; a cancelled box is redrawn. */
void lineinput(uint16_t d0, uint16_t d1, uint16_t d2, struct SortObj *o)
{
    if (g.sflags5 & 8) passmode(d0, d2, o);
    struct Team *tm = team_struct(PFLAG(o, pfteam));
    if (tm->flags & 1) {
        tm->flags &= (uint8_t)~1;
        lcfound2(tm);
        SetLCmode2(tm);
    }
    if (d1 & BUT_A) { lcfound(0, tm, o); return; }
    if (d1 & BUT_B) { lcfound(1, tm, o); return; }
    if (d1 & BUT_C) { lcfound(2, tm, o); return; }
    if (!PFLAG(o, pfjoycon)) return;
    if (PFLAG(o, pfalock)) return;
    if (!(o->pflags2 & (1 << pf2fight))) doplayeracc(d0, o);
}

/* input94.asm:lcfound. d2 = choice made 0-2: set the team's line and close the box. */
void lcfound(uint16_t choice, struct Team *tm, struct SortObj *o)
{
    tm->lchoice = choice;
    int16_t line = getlchoice((int16_t)choice, tm);
    if (line < 0) return;
    o->pflags2 &= (uint8_t)~8;
    o->pflags |= 1 << pfjoycon;
    tm = loadTeamStruct(o, NULL);
    tm->flags &= (uint8_t)~2;
    tm->line = (uint16_t)line;
    setpersonel(tm);
    lcfound2(tm);
}

/* input94.asm:lcfound2. Close the line change box of team a2. */
void lcfound2(struct Team *tm)
{
    if (g.sflags & HI(sfhor)) return;
    uint16_t d0, d1;
    setlccords(tm, &d0, &d1);
    if ((int16_t)g.printy >= 0xF) g.sflags3 &= (uint8_t)~(1 << sf3llcs);
    eraser(d0, (uint16_t)(d1 + 1), 0x7FF);
    printscores1();
}

/* input94.asm:burst. C button press: a burst of speed in the facing direction, paid for in
 * energy (line changes on). */
void burst(struct SortObj *o)
{
    int16_t e = getpde(o);
    if (g.OptLine == 0) {
        e = (int16_t)(e - rule_scale(RULE_TIRING, 0xCC));   /* (rules: tiring) */
        setpde(team_struct(PFLAG(o, pfteam)), (int16_t)((int8_t)o->pnum * 2), e);
        if (g.sflags7 & 0x10) e = 0x1000;
    }
    uint16_t d0 = (uint16_t)e >> 7;
    d0 = (uint16_t)rule_scale(RULE_BURST, d0);            /* (rules: the burst) */
    uint16_t d1 = d0;
    unsigned idx = (unsigned)o->facedir * 2;
    o->Xvel = (int16_t)(o->Xvel + (int16_t)((int32_t)(int16_t)d0 * (int16_t)dirtab[idx]));
    o->Yvel = (int16_t)(o->Yvel + (int16_t)((int32_t)(int16_t)d1 * (int16_t)dirtab[idx + 1]));
    o->pflags |= 1 << pfalock;
    SetSPA(o, SPAburst);
}

/* input94.asm:holdplayer. A button hold: hook the last player he hit (Acheck). */
void holdplayer(struct SortObj *o)
{
    o->pflags |= 1 << pfalock;
    if (o->impact == 0) { SetSPA(o, SPAhook); return; }
    Acheck(sortobj(o->impactp), o);
}

/* input94.asm:Acheck. Start SPAhook, or SPAHold if a0 is ahead. The computer's hold check
 * comes in here. */
void Acheck(struct SortObj *a0, struct SortObj *o)
{
    o->pflags |= 1 << pfalock;
    uint16_t d1 = SPAhook;
    if (o->impact != 0) {
        int16_t d0 = (int16_t)(o->Ypos - a0->Ypos);
        if (PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
        if (d0 >= 0) d1 = SPAHold;                        /* stick in the air */
    }
    SetSPA(o, d1);
}

/* input94.asm:setpassmode. Initialize pass mode: the pass direction starts as his facing. */
void setpassmode(struct SortObj *o)
{
    g.passdir = (uint16_t)(o->facedir & 7);
    if (g.BA_PS_flags & HI(2)) {
        g.gmode2 &= (uint16_t)~HI(2);
        int was = g.BA_PS_flags & HI(5);
        g.BA_PS_flags |= HI(5);
        if (was) return;
        g.gmode2 &= (uint16_t)~HI(5);
        g.passmodetimer = 0x64;
    }
    g.sflags |= HI(sfspdir);
}

/* input94.asm:passmode. In pass mode: B released (or a one-timer pass) passes; the dpad sets
 * the direction and passes. */
void passmode(uint16_t d0, uint16_t d2, struct SortObj *o)
{
    if (!(d2 & BUT_B) && !(g.sflags5 & 8)) {
        if (d0 & 8) return;                               /* no direction */
        g.passdir = d0 & 7;
    }
    dopass(o);
}

/* input94.asm:dopass. Pass the puck in passdir, to the best teammate there if any (passto),
 * else just send it that way; a one-timer pass goes to the receiver's spot. The pass speed
 * comes from his passing rating. */
void dopass(struct SortObj *o)
{
    g.sflags &= (uint16_t)~HI(sfspdir);
    puck_loose();
    o->nopuck[0] = 0x10;
    g.lastplayer = (uint16_t)o->SCnum;
    if (g.sflags5 & 8) {
        g.sflags5 &= (uint8_t)~8;
        OneTimerTarget(o);
        g.onetimerheight = 0x12;
        if (g.sflags6 & HI(2)) g.onetimerheight = 0x3A;
        OneTimerPass(o);
    } else {
        int16_t d0 = 8;
        if (o->position != 0) d0 = o->passacc;
        d0 = (int16_t)(((int16_t)(d0 << 2) >> 1) + 0xA0);
        g.passspeed = (uint16_t)d0;
        if (o->passacc & 1) g.passspeed = (uint16_t)(g.passspeed + (d0 >> 4));
        int32_t d4 = -1;
        struct SortObj *best = NULL;
        struct SortObj *a1 = teammates(o);
        int search = 1;
        if (o->SCnum >= 6 && (g.BA_PS_flags & HI(2))) search = 0;
        if (search) {
            for (int i = 0; i < 6; i++, a1++) {           /* the nearest teammate in that direction */
                if (a1 == o) continue;
                if (a1->position == 0) continue;
                if (a1->pflags2 & 4) continue;
                int16_t dx = (int16_t)(a1->Xpos - puckx()), dy = (int16_t)(a1->Ypos - pucky());
                int16_t dd = (int16_t)((vtoa(dx, dy) - g.passdir) & 7);
                dd = (int16_t)(int8_t)(dd << 5);
                dd = (int16_t)(dd << 3);
                int32_t ddsq = sq(dd);
                if ((uint32_t)ddsq > 0x10000) continue;
                int32_t d2 = sq(dx) + sq(dy) + ddsq;
                if ((uint32_t)d2 > (uint32_t)d4) continue;
                d4 = d2;
                best = a1;
            }
        }
        if (d4 >= 0)
            passto(best, o);
        else {                                            /* nobody: hit it in passdir */
            unsigned idx = (unsigned)g.passdir * 2;
            int32_t v = (int32_t)(int16_t)dirtab[idx + 1] * (int16_t)g.passspeed;
            v <<= 10;
            g.puckvy = (uint16_t)((int16_t)m68k_divs(v, 0xBB8) + o->Yvel);
            v = (int32_t)(int16_t)dirtab[idx] * (int16_t)g.passspeed;
            v <<= 10;
            g.puckvx = (uint16_t)((int16_t)m68k_divs(v, 0xBB8) + o->Xvel);
            g.puckvz[0] = randomd0(0x1000);
        }
    }
    if (o->position == 0) {                               /* the goalie clears away from his net */
        int n = puckvy() < 0;
        if (PFLAG(o, pfgoal) ? n : !n) g.puckvy = (uint16_t)-puckvy();
    }
    uint16_t dir = vtoa(puckvx(), puckvy());
    uint16_t d1 = SPAgswing;
    if (o->position != 0) {
        d1 = SPApassf;
        if (!Findhittype(dir, o)) d1 = SPApassb;
    }
    SetSPA(o, d1);
    o->pflags |= 1 << pfalock;
    uint16_t snd = (uint16_t)(uint8_t)(0xC - (uint8_t)(g.puckvz[0] >> 8));
    snd = (uint16_t)(((snd >> 2) & 3) + 0x10);            /* SFXpass, by the puck's hop */
    sfx((int16_t)snd);
}

/* input94.asm:passto. Pass the puck to player a0 from a3: a human goalie's pass hands the
 * controller to the receiver. */
void passto(struct SortObj *rcv, struct SortObj *o)
{
    if (PFLAG(o, pfjoycon) && o->position == 0 && !PFLAG(rcv, pfjoycon)) {
        rcv->pflags3 |= 0x40;
        uint16_t d0 = (uint16_t)rcv->SCnum;
        if (g.cont1team == g.cont2team) {                 /* two pads on one team */
            if ((uint16_t)o->SCnum == g.c1playernum) setc1player(d0); else setc2player(d0);
        } else {
            uint16_t d1 = PFLAG(o, pfteam) ? 2 : 1;
            if (d1 == g.cont1team) setc1player(d0); else setc2player(d0);
        }
    }
    passtoa0(rcv, o);
}

/* input94.asm:passtoa0. Pass the puck to player a0: he gets apassrec; the puck's velocity
 * is solved so that it meets him where he will be (passspeed pixels per second, his speed
 * per quarter second), with a hop timed to the meeting. */
void passtoa0(struct SortObj *rcv, struct SortObj *o)
{
    struct Team *tm = loadTeamStruct(o, NULL);
    tm->passes++;                                                /* pass attempts */
    g.passplayer = (uint16_t)rcv->SCnum;
    int16_t d5 = (int16_t)((int16_t)g.passspeed >> 2);
    assinsert(0x13, rcv);                                 /* apassrec */
    int16_t hx, hy;
    GetHot(rcv, &hx, &hy);
    int16_t dx = (int16_t)(hx + rcv->Xpos - puckx());
    int16_t dy = (int16_t)(hy + rcv->Ypos - pucky());
    int16_t d2 = (int16_t)(dx >> 2), d3 = (int16_t)(dy >> 2);
    int16_t vx = (int16_t)(((int32_t)rcv->Xvel * 0xF0) >> 16);   /* pixels per quarter second */
    int16_t vy = (int16_t)(((int32_t)rcv->Yvel * 0xF0) >> 16);
    int16_t j = (int16_t)((int16_t)((int16_t)((int32_t)vx * d2) + (int16_t)((int32_t)vy * d3)) << 1);
    int32_t k = sq(vx) + sq(vy) - sq(d5);
    int32_t asq = sq(d2) + sq(d3);
    int32_t kasq = (int32_t)(int16_t)asq * (int16_t)k;
    kasq <<= 2;
    int32_t disc = sq(j) - kasq;
    int16_t r = (int16_t)sroot((uint32_t)disc);
    int16_t div = (int16_t)((int16_t)k >> 2);
    if (div == 0) div = 1;                                /* no divide by zero */
    int16_t t = 0, d0 = r;
    int zero = 0;
    for (int d3c = 1; ; ) {
        int32_t q = m68k_divs((int32_t)(int16_t)(d0 - j), div);
        t = (int16_t)q;
        zero = t == 0;
        d0 = (int16_t)-d0;
        if (t >= 0) break;
        if (--d3c == -1) break;
    }
    if (zero) t = (int16_t)(t + 1);                       /* can't be zero */
    if ((uint16_t)t > 0x18) t = 0x18;                     /* at most 3 seconds, in eighths */
    g.puckvz[0] = (uint16_t)(((uint8_t)t << 8) | (g.puckvz[0] & 0xFF));
    if (t >= 0xC) g.puckvz[0] = (uint16_t)((0xC << 8) | (g.puckvz[0] & 0xFF));
    int16_t w = (int16_t)((t << 3) - 0xA);
    rcv->temp1 = (int16_t)(((uint16_t)(uint8_t)w << 8) | ((uint16_t)rcv->temp1 & 0xFF));
    w = (int16_t)(w - 6);
    g.puckx_nopuck = (uint8_t)w;
    int32_t px = ((int32_t)vx * t) >> 1;
    px = (int32_t)((px & 0xFFFF0000u) | (uint16_t)((int16_t)px + dx));
    rcv->temp3 = (int16_t)(puckx() + (int16_t)px);
    int32_t py = ((int32_t)vy * t) >> 1;
    py = (int32_t)((py & 0xFFFF0000u) | (uint16_t)((int16_t)py + dy));
    rcv->temp4 = (int16_t)(pucky() + (int16_t)py);
    int16_t tt = (int16_t)(t * 0x78);
    uint32_t sw = ((uint32_t)(uint16_t)px << 16) | ((uint32_t)px >> 16);
    g.puckvx = (uint16_t)m68k_divs((int32_t)sw, tt);
    sw = ((uint32_t)(uint16_t)py << 16) | ((uint32_t)py >> 16);
    g.puckvy = (uint16_t)m68k_divs((int32_t)sw, tt);
}

/* input94.asm:changeplayer. The pad d4 takes the nearest free skater (chgplayer); a3 sweep
 * checks when it is the pad's own player. */
void changeplayer(uint16_t pad, struct SortObj *o)
{
    chgplayer(pad, o);
}

/* input94.asm:Sweepcheck. Start a sweep check on player a3. */
void Sweepcheck(struct SortObj *o)
{
    o->pflags |= 1 << pfalock;
    SetSPA(o, SPAsweepchk);
}

/* input94.asm:setc1player / setc2player. Restore the old player and switch to the new d0
 * player on controller 1 / 2. */
void setc1player(uint16_t player)
{
    if (player == g.c1playernum) return;
    g.c1playernum = (uint16_t)restorepl((int16_t)g.c1playernum, (int16_t)player);
}
void setc2player(uint16_t player)
{
    if (player == g.c2playernum) return;
    g.c2playernum = (uint16_t)restorepl((int16_t)g.c2playernum, (int16_t)player);
}

/* input94.asm:restorepl. d1 = the old player: give him back to the computer (unless he is
 * in a line change box); d0 = the new one takes the pad, except a goalie the pad does not
 * control in a penalty shot / shootout (the first skater instead). Returns d0. */
int16_t restorepl(int16_t old, int16_t new)
{
    if (old >= 0 && old <= 0xB) {
        struct SortObj *p = sortobj(old);
        if (p->pflags2 & 8) return old;
        p->pflags &= (uint8_t)~(1 << pfjoycon);
        if (!(p->pflags3 & 8)) p->pflags |= 1 << pfna;
    }
    if (new < 0 || new > 0xB) return new;
    struct SortObj *q = sortobj(new);
    if (((g.BA_PS_flags & HI(2)) || (g.gmode2 & HI(0))) && q->position == 0) {
        uint16_t gm = PFLAG(q, pfteam) ? g.goaliemode2 : g.goaliemode1;
        if (gm != 0) return PFLAG(q, pfteam) ? 6 : 0;
    }
    q->pflags |= 1 << pfjoycon;
    return new;
}

/* input94.asm:Findhittype. Look for the type of swing: d0 = launch direction relative to
 * his facing and stick side. Returns 1 for a forehand (the 68k Z flag). */
int Findhittype(uint16_t dir, const struct SortObj *o)
{
    unsigned d0 = (unsigned)(o->facedir - (int16_t)dir) & 7;
    unsigned mask = (o->attribute & HI(3)) ? 0xF0 : 0x1E;
    return !((mask >> d0) & 1);
}

/* input94.asm:SetShotMode. Initiate a shot by player a3: wind up forehand or backhand toward
 * the far boards. */
void SetShotMode(struct SortObj *o)
{
    if (g.BA_PS_flags & HI(2)) {
        g.gmode2 &= (uint16_t)~HI(2);
        int was = g.BA_PS_flags & HI(5);
        g.BA_PS_flags |= HI(5);
        if (was) return;
        g.gmode2 &= (uint16_t)~HI(5);
        g.passmodetimer = 0x64;
    }
    g.passdir = 8;
    g.sflags |= HI(sfssdir);
    int16_t d0 = 0, d1 = PFLAG(o, pfgoal) ? 0x128 : -0x128;
    d0 = (int16_t)(d0 - o->Xpos);
    d1 = (int16_t)(d1 - o->Ypos);
    uint16_t dir = vtoa(d0, d1);
    g.passspeed = 0xF;
    uint16_t spa = SPAshotf;
    if (!Findhittype(dir, o)) {
        spa = SPAshotb;
        g.passspeed = 0xF;
    }
    SetSPA(o, spa);
}

/* input94.asm:ShotMode. Shot input: the dpad aims, the windup adds speed until the C button
 * changes (or the swing is forced past the half way point for weak shooters); at the end of
 * the animation the shot goes (prepshot). d0 = dpad, d2 = changed buttons. */
void ShotMode(uint16_t d0, uint16_t d2, struct SortObj *o)
{
    if ((int16_t)o->SPAnum >= 0x1C) { prepshot(o); return; }
    if (!(d0 & 8)) g.passdir = d0 & 7;
    if ((int16_t)o->SPAnum >= 0x10) return;
    g.passspeed = (uint16_t)(g.passspeed + elapsed);
    if ((int8_t)o->shotspd >= 0x14 || !((int16_t)o->SPAnum > 8)) {
        if (!(d2 & BUT_C)) return;
    }
    o->SPAnum = (uint16_t)(0x1C - o->SPAnum);             /* end the windup and swing through */
}

/* input94.asm:prepshot. Check certain conditions before shooting: a shot from in close at a
 * standing goalie goes top shelf (sflags5 bit 4). */
void prepshot(struct SortObj *o)
{
    g.sflags5 &= (uint8_t)~0x10;
    int16_t gk = getGoalieSCnum(PFLAG(o, pfteam) ? 5 : 0xB);
    int standing = 0;
    if (gk >= 0) {
        uint16_t spa = sortobj(gk)->SPA;
        standing = spa != SPAgstackr && spa != SPAgstackl;
    }
    if (!standing && ((int16_t)g.passdir <= 1 || g.passdir == 7)) {
        int16_t d0 = pucky();
        if (!PFLAG(o, pfgoal)) d0 = (int16_t)-d0;
        if (d0 >= 0xD8) g.sflags5 |= 0x10;
    }
    doshot(o);
}

/* input94.asm:doshot. The stick is at the puck: launch it toward the goal. The speed comes
 * from his shot power, energy and the windup; the aim (shotsets) is perfect in close with
 * luck by his accuracy, else scattered; the height from the distance. */
void doshot(struct SortObj *o)
{
    g.gmode2 &= (uint16_t)~HI(4);
    if (o->pflags3 & (1 << pf3break)) g.gmode2 |= HI(4);
    shotdiradj(o);
    int16_t snd = SYM_SFXshotwiff;
    g.shotplayer = (uint16_t)o->SCnum;
    g.sflags &= (uint16_t)~HI(sfssdir);
    o->pflags |= 1 << pfalock;
    if (!(o->pflags3 & (1 << pf3onetimer)) && puckc() != o->SCnum) {
        sfx(snd);                                         /* whiffed */
        return;
    }
    snd = 0x18;                                           /* SFXshotfh */
    g.sflags2 |= HI(sf2shot);
    if (o->SPA == SPAshotb) {
        snd = 0x14;                                       /* SFXshotbh */
        g.passspeed = (uint16_t)(g.passspeed - (g.passspeed >> 2));
    }
    if (o->pflags3 & (1 << pf3onetimer)) g.passspeed = 0x1F;
    uint16_t d0 = makepde((int8_t)(o->shotspd >> 1), o);
    d0 = (uint16_t)(d0 + 0x14);
    uint32_t prod = (uint32_t)d0 * g.passspeed;
    prod = (uint32_t)(uint16_t)prod * 0x5249u;
    d0 = (uint16_t)(prod >> 16);
    d0 = (uint16_t)rule_scale(RULE_SHOT_POWER, d0);       /* (rules: shot power) */
    g.passspeed = d0;
    if (o->shotspd & 1) d0 = (uint16_t)(((int16_t)d0 >> 4) + g.passspeed);
    d0 = (uint16_t)(d0 >> 4);
    int16_t sadd = (int16_t)(3 - (int16_t)d0);
    if (sadd < 0) sadd = 0;
    snd = (int16_t)(snd + sadd);
    puck_loose();
    o->nopuck[0] = 0x10;
    g.lastplayer = (uint16_t)o->SCnum;
    int16_t dy = PFLAG(o, pfgoal) ? 0x108 : -0x108;
    unsigned idx = (unsigned)g.passdir * 2;
    int16_t dx = (int16_t)((int16_t)shotsets[idx] - puckx());
    int16_t dz = (int16_t)shotsets[idx + 1];
    dy = (int16_t)(dy - pucky());
    int16_t d3 = (int16_t)sroot((uint32_t)(sq(dx) + sq(dy)));
    if (d3 == 0) d3 = 1;                                  /* distance to the spot */
    int perfect = 0;
    if (g.gmode & (1 << gmhl)) perfect = 1;
    else if ((uint16_t)d3 <= 0xC8) {
        if (ReadGoaliePulled(o) & CCR_N) perfect = 1;
        else if (g.gmode2 & HI(0)) perfect = 1;
        else if (randomd0((uint16_t)((0x10 + o->shotacc) & 0xFF)) > 0xE) perfect = 1;
    }
    if (!perfect) {
        uint8_t acc = (uint8_t)(o->shotacc >> 1);
        uint16_t a = (uint16_t)(g.passspeed >> 4);
        a = (uint16_t)((a & 0xFF00) | ((uint8_t)(a - acc) & 0xFF));
        a = (uint16_t)((a & 0xFF00) | ((uint8_t)(a + 0x10) & 0xFF));
        a = (uint16_t)((uint16_t)(a * (uint16_t)d3) >> 6);
        if ((uint16_t)d3 <= 0xFA) a = (uint16_t)(a >> 1);
        if ((int16_t)a >= 0x88) a = 0x88;
        dx = (int16_t)(dx + (int16_t)randomd0s(a));
        if (a > 0x3C) a = 0x3C;
        uint16_t ay = a;
        if (dy < 0) ay = (uint16_t)(ay >> 1);
        dy = (int16_t)(dy + (int16_t)randomd0s(ay));
        dz = (int16_t)(dz + (int16_t)randomd0((uint16_t)(a >> 1)));
    }
    int32_t v = (int32_t)(int16_t)g.passspeed * 0x44;
    v = (int32_t)(int16_t)v * dx;
    g.puckvx = (uint16_t)m68k_divs(v, d3);
    v = (int32_t)(int16_t)g.passspeed * 0x44;
    v = (int32_t)(int16_t)v * dy;
    int32_t qy = m68k_divs(v, d3);
    g.puckvy = (uint16_t)qy;
    {
        uint16_t lim = PFLAG(o, pfgoal) ? 0 : 0x8000;
        if ((int16_t)((uint16_t)qy ^ lim) < 0) {          /* past the fastest allowed */
            g.puckvy = 0x3810;
            if (!PFLAG(o, pfgoal)) g.puckvy = 0xC7F0;
        }
    }
    if (dz != 0) {
        uint32_t h = (uint32_t)(uint16_t)dz * g.passspeed;
        h = (uint32_t)(uint16_t)h * 0x44u;
        h = (uint16_t)m68k_divu(h, (uint16_t)d3);
        uint32_t dd = (uint32_t)(uint16_t)d3 * 0xB33u;
        dd = (uint16_t)m68k_divu(dd, g.passspeed);
        uint16_t vz = (uint16_t)(dd + h);
        if (vz > 0x1800) vz = 0x1800;
        g.puckvz[0] = vz;
        int was = g.sflags5 & 0x10;
        g.sflags5 &= (uint8_t)~0x10;
        if (was && !(o->pflags3 & (1 << pf3onetimer))) puckvzadj(o);
    }
    sfx(snd);
}

/* input94.asm:CalcAngleOffset. The area of the triangle puck - goalie - post over the
 * distance from the goalie to the puck: which side of the goalie the post is seen on. */
int16_t CalcAngleOffset(int16_t d0, int16_t d1, int16_t d2, int16_t d3, int16_t d4)
{
    d3 = (int16_t)(d3 - puckx());
    d4 = (int16_t)(d4 - pucky());
    int32_t a = (int32_t)d0 * d4;
    int32_t b = (int32_t)d1 * d3;
    return (int16_t)m68k_divs(a - b, d2);
}

/* input94.asm:shotdiradj. Where a computer player (or a one-timer) shoots: at the post the
 * goalie leaves more open, straight when he covers the middle, 8 with no goalie. */
void shotdiradj(struct SortObj *o)
{
    if (!(o->pflags3 & (1 << pf3onetimer)) && PFLAG(o, pfjoycon)) return;
    uint16_t d0 = 8;
    const struct SortObj *gk = opponents(o);
    int found = 0;
    for (int i = 0; i < 6; i++, gk++)
        if (gk->position == 0) { found = 1; break; }
    if (found) {
        int16_t dx = (int16_t)((HIB(gk->Xvel) >> 1) + gk->Xpos - puckx());
        int16_t dy = (int16_t)((HIB(gk->Yvel) >> 1) + gk->Ypos - pucky());
        int16_t d2 = (int16_t)sroot((uint32_t)(sq(dx) + sq(dy) + 1));
        int16_t d3 = 0x12;
        int16_t d4 = PFLAG(o, pfgoal) ? 0x108 : -0x108;
        int16_t d5 = CalcAngleOffset(dx, dy, d2, d3, d4);
        int16_t off = (int16_t)(CalcAngleOffset(dx, dy, d2, (int16_t)-d3, d4) + d5);
        d0 = 0;
        if (off <= 0x2C && off >= -0x2C) {
            if (PFLAG(o, pfgoal)) off = (int16_t)-off;
            d0 = off >= 0 ? 2 : 6;
        }
    }
    g.passdir = d0;
    if ((g.BA_PS_flags & HI(2)) || (g.gmode2 & HI(0))) PSandSOpassdir(o);
}

/* input94.asm:setpads. Put the SCnum of a3 in the d4 nibble of PadControlBits; d4 = -2 puck
 * carrier, 0 / 2 pads, 4 replay target. */
void setpads(int16_t pad, struct SortObj *o)
{
    unsigned sh = ((unsigned)(2 + pad) * 2) & 63;
    uint16_t mask = (uint16_t)((0xFFF0u << sh) | (0xFFF0u >> (16 - sh)));
    if (sh == 0) mask = 0xFFF0;
    g.PadControlBits &= mask;
    g.PadControlBits |= (uint16_t)((uint16_t)o->SCnum << sh);
}
