/* penalty94.asm: penalty logic. Calls penalties and the other stoppages of play, awards penalty
 * shots on breakaways, sends players to the penalty box, runs power plays, shows the referee's
 * messages and switches the rink between its horizontal and vertical views; the intermission
 * between periods and the highlights of the other games (Intermission, DoHiLights, StartHL,
 * StartHL2). */
#include <string.h>
#include "game.h"
#include "anim_gen.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "view.h"
#include "penalty_tables_gen.h"
#include "penalty_strings_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
static inline int16_t puckx(void) { return (int16_t)g.puckx[0]; }
static inline int16_t pucky(void) { return (int16_t)g.pucky; }
static inline int16_t puckc(void) { return (int16_t)puck_carrier(); }
/* the Penaltylist entry of penalty number d0 (a word offset into the table): byte 0 = delay before
 * the stop, byte 1 = minutes, +2 a String (the message), then the referee animation */
static inline const uint8_t *penalty_entry(uint16_t pen) { const uint8_t *t = rom_ptr(SYM_Penaltylist); return t + rd16(t + pen); }
static inline uint8_t *penbuf(void) { return g.PenBuf; }

/* penalty94.asm:loadTeamStruct. a2 = team struct of player a3, a1 = the other team's. */
struct Team *loadTeamStruct(const struct SortObj *o, struct Team **other)
{
    int away = (o->pflags & (1 << pfteam)) != 0;
    if (other) *other = team_struct(!away);
    return team_struct(away);
}

/* penalty94.asm:AddPenalty. Add penalty d0 for player a3. Ignored while the clock is stopped,
 * when penalties are off, for a player with pflags2 bit 2 set, or offsides when offsides are
 * off. */
void AddPenalty(uint16_t pen, struct SortObj *o)
{
    if (g.gmode & (1 << gmclock)) return;
    if (pen != 0xC) {                                     /* icing always */
        if (g.OptPen == 0) return;
        if (o->pflags2 & 4) return;
        if (!(g.gmode & (1 << gmoffs)) && pen == 0x10) return;
    }
    if (pen > 0x10 && !rule_chance(rules[RULE_PENALTIES])) return;   /* (rules: the penalties called) */
    AddPenalty2(pen, o);
}

/* penalty94.asm:AddPenalty2. Forced penalties like the faceoff and game over. d0 = penalty
 * number, a3 = player or a player on the penalized team. From $E (goal) up: crowd and song. A
 * penalty with minutes sets DelayedPen bit 1 (home) or bit 2 (away). */
void AddPenalty2(uint16_t pen, struct SortObj *o)
{
    if (g.sflags & HI(sfhor)) return;
    if ((int16_t)pen >= 0xE) {
        g.crowdlevel += 0xC8;
        int16_t snd = 0xC;
        if (PFLAG(o, pfteam)) { g.CwdExciteLvl += 0x14; snd = 0xB; }
        song(snd);
    }
    uint8_t *a1 = penbuf();
    int i;
    for (i = 0; i < 32; i++, a1 += 2)                     /* a free slot */
        if (rd16(a1) == 0) break;
    if (i == 32) return;                                  /* buffer full */
    a1[1] = (uint8_t)o->SCnum;
    a1[0] = (uint8_t)pen;
    const uint8_t *e = penalty_entry(pen);
    if ((int8_t)e[1] <= 0) return;                        /* no minutes */
    g.DelayedPen |= PFLAG(o, pfteam) ? 4 : 2;
    int was = o->pflags2 & 0x10;
    o->pflags2 |= 0x10;
    if (was) wr16(a1, 0);                                 /* he already has one pending: drop this */
}

/* penalty94.asm:PenaltyManager. Called every game loop pass (d7 = elapsed frames). */
void PenaltyManager(void)
{
    updatepentime();
    checkfornewpen();
    chkprogress();
    UpdatePA();
}

/* penalty94.asm:chkprogress. Control the progress of the ref and the game through the
 * penalty events: the penalty shot message timer, then when the stop delay runs out and a
 * penalty with minutes is pending switch to the horizontal rink for the player to enter the
 * box (InProgress takes it from there). */
void chkprogress(void)
{
    if (g.BA_PS_flags & HI(2)) {
        if (g.BA_PS_flags & HI(7)) {
            PenaltyShotBox();
            g.BA_PS_flags &= (uint16_t)~HI(7);
            return;
        }
        if ((int16_t)g.msgtimer < 0) goto cont;
        g.msgtimer--;
        if ((int16_t)g.msgtimer >= 0) return;
        g.sflags2 &= (uint16_t)~HI(sf2drec);
        g.gmode2 &= (uint16_t)~HI(7);
        printz(str_chkprogress_1, sizeof str_chkprogress_1);
        eraser(0x1B, (g.gmode2 & HI(0)) ? 0xC : 8, 0x7FF);   /* the message area */
        return;
    }
cont:
    if (!(g.gmode & (1 << gmpen))) return;
    if ((int16_t)g.Pencntdwn < 0) { InProgress(); return; }
    g.Pencntdwn = (uint16_t)(g.Pencntdwn - elapsed);
    if ((int16_t)g.Pencntdwn >= 0) return;
    g.gmode &= (uint8_t)~(1 << gmpendel);
    uint8_t *a0 = penbuf();
    for (;; a0 += 2) {                                    /* any penalty with minutes? */
        if (rd16(a0) == 0) return;
        if ((int8_t)penalty_entry(a0[0])[1] > 0) break;
    }
    g.sflags2 |= HI(sf2drec);
    uint16_t vc = g.vcount;
    forceblack();
    g.vcount = vc;
    SetHor();
    DecompressGraphicsPlain(ART(RefMap2) + 8, g.ExtraChars);   /* the horizontal ref tiles */
    struct Matchup *gs = NULL;
    int16_t d3 = NewTicker(&gs);
    NewTicker3(d3, gs);
    puck_loose();
    struct SortObj *puck = sortobj(puckscnum);
    assinsert(0x1A, puck);                                /* puckunflip */
    puck->temp1 = 0x1C20;
    ST_W(g.RefStep);
    g.RefCnt = 0;
    UpdatePA();
    g.RefCnt = 0x32;
    PushRef(0);
    g.palcount = 0x18;
}

/* penalty94.asm:PenShotChk. a3 = checker, a2 = player hit, d0 = penalty. If a2 is on a
 * breakaway, no penalty shot is pending and PenShotPenalties has an entry for d0,
 * getBAplayerInfo sets one up and the result is that penalty (also pspenalty). */
uint16_t PenShotChk(uint16_t pen, struct SortObj *hit, struct SortObj *o)
{
    if (!(hit->pflags3 & (1 << pf3break)) || (g.BA_PS_flags & HI(3))) return pen;
    int16_t d1 = (int16_t)rd16(rom_ptr(SYM_PenShotPenalties) + pen);
    if (d1 < 0) return pen;
    if (getBAplayerInfo((uint16_t)hit->SCnum, o) & CCR_N) {
        g.BA_PS_flags &= (uint16_t)~HI(3);
        return pen;
    }
    g.pspenalty = (uint16_t)d1;
    return (uint16_t)d1;
}

/* penalty94.asm:getBAplayerInfo. d0 = SCnum of the player on the breakaway, a3 = the checker.
 * With penalties on and the other team's goalie in, set BA_PS_flags bit 3 and save the skater,
 * team, goalie and the three roster offsets. Returns the ccr: N clear when set up. */
uint8_t getBAplayerInfo(uint16_t skater, const struct SortObj *o)
{
    if (g.OptPen == 0) return CCR_N;
    g.BA_PS_flags |= HI(3);
    int16_t gk = getGoalieSCnum((int16_t)skater > 5 ? 5 : 0xB);   /* the other team's goalie */
    if (gk < 0) {
        g.BA_PS_flags &= (uint16_t)~HI(3);
        return CCR_N;
    }
    g.BA_Sktr_SCnum = skater;
    const struct SortObj *sk = sortobj((int16_t)skater);
    g.BA_Team = PFLAG(sk, pfteam) ? 1 : 0;
    g.BA_Skater_Offset = sk->pnum;
    g.BA_Goalie_SCnum = (uint16_t)gk;
    g.BA_Goalie_Offset = sortobj(gk)->pnum;
    g.BA_Checker_Offset = o->pnum;
    return (uint8_t)((int8_t)o->pnum < 0 ? CCR_N : 0);
}

/* penalty94.asm:InProgress. Ref in progress: update graphics, stats and the penalty
 * information. Takes the last PenBuf entry; the first pass logs a penalty with minutes and
 * sends the player to the box, later passes wait until he is off the ice and drop the entry.
 * When PenBuf is empty, recount the players and go to the faceoff or the penalty shot. */
void InProgress(void)
{
    if ((int16_t)g.RefCnt >= 0) return;                   /* animation in progress */
    if ((int16_t)g.RefStep >= 0) return;
    for (;;) {
        uint8_t *a0 = penbuf();
        if (rd16(a0) == 0) break;
        while (rd16(a0) != 0) a0 += 2;
        a0 -= 2;                                          /* the last entry */
        int called = a0[1] & 0x80;
        a0[1] = (uint8_t)(a0[1] & 0x7F);
        if (!called) {
            if (sortobj(a0[1])->position >= 0) return;       /* still on the ice */
            wr16(a0, 0);
            return;
        }
        uint16_t pen = a0[0];
        const uint8_t *e = penalty_entry(pen);
        g.sflags5 &= (uint8_t)~0x20;
        uint16_t d2 = e[1];
        if (d2 == 0 || (int8_t)d2 < 0) {                  /* no player involved */
            wr16(a0, 0);
            if (g.sflags & HI(sfhor)) continue;
            SetPA(pen);
            return;
        }
        if ((uint8_t)d2 == 5) g.sflags5 |= 0x20;          /* a major: matched as coincidental */
        uint16_t t = GetPeriodTimeRemaining();            /* log time, penalty and player */
        uint8_t *a4 = g.PenSum + g.PenSumLength;
        if (g.PenSumLength != 0xEC) g.PenSumLength += 4;
        wr16(a4, t);
        a4[2] = (uint8_t)pen;
        struct SortObj *pl = sortobj(a0[1]);
        uint16_t d0 = 0;
        struct Team *a2 = team_struct(0), *a1 = team_struct(1);
        if (PFLAG(pl, pfteam)) {
            a4[2] = (uint8_t)(a4[2] | 0x80);
            d0 = 0x8000;
            struct Team *tt = a1; a1 = a2; a2 = tt;
        }
        a2->Penalties++;
        a2->Penmin += d2;
        d0 = (uint16_t)((d0 & 0xFF00) | pl->pnum);
        a4[3] = pl->pnum;
        g.TempPlOffset = d0;
        int16_t p = (int8_t)pl->pnum;
        a2->ppim[p] += d2;                                        /* his penalty minutes */
        p = (int16_t)(p * 2);
        d2 = (uint16_t)((uint16_t)(int8_t)d2 * 0x3C);
        d2 |= 0x2000;
        uint16_t old = a2->pdst[p / 2];
        if ((int16_t)old >= 0 && (old & 0x1000)) d2 |= 0x1000;   /* keep bit 12 of an old time */
        a2->pdst[p / 2] = d2;
        d2 &= 0xEFFF;
        for (int16_t d1 = 0x32; d1 >= 0; d1 -= 2) {       /* the same time on the other team: coincidental */
            if ((a1->pdst[d1 / 2] & 0xEFFF) != d2) continue;
            if (!(g.sflags5 & 0x20)) continue;
            a1->pdst[d1 / 2] |= 0x4000;
            a2->pdst[p / 2] |= 0x4000;
            break;
        }
        uint8_t *box = a2->penbox;                        /* add him to the penalty box list */
        for (int i = 0; i <= 0x18; i++)
            if ((int8_t)*box++ < 0) break;
        box[-1] = (uint8_t)p;
        *box = 0xFF;
        assreplace(0xC, pl);                              /* asspenalty */
        SetPA(pen);
        USBoard();
        return;
    }
    coinsearch(team_struct(0));
    coinsearch(team_struct(1));
    g.gmode &= (uint8_t)~(1 << gmpen);
    g.BA_PS_flags &= (uint16_t)~HI(6);
    assreplace((g.BA_PS_flags & HI(3)) ? 0x1E : 0x1B, sortobj(puckscnum));   /* puckshootout / puckfaceoff */
}

/* penalty94.asm:coinsearch. a2 = team. Clears tmpdst bit 5, counts the players kept off the
 * ice by penalties (coincidental and bit 4 times do not count, at most 2) and sets tmap. */
void coinsearch(struct Team *tm)
{
    uint16_t d1 = 6;
    for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2) {
        uint16_t st = tm->pdst[d0 / 2];
        if ((int16_t)st <= 0) continue;
        st &= (uint16_t)~0x2000;
        tm->pdst[d0 / 2] = st;
        if (st & 0x4000) continue;
        if (st & 0x1000) continue;
        if (d1 == 4) continue;                            /* never below 4 players */
        d1--;
    }
    tm->ap = d1;
}

/* penalty94.asm:checkfornewpen. Look for a new penalty (entered through AddPenalty). Stops
 * play at once for penalties without minutes or when the guilty team does not have the puck,
 * otherwise starts a delayed penalty call. */
void checkfornewpen(void)
{
    if (g.sflags & HI(sfhor)) return;
    uint8_t *a0 = penbuf();
    for (;;) {
        if (rd16(a0) == 0) return;
        a0 += 2;
        if (!(g.gmode & (1 << gmpen))) {
            const uint8_t *e = penalty_entry(a0[-2]);
            int delayed = 0;
            if (e[1] != 0) {
                int16_t d0 = puckc();
                if (d0 < 0) delayed = 1;
                else {
                    d0 = (int16_t)(d0 - 6);
                    int16_t d1 = (int16_t)((int8_t)a0[-1] - 6);
                    if ((int16_t)(d0 ^ d1) < 0) delayed = 1;   /* the other team has the puck */
                }
            }
            if (delayed) {
                int was = g.gmode & (1 << gmpendel);
                g.gmode |= 1 << gmpendel;
                if (!was) SetPA(0x2C);                    /* delayed penalty */
                continue;
            }
            Stop4Pen(ram_addr(a0), NULL);
        }
        int was = a0[-1] & 0x80;
        a0[-1] = (uint8_t)(a0[-1] | 0x80);
        if (was) continue;
        const uint8_t *e = penalty_entry(a0[-2]);         /* the longest stop delay wins */
        uint16_t d1 = (uint16_t)(e[0] << 5);
        if ((int16_t)d1 <= (int16_t)g.Pencntdwn) continue;
        g.Pencntdwn = d1;
    }
}

/* penalty94.asm:Stop4Pen. a0 = PenBuf entry + 2 (the penalty number and player sit before it;
 * puckfaceoff and puckshootout branch here with a0 = their own code). Stop the clock, set the
 * faceoff spot from the penalty type, blow the whistle. */
void Stop4Pen(uint32_t a0, struct SortObj *o)
{
    int was = g.gmode & (1 << gmclock);
    g.gmode |= 1 << gmclock;
    uint16_t leak = 0;                                    /* d0 for stop_current_song (the caller's when the clock was stopped: not followed) */
    if (!was) {
        int16_t d0 = 0, d1 = 0;
        uint8_t pen = m8(a0 - 2);
        if (pen != 0xE) {                                 /* a goal: center ice */
            d0 = (int16_t)g.ltx;
            d1 = (int16_t)g.lty;
            if (pen != 6) {                               /* out of play: the last touch */
                d0 = puckx();
                d1 = pucky();
                if (pen == 0xC) {                         /* icing: the far end */
                    const struct SortObj *pl = sortobj((int8_t)m8(a0 - 1));
                    d1 = PFLAG(pl, pfgoal) ? -0x258 : 0x258;
                }
            }
        }
        int16_t x0 = d0;
        if (d0 >= 0x46) d0 = 0x46;
        if (d0 <= -0x46) d0 = -0x46;
        if (d1 >= 0xA6) { d1 = 0xCE; d0 = x0 < 0 ? -0x46 : 0x46; }
        if (d1 <= -0xA6) { d1 = -0xCE; d0 = x0 < 0 ? -0x46 : 0x46; }
        g.fox = (uint16_t)d0;
        g.foy = (uint16_t)d1;
        limitfo();                                        /* keeps d0 */
        leak = (uint16_t)d0;
    }
    g.Pencntdwn = 0;
    g.gmode |= 1 << gmpen;
    stop_current_song(leak);
    sfx(SYM_SFXwhistle);
    SetPA(0xA);                                           /* the whistle */
}

/* penalty94.asm:limitfo. Limit the faceoff to 5-20 feet from the walls, for every player in
 * PenBuf (the first entry is looked at even when empty). */
void limitfo(void)
{
    uint8_t *a0 = penbuf();
    do {
        const struct SortObj *pl = sortobj(a0[1] & 0x7F);
        int16_t d1 = 0x58;
        if (!PFLAG(pl, pfgoal)) {
            d1 = (int16_t)-d1;
            if (d1 < (int16_t)g.foy) goto next;
            g.foy = (uint16_t)-0x41;
        } else {
            if (d1 > (int16_t)g.foy) goto next;
            g.foy = 0x41;
        }
        g.fox = (uint16_t)((int16_t)g.fox < 0 ? -0x46 : 0x46);
next:
        a0 += 2;
    } while (rd16(a0) != 0);
}

/* penalty94.asm:UpdatePA. Animate the ref in the ref window. Runs ShowThreeStars for game
 * over, and the horizontal penalty message line is reprinted when penmsgtimer runs out. */
void UpdatePA(void)
{
    if ((int16_t)g.RefCnt < 0) return;
    g.RefCnt = (uint16_t)(g.RefCnt - elapsed);
    if ((int16_t)g.RefCnt < 0) SetPA2();
    if (g.RefPen == 4) ShowThreeStars();
    g.penmsgtimer = (uint16_t)(g.penmsgtimer - elapsed);
    if ((int16_t)g.penmsgtimer >= 0) return;
    g.penmsgtimer = 0x7FFF;
    ClearPenaltyMessage();
    printz(str_UpdatePA_1, sizeof str_UpdatePA_1);
    uint32_t a1 = GetPlayerName();
    g.printx -= m16(a1) >> 1;                             /* center it */
    print(a1);
}

/* penalty94.asm:SetPA. Start the ref animation d0 (the penalty number). Animations from $2E up
 * are ignored; a goal shows the goal box and the shootout its result. */
void SetPA(uint16_t pen)
{
    g.RefPen = pen;
    if ((int16_t)pen >= 0x2E) return;
    g.RefStep = 0;
    prefmes((int16_t)pen);
    if (pen == 0xE) ShowGoalBox();
    if ((g.gmode2 & HI(0)) && (g.gmode2 & HI(3))) ShootoutWonBy();
    g.penmsgtimer = 0x7FFF;
    if (g.sflags & HI(sfhor)) g.penmsgtimer = 0x3C;
    SetPA2();
}

/* penalty94.asm:SetPA2. The next frame / delay pair of the ref animation of RefPen (frame in
 * the high byte, delay * 8 frames in the low byte; negative = the last one). The delay is
 * doubled when the shootout is decided. */
void SetPA2(void)
{
    uint16_t d0 = 0x40;                                   /* clear the ref window */
    if ((int16_t)g.RefStep >= 0) {
        d0 = g.RefStep;
        g.RefStep += 2;
        const uint8_t *a0 = penalty_entry(g.RefPen) + 2;
        a0 += rd16(a0);
        d0 = rd16(a0 + d0);
        if ((int16_t)d0 < 0) {
            d0 = (uint16_t)-d0;
            ST_W(g.RefStep);
        }
        g.RefCnt = (uint16_t)((d0 & 0xFF) << 3);
        if ((g.gmode2 & HI(0)) && (g.gmode2 & HI(3))) g.RefCnt = (uint16_t)(g.RefCnt * 2);
        d0 >>= 8;
    }
    PushRef(d0);
}

/* penalty94.asm:PushRef. Tell vblank what to display: d0 = ref frame into RefRamMap, $40
 * clears the window. The horizontal ref uses RefMap2. */
void PushRef(uint16_t frame)
{
    uint8_t *a1 = g.RefRamMap;
    if (frame == 0x40) {
        if (!(g.sflags & HI(sfhor))) {                    /* horizontal mode only clears the line */
            for (int i = 0; i < 0x38; i++) wr16(a1 + i * 2, 0x7FF);
            g.sflags2 |= HI(sf2refref);
        }
        prefmes(-1);
        return;
    }
    const uint8_t *a0 = rom_ptr((g.sflags & HI(sfhor)) ? ART(RefMap2) : ART(RefsMap));
    a0 += rd32(a0 + 4);
    a0 += 4;
    a0 += (int16_t)(uint16_t)(frame * 0x70);
    uint16_t d2 = g.ExtraChars;
    if (!(g.sflags & HI(sfhor))) d2 |= 0x8000;            /* priority */
    for (int i = 0; i < 0x38; i++) wr16(a1 + i * 2, (uint16_t)(rd16(a0 + i * 2) + d2));
    g.sflags2 |= HI(sf2refref);
}

/* penalty94.asm:prefmes. Print the message for penalty d0 (negative clears it). Vertical
 * mode: framed under the ref. Horizontal mode: one centered line on row $B. */
void prefmes(int16_t pen)
{
    if (g.sflags & HI(sfhor)) {
        ClearPenaltyMessage();
        if (pen < 0) return;
        printz(str_prefmes_3, sizeof str_prefmes_3);
        const uint8_t *a1 = penalty_entry((uint16_t)pen) + 2;
        g.printx -= rd16(a1) >> 1;
        print((uint32_t)(a1 - rom_image));
        return;
    }
    if (pen < 0) {
        printz(str_prefmes_1, sizeof str_prefmes_1);
        eraser(0xD, 3, 0x7FF);
        return;
    }
    printz(str_prefmes_2, sizeof str_prefmes_2);
    const uint8_t *a1 = penalty_entry((uint16_t)pen) + 2;
    uint16_t d0 = (uint16_t)(rd16(a1) - 2);
    if (d0 == 0) return;                                  /* empty string */
    g.printx -= d0 >> 1;
    if ((int16_t)g.printx < 0) g.printx = 0;
    d0 = rd16(a1);
    if (a1[d0 - 1] == 0) d0--;
    Framer(d0, 3);
    g.printy -= 2;
    g.printx += 1;
    print((uint32_t)(a1 - rom_image));
}

/* penalty94.asm:ClearPenaltyMessage. Blank the horizontal mode penalty message line. */
void ClearPenaltyMessage(void)
{
    printz(str_ClearPenaltyMessage_1, sizeof str_ClearPenaltyMessage_1);
}

/* penalty94.asm:PenGoalStuff. After a goal: a1 = scored on team, a2 = scoring team. Clears
 * PenBuf only when the scored on team has 6 on the ice. A power play goal releases the first
 * scored on player in the box without a coincidental penalty. */
void PenGoalStuff(struct Team *a1, struct Team *a2)
{
    if (a1->ap == 6) ClearPenaltyBuffer();
    if ((int16_t)a2->ap <= (int16_t)a1->ap) return;
    uint8_t *a0 = a1->penbox;
    for (;;) {
        uint8_t d2 = *a0++;
        if ((int8_t)d2 < 0) return;
        if (a1->pdst[d2 / 2] & 0x4000) continue;          /* coincidental */
        a1->pdst[d2 / 2] = 0;
        RemovePlayerFromList(a0);
        g.DelayedPen |= 1;
        a1->ap++;
        a2->PwrGoals++;
        team_struct(0)->flags |= 1;                                                /* tmflcc: line changes */
        team_struct(1)->flags |= 1;
        return;
    }
}

/* penalty94.asm:ClearPenaltyBuffer. */
void ClearPenaltyBuffer(void)
{
    uint8_t *a0 = penbuf();
    for (int i = 0; i < 32; i++) wr16(a0 + i * 2, 0);
}

/* penalty94.asm:updatepentime. Update the time remaining on all penalized players, once a
 * second (sflags3 bit 6 marks that tick). */
void updatepentime(void)
{
    g.sflags3 &= (uint8_t)~0x40;
    if (g.gmode & (1 << gmclock)) return;
    g.Penaltytimer = (uint16_t)(g.Penaltytimer - elapsed);
    if ((int16_t)g.Penaltytimer >= 0) return;
    g.Penaltytimer += 0x18;
    g.sflags3 |= 0x40;
    chkatop();
    updatePPTeamTime();
    ProcessPenaltyList(team_struct(0));
    ProcessPenaltyList(team_struct(1));
}

/* a player's box time is 5 seconds or less: beep, and release him at 0 */
static void penbox_check(struct Team *tm, uint16_t d0)
{
    if ((int16_t)tm->pdst[d0 / 2] > 5) return;
    int16_t snd = SYM_SFXbeep1;
    if (tm->pdst[d0 / 2] == 0) {
        releasepl(tm, d0);
        snd = SYM_SFXbeep2;
    }
    sfx(snd);
}

/* penalty94.asm:ProcessPenaltyList. a2 = team. Walk the penalty box list: the first two
 * players without a coincidental penalty count down one second and the rest wait; beep when
 * the first served time gets to 5 or less and release the player at 0. Coincidental penalties
 * count down on their own. */
void ProcessPenaltyList(struct Team *tm)
{
    uint8_t *a0 = tm->penbox;
    uint8_t *a1 = g.mesarea;                              /* the serving players (the message area as scratch) */
    int16_t d1 = 2;
    for (;;) {
        uint8_t d0 = *a0++;
        if ((int8_t)d0 < 0) break;
        uint16_t st = tm->pdst[d0 / 2];
        if (st & 0x4000) {                                /* coincidental: on its own clock */
            st--;
            tm->pdst[d0 / 2] = st;
            if (!(st & 0x0800)) continue;
            tm->pdst[d0 / 2] = (st & 0x1000) ? 0 : 0x1000;
            RemovePlayerFromList(a0);
            return;
        }
        if (--d1 < 0) continue;                           /* third or later player waits */
        wr16(a1, d0);
        a1 += 2;
        st--;
        tm->pdst[d0 / 2] = st;
        if (st == 0) RemovePlayerFromList(a0);            /* time is up */
    }
    uint16_t first = rd16(g.mesarea), second = rd16(g.mesarea + 2);   /* (the message area as scratch) */
    if (d1 == 1) { penbox_check(tm, first); return; }
    if (d1 == 0) { penbox_check(tm, first); penbox_check(tm, second); return; }
    if (d1 == -1) penbox_check(tm, second);
}

/* penalty94.asm:RemovePlayerFromList. Remove the entry before a0 from a penalty box list by
 * shifting the rest down (the list ends with a negative byte). Returns the removed slot. */
uint8_t *RemovePlayerFromList(uint8_t *a0)
{
    int d2 = 0;
    for (;;) {
        a0[d2 - 1] = a0[d2];
        if ((int8_t)a0[d2] < 0) break;
        d2++;
    }
    return a0 - 1;
}

/* penalty94.asm:chkatop. Attack time of possession stat, once a second. */
void chkatop(void)
{
    uint16_t d1 = 0;
    int16_t d0 = pucky();
    if (!(d0 > 0x58)) {
        d1 = tmsize;
        d0 = (int16_t)-d0;
        if (d0 < 0x58) return;
    }
    if (g.gmode & (1 << gmdir)) d1 ^= tmsize;
    struct Team *tm = team_struct(d1 != 0);
    tm->ATOP++;
}

/* penalty94.asm:releasepl. A player's penalty time is up so let him out: a2 = team, d0 =
 * player * 2. He takes the first sort object not on the ice, at the next position in
 * importance (priolist, after the goalie when there is one). */
void releasepl(struct Team *tm, uint16_t d0)
{
    struct SortObj *a3 = sortobj_at(tm->sort);
    while (a3->position >= 0) a3++;
    uint16_t d3 = d0 >> 1;
    uint16_t d1 = tm->ap;
    tm->ap = (uint16_t)(d1 + 1);
    team_struct(0)->flags |= 1;
    team_struct(1)->flags |= 1;
    const uint8_t *a0 = rom_ptr(SYM_priolist);
    if ((int16_t)tm->goalie < 0) a0++;
    a3->position = a0[d1];
    Setplass(a3);
    setplayer(d3, a3);
    a3->pflags2 |= 4;
}

/* penalty94.asm:GetLowestPen. a2 = shorthanded team, a3 = team on the power play. Returns
 * the power play time left from the penalty box times. */
uint16_t GetLowestPen(struct Team *a2, struct Team *a3)
{
    uint16_t d0 = 0, d3 = 0;
    const uint8_t *a0 = a2->penbox;
    for (;;) {
        uint8_t i = *a0++;
        if ((int8_t)i < 0) break;
        uint16_t d2 = a2->pdst[i / 2];
        if (d2 & 0x4000) continue;                        /* coincidental */
        d2 = (uint16_t)(d2 - d3);
        d0 = (uint16_t)(d0 + d2);
        d3 = d2;
    }
    if (a3->ap == 6) return d0;                           /* 6 on the ice: done */
    d0 = (uint16_t)(d0 - d3);
    a0 = a3->penbox;
    for (;;) {
        uint8_t i = *a0++;
        if ((int8_t)i < 0) return d0;
        if ((uint8_t)(a3->pdst[i / 2] >> 8) & 0x40) continue;
        if ((int16_t)d0 < (int16_t)a3->pdst[i / 2]) return d0;
        return (uint16_t)(d0 + d3);
    }
}

/* the end of a power play: the EASN logo comes back */
static void clrpwrplay(void)
{
    int was = g.sflags2 & HI(sf2pwrplay);
    g.sflags2 &= (uint16_t)~HI(sf2pwrplay);
    if (!was) return;
    EASNLogo();
}

/* penalty94.asm:updatepwrplay. Show the graphic and time remaining for a power play (nothing
 * when gmode2 bit 1 is set). Prints the time and the team on the power play. */
void updatepwrplay(void)
{
    if (g.gmode2 & HI(1)) return;
    struct Team *a2 = team_struct(0), *a3 = team_struct(1);
    int16_t d0 = (int16_t)(a2->ap - a3->ap);
    if (d0 == 0) { clrpwrplay(); return; }                /* the same number on the ice */
    if (d0 < 0) {
        if (!(g.sflags2 & HI(sf2pwrtm))) {
            clrpwrplay();
            g.sflags2 |= HI(sf2pwrtm);
        }
    } else {
        struct Team *t = a2; a2 = a3; a3 = t;
        if (g.sflags2 & HI(sf2pwrtm)) {
            clrpwrplay();
            g.sflags2 &= (uint16_t)~HI(sf2pwrtm);
        }
    }
    int was = g.sflags2 & HI(sf2pwrplay);
    g.sflags2 |= HI(sf2pwrplay);
    if (!was) a3->PwrPlays++;
    printz(str_updatepwrplay_1, sizeof str_updatepwrplay_1);
    print(PushTime(GetLowestPen(a2, a3)));
    uint32_t a0 = a3->data;
    uint32_t mes = ram_addr(g.mesarea);
    wr16(g.mesarea, 2);
    appendz(str_updatepwrplay_2, sizeof str_updatepwrplay_2, mes);
    a0 += rd16(rom_ptr(a0) + 4);                          /* the team name */
    a0 += rd16(rom_ptr(a0));
    appstring(a0, mes);
    print(mes);
}

/* penalty94.asm:ClrHor. Revert the graphics to the vertical ice rink: reload the rink tiles
 * and the home team's graphics, redraw the scoreboard. */
void ClrHor(void)
{
    g.sflags &= (uint16_t)~HI(sfhor);
    g.Oldrow = 0x3E8;
    uint16_t d4 = DecompressGraphicsPlain(ART(Rinktiles), g.rinkvrcset);
    LoadHomeTeamGfx(d4);
    SprSort();
    if (g.sflags & HI(sfpz)) return;
    printz(str_ClrHor_1, sizeof str_ClrHor_1);
    eraser(0x20, 0x1C, 0x7FF);
    printscores1();
}

/* penalty94.asm:SetHor. Switch the graphics to the horizontal ice rink. */
void SetHor(void)
{
    g.sflags |= HI(sfhor);
    while (g.disflags & HI(dfok)) hal_wait_vblank();      /* wait for dfok clear */
    g.Hscroll = (uint16_t)view_shift();                   /* (the wide view: the 256-wide rink in the middle) */
    g.Vscroll = 0;
    SprSort();
    if (view_shift()) DoFill(0x800, g.maps[1].vram, 0x7FF);     /* (and nothing beside it) */
    printz(str_SetHor_1, sizeof str_SetHor_1);
    uint32_t a1 = ART(HorRinkMap) + rd32(rom_ptr(ART(HorRinkMap)) + 4);
    dobitmap(0, a1, ART(HorRinkTiles), 0, 0, 0x20, 0x1C, g.rinkvrcset, 0);   /* a0 (the palette) is not set */
    USBoard();
    if (g.sflags & HI(sfpz)) return;
    DoFill(0x800, g.maps[0].vram, 0x7FF);
}

/* penalty94.asm:printscores1. Draw the scoreboard. Vertical rink: the period box and the
 * score box (team names and scores). Horizontal rink: the period and both team logos with big
 * scores. */
void printscores1(void)
{
    if (g.sflags & HI(sfhor)) {
        printz(str_printscores1_6, sizeof str_printscores1_6);
        uint16_t d0 = g.gsp;
        if (g.gmode2 & HI(0)) d0 = 4;
        uint32_t a1 = AdvanceStringPtr(SYM_PerLabels2, d0);   /* the period name, centered */
        g.printx -= rd16(rom_ptr(a1)) >> 1;
        print(a1);
        printz(str_printscores1_7, sizeof str_printscores1_7);
        PrintTeamLogoAndScore(team_struct(0), 0);
        printz(str_printscores1_8, sizeof str_printscores1_8);
        PrintTeamLogoAndScore(team_struct(1), 0x2C);
        return;
    }
    int was = g.sflags5 & 0x80;
    g.sflags5 &= (uint8_t)~0x80;
    if (was) g.crowdnoisedelay = 0x78;
    printz(str_printscores1_1, sizeof str_printscores1_1);
    Framer(9, 5);
    g.disflags |= HI(dfclock);                                  /* dfclock */
    printz(str_printscores1_2, sizeof str_printscores1_2);
    uint16_t d0 = g.gsp;
    uint32_t list = SYM_PerLabels;
    if (g.gmode2 & HI(1)) { d0 = 4; list = SYM_PerLabels2; }
    PrintStringFromList(list, d0);
    EASNLogo();
    if (g.sflags3 & (1 << sf3llcs)) return;               /* the lower line change box is up */
    printz(str_printscores1_3, sizeof str_printscores1_3);
    Framer(8, 5);
    printz(str_printscores1_4, sizeof str_printscores1_4);
    PrintTeamNameAndScore(team_struct(0));
    printz(str_printscores1_5, sizeof str_printscores1_5);
    PrintTeamNameAndScore(team_struct(1));
}

/* penalty94.asm:PrintTeamNameAndScore. The team name of a2 at printx / printy, then its score
 * as 2 digits at x $1C. */
void PrintTeamNameAndScore(struct Team *tm)
{
    uint32_t a1 = tm->data;
    a1 += rd16(rom_ptr(a1) + 4);
    a1 += rd16(rom_ptr(a1));
    print(a1);
    g.printx = 0x1C;
    print(PushNumberWidth(tm->score, 2));
}

/* penalty94.asm:PrintTeamLogoAndScore. The team logo map (d0 = offset) 5 columns left of
 * printx, then the score of a2 in the big font, centered 2 rows lower. */
void PrintTeamLogoAndScore(struct Team *tm, uint16_t logo)
{
    uint16_t saved = g.printx;
    g.printx -= 5;
    PutTeamBlock(logo);
    g.printx = saved;
    g.printy += 2;
    uint16_t d0 = tm->score;
    if ((int16_t)d0 >= 0xA) g.printx -= 1;
    uint32_t a1 = PushNumber(d0);
    d0 = (uint16_t)((uint16_t)(m16(a1) - 2) >> 1);
    g.printx -= d0;
    g.printx += 1;
    printbig(a1);
}

/* penalty94.asm:EASNLogo. The EASN logo map at x 1, y $19 on the vertical rink if no power
 * play. */
void EASNLogo(void)
{
    if (g.sflags2 & HI(sf2pwrplay)) return;
    printz(str_EASNLogo_1, sizeof str_EASNLogo_1);
    uint32_t a1 = ART(EASNmap) + rd32(rom_ptr(ART(EASNmap)) + 4);
    const uint8_t *m = rom_ptr(a1);
    dobitmap(0, a1, SYM_NoTiles, 0, 0, rd16(m), rd16(m + 2), g.EASNcset, 0);
}

/* the penalty box list of team a2: the players without a coincidental penalty first, then
 * those with one */
static void dispen(struct Team *tm)
{
    for (int pass = 0; pass < 2; pass++) {
        const uint8_t *a0 = tm->penbox;
        for (;;) {
            uint8_t d0 = *a0++;
            if ((int8_t)d0 < 0) break;
            int coin = (tm->pdst[d0 / 2] & 0x4000) != 0;
            if (coin != pass) continue;
            pplpen(tm, d0);
        }
    }
}

/* penalty94.asm:USBoard. Update the scoreboard, including the players in the penalty box and
 * their time remaining. */
void USBoard(void)
{
    g.disflags |= HI(dfclock);
    printscores1();
    printz(str_USBoard_1, sizeof str_USBoard_1);
    dispen(team_struct(0));
    printz(str_USBoard_2, sizeof str_USBoard_2);
    dispen(team_struct(1));
}

/* penalty94.asm:pplpen. One penalty box line: the player's number and time remaining. a2 =
 * team, d0 = roster offset (player * 2). Only rows up to y $A are printed. */
void pplpen(struct Team *tm, uint16_t d0)
{
    if (g.printy > 0xA) return;
    uint16_t d2 = (uint16_t)(tm->pdst[d0 / 2] & 0x7FF);
    const uint8_t *a1 = rom_ptr(tm->data);
    a1 += rd16(a1);
    for (int i = (int)(d0 >> 1); i >= 0; i--) {           /* skip d0 / 2 + 1 roster entries */
        a1 += rd16(a1);
        a1 += 8;
    }
    uint16_t num = a1[-8];                                /* his number (BCD) */
    uint16_t saved = g.printx;
    ConvertByteToDigits(num, text_buffer());
    wr16(g.mesarea, 4);                                   /* string length: 2 digits */
    print(ram_addr(g.mesarea));
    print(PushTime(d2));
    g.printx = saved;
    g.printy += 1;
}

/* penalty94.asm:linebar. The energy bar of line d0 for team a2 at printx / printy (16
 * steps). */
void linebar(uint16_t line, struct Team *tm)
{
    uint16_t e = getlinee(line, tm);
    uint16_t saved = g.printa;
    g.printa = 0x8000;
    uint16_t d0 = (uint16_t)m68k_divu((uint32_t)(int32_t)(int16_t)e, 0x100);
    if (d0 > 0xF) d0 = 0xF;
    uint16_t d1 = (uint16_t)(0xF - d0);                   /* bar frame, 15 = empty */
    uint32_t a1 = ART(EnergyBarMap) + rd32(rom_ptr(ART(EnergyBarMap)) + 4);
    dobitmap(0, a1, SYM_NoTiles, 0, d1, rd16(rom_ptr(a1)), 1, g.energybarchars, 0);
    g.printa = saved;
}

/* penalty94.asm:getlinee. d0 = line number, a2 = team struct. Returns the average energy of
 * that line's skaters. */
uint16_t getlinee(uint16_t line, struct Team *tm)
{
    const uint8_t *a1 = tm->lines[line];                    /* the line set */
    const uint8_t *prio = rom_ptr(SYM_priolist);
    uint32_t d0 = 0;
    uint16_t d1 = 0;
    for (int d4 = (int)tm->ap - 1; d4 >= 0; d4--) {
        uint8_t d5 = prio[d4];
        if (d5 == 0) continue;                            /* the goalie's energy does not count */
        d1++;
        d0 += tm->pde[a1[d5] - 1];
    }
    return (uint16_t)m68k_divu(d0, d1);
}

/* penalty94.asm:AvgCline. The average energy of the current line on team a2. */
uint16_t AvgCline(struct Team *tm)
{
    uint32_t d0 = 0;
    uint16_t d1 = 0;
    const struct SortObj *a0 = sortobj_at(tm->sort);
    for (int i = 0; i < 6; i++, a0++) {
        if (a0->position <= 0) continue;
        d0 += tm->pde[a0->pnum];
        d1++;
    }
    if (d1 == 0) return (uint16_t)d0;
    return (uint16_t)m68k_divu(d0, d1);
}

/* penalty94.asm:ChkShotStat. Determine if a shot was taken and add it to the stats: crowd,
 * team (power play and period shots), shooter and goalie shots against. A second path counts
 * the shot for the player in the last ScoreSum entry. */
void ChkShotStat(void)
{
    if (g.gmode & (1 << gmclock)) return;
    if (g.gmode & (1 << gmhl)) return;
    int16_t d0;
    int was = g.sflags2 & HI(sf2shot);
    g.sflags2 &= (uint16_t)~HI(sf2shot);
    if (!was) {
        if (!(g.sflags4 & HI(0))) return;
        int was8 = g.sflags8 & 8;
        g.sflags8 &= (uint8_t)~8;
        if (!was8) { g.sflags8 |= 8; return; }            /* every second call */
        uint8_t *a4 = g.ScoreSum + -6 + g.ScoreSumbytes;                                  /* the last goal entry */
        struct Team *tm = team_struct((a4[2] & 0x80) != 0);
        uint8_t pn = a4[3];
        const struct SortObj *p = sortobj_at(tm->sort);
        int i;
        for (i = 0; i < 6; i++, p++)
            if (p->pnum == pn) break;
        if (i == 6) return;
        d0 = p->SCnum;
    } else {
        g.crowdlevel += 0x64;
        g.CwdExciteLvl += 0xA;
        d0 = (int16_t)g.shotplayer;
    }
    struct SortObj *a3 = sortobj(d0);
    struct Team *a1;
    struct Team *a2 = loadTeamStruct(a3, &a1);
    a2->shots++;                                          /* tmshots */
    if (g.sflags2 & HI(sf2pwrplay)) {
        int pp_away = (g.sflags2 & HI(sf2pwrtm)) != 0;
        if ((PFLAG(a3, pfteam) != 0) == pp_away)
            a2->ppshots++;                                        /* power play shots */
    }
    a2->pershots[g.gsp]++;                                                        /* shots by period */
    a2->pshots[a3->pnum]++;
    int16_t gk = (int16_t)a1->goalie;
    if (gk < 0) return;                                   /* empty net */
    a1->pshots[gk]++;
}

/* refill one team (a2) and pick the other's (a3) starting line from the players on the ice */
static void intermission_team(struct Team *a2, struct Team *a3)
{
    reenergizeteam(a2);
    a3->line = 0;
    if (g.OptLine != 0) return;
    int16_t d0 = (int16_t)(a3->ap - a2->ap);
    if (d0 == 0) return;
    a3->line = 3;                                         /* Pw1 */
    if (d0 >= 0) return;
    a3->line = 5;                                         /* PK1 */
}

/* penalty94.asm:SetupTeamForIntermission. Reset the bench, then for each team refill the
 * energy and pick the starting line. */
void SetupTeamForIntermission(void)
{
    ResetBench();
    intermission_team(team_struct(0), team_struct(1));
    intermission_team(team_struct(1), team_struct(0));
}

/* penalty94.asm:RestoreTeamEnergy. a2 = team: max energy for every player. */
void RestoreTeamEnergy(struct Team *tm)
{
    for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2)
        tm->pde[d0 / 2] = 0x1000;
}

/* penalty94.asm:reenergizeteam. Max energy for team a2 except the players at tmpdst -4;
 * players marked -3 go to the bench (-2). */
void reenergizeteam(struct Team *tm)
{
    for (int16_t d0 = 0x32; d0 >= 0; d0 -= 2) {
        if (tm->pdst[d0 / 2] == 0xFFFC) continue;
        tm->pde[d0 / 2] = 0x1000;
        if (tm->pdst[d0 / 2] == 0xFFFD) tm->pdst[d0 / 2] = 0xFFFE;
    }
}

/* the weights at nibblebuffer two to a long, as getscore adds them (add.l) */
static uint32_t nib_long(int k) { return (uint32_t)g.nibblebuffer[2 * k] << 16 | g.nibblebuffer[2 * k + 1]; }
static void set_nib_long(int k, uint32_t v) { g.nibblebuffer[2 * k] = (uint16_t)(v >> 16); g.nibblebuffer[2 * k + 1] = (uint16_t)v; }

/* the weighted goals of one team's period: d0 = scoring team, d1 = the other team. Adds the
 * scoring team's (bits 4-6) and the other team's (bits 0-2) sctab rows as weights */
static uint16_t getscore(uint16_t d0, uint16_t d1)
{
    const uint8_t *tl = rom_ptr(LG(TeamList));
    const uint8_t *a1 = rom_ptr(rd32(tl + d0 * 4));
    a1 += rd16(a1 + 8);
    unsigned row = (a1[0] & 0x70) >> 1;                   /* row * 8 */
    set_nib_long(0, ((uint32_t)sctab[row / 2] << 16) | sctab[row / 2 + 1]);
    set_nib_long(1, ((uint32_t)sctab[row / 2 + 2] << 16) | sctab[row / 2 + 3]);
    a1 = rom_ptr(rd32(tl + d1 * 4));
    a1 += rd16(a1 + 8);
    row = (a1[0] & 7) << 3;
    set_nib_long(0, nib_long(0) + (((uint32_t)sctab[row / 2] << 16) | sctab[row / 2 + 1]));
    set_nib_long(1, nib_long(1) + (((uint32_t)sctab[row / 2 + 2] << 16) | sctab[row / 2 + 3]));
    return WeightedRandomSelect(4);
}

/* penalty94.asm:SetScore. Add to the score of the ticker game a0. After period 3 a game
 * within one goal goes to overtime and asks for a highlight. */
void SetScore(struct Matchup *a0)
{
    if ((int16_t)a0->per >= 4) return;
    if (a0->per == 3) {
        a0->per = 5;                                      /* final */
        int16_t d0 = (int16_t)(a0->score1 - a0->score2);
        if (d0 > 1) return;
        if (d0 < -1) return;
        a0->per = 3;                                      /* close game: overtime */
        a0->flags |= MU_HIGHLIGHT;
        return;
    }
    a0->per++;
    uint16_t s = getscore(a0->team1, a0->team2);
    a0->score1 += s;
    s = getscore(a0->team2, a0->team1);
    a0->score2 += s;
}

/* penalty94.asm:InitScores. Initialize the other games' scores / period in the playoffs. */
void InitScores(void)
{
    if ((int16_t)g.gamelevel > 1) return;
    int16_t d1 = (int16_t)GetShifter(NULL);
    for (; d1 >= 0; d1--) {
        struct Matchup *a0 = &g.matchups[d1];
        if ((uint16_t)d1 == g.gamenum) continue;
        if (a0->flags & MU_SERIES_OVER) continue;
        a0->per = 0;
        for (int16_t d0 = (int16_t)randomd0(3); d0 > 0; d0--) SetScore(a0);
    }
}

/* penalty94.asm:UpdateScores. Update the ticker score values (PeriodOver). */
void UpdateScores(void)
{
    int16_t d1 = (int16_t)GetShifter(NULL);
    g.TickerNum = (uint16_t)d1;
    for (; d1 >= 0; d1--) {
        struct Matchup *a0 = &g.matchups[d1];
        if ((uint16_t)d1 == g.gamenum) continue;
        a0->flags &= (uint8_t)~MU_HIGHLIGHT;
        if (a0->flags & MU_SERIES_OVER) continue;
        SetScore(a0);
    }
}

/* penalty94.asm:NewTicker. Show the next ticker score once the clock is at 8:00 or less.
 * Returns d3 = the game, negative for none. */
int16_t NewTicker(struct Matchup **gs)
{
    if ((int16_t)g.gameclock > 0x1E0) return -1;
    return NewTicker2(gs);
}

/* penalty94.asm:NewTicker2. The next ticker game, skipping this game. */
int16_t NewTicker2(struct Matchup **gs)
{
    int16_t d3 = (int16_t)g.TickerNum;
    if (d3 < 0) return d3;
    g.TickerNum--;
    if ((uint16_t)d3 == g.gamenum) return NewTicker2(gs);
    return CheckGameTickerStatus(d3, gs);
}

/* penalty94.asm:CheckGameTickerStatus. d3 = game. Returns a0 = its gstruct, skipping to the
 * next ticker game if a highlight is coming up or the series is over. */
int16_t CheckGameTickerStatus(int16_t d3, struct Matchup **gs)
{
    struct Matchup *a0 = &g.matchups[d3];
    if (a0->flags & MU_HIGHLIGHT) return NewTicker2(gs);  /* a highlight is coming */
    if (a0->flags & MU_SERIES_OVER) return NewTicker2(gs);   /* the series is over */
    *gs = a0;
    return d3;
}

/* one team's name and score on the next ticker row */
static void ticker_team(uint16_t team, uint16_t score, uint16_t x)
{
    g.printx = x;
    g.printy += 1;
    uint32_t a1 = rd32(rom_ptr(LG(TeamList)) + team * 4);
    a1 += rd16(rom_ptr(a1) + 4);                          /* the team name */
    print(a1);
    g.printx = (uint16_t)(x + 0x15);
    print(PushNumberWidth(score, 2));
}

/* penalty94.asm:NewTicker3. Display the ticker score for game d3 (a0 = its gstruct); nothing
 * for a negative d3. */
void NewTicker3(int16_t d3, const struct Matchup *gs)
{
    if (d3 < 0) return;
    uint16_t d0, d1;
    SetTickerAreaPosition(&d0, &d1);
    Framer(d0, d1);
    g.printx += 1;
    g.printy -= 4;
    uint16_t x = g.printx;
    GetShifter(NULL);
    g.printa = 0xA000;
    printz(str_NewTicker3_1, sizeof str_NewTicker3_1);
    g.printx = x;
    d0 = (uint16_t)(1 + g.gamelevel);
    if (g.OptPlayMode == 0) d0 = 0;                       /* not the playoffs: 'EA Hockey Night' */
    PrintStringFromList(SYM_GameLabels, d0);
    uint32_t a1 = AdvanceStringPtr(SYM_PerLabels, (uint16_t)(gs->per - 1));
    d0 = rd16(rom_ptr(a1)) >> 1;                          /* the period name centered on x + $17 */
    g.printx = (uint16_t)(x + 0x17 - d0);
    print(a1);
    g.printa = 0x8000;
    ticker_team(gs->team2, gs->score2, x);
    ticker_team(gs->team1, gs->score1, x);
}

/* penalty94.asm:ClearTickerArea. Erase the ticker box. */
void ClearTickerArea(void)
{
    uint16_t d0, d1;
    SetTickerAreaPosition(&d0, &d1);
    eraser(d0, d1, 0x7FF);
}

/* penalty94.asm:SetTickerAreaPosition. printx / printy / printm for the ticker box (x 3, y
 * $17; map 3 in pause mode). Returns d0 = $1A wide, d1 = 5 high. */
void SetTickerAreaPosition(uint16_t *d0, uint16_t *d1)
{
    printz(str_SetTickerAreaPosition_1, sizeof str_SetTickerAreaPosition_1);
    if (!(g.sflags & HI(sfpz))) printz(str_SetTickerAreaPosition_2, sizeof str_SetTickerAreaPosition_2);
    *d0 = 0x1A;
    *d1 = 5;
}

/* penalty94.asm:PrintStringFromList. Print string d0 of the String list a1 with printsmall. */
void PrintStringFromList(uint32_t list, uint16_t n)
{
    printsmall(AdvanceStringPtr(list, n));
}

/* penalty94.asm:AdvanceStringPtr. a1 = string d0 of the String list a1. */
uint32_t AdvanceStringPtr(uint32_t list, uint16_t n)
{
    for (; n != 0; n--) list += rd16(rom_ptr(list));
    return list;
}

/* ---- the intermission and the highlights of the other games ---- */

#define GS_SAVE_WORDS 0x5BE                               /* StartHL2's .sv: gmode on, to M68K_RAM */

/* StartHL2's .sv / .lo: keep the game's RAM from gmode on (and ScoreSumbytes) at M68K_RAM while
 * a highlight game uses it */
static void hl_save(void)
{
    rp ram = ram_at(RAM_BASE);
    ram_copy(ram, RAM(gmode), 2 * GS_SAVE_WORDS);
    w16(ram, 2 * GS_SAVE_WORDS, g.ScoreSumbytes);
}
static void hl_restore(void)
{
    rp ram = ram_at(RAM_BASE);
    ram_copy(RAM(gmode), ram, 2 * GS_SAVE_WORDS);
    g.ScoreSumbytes = r16(ram, 2 * GS_SAVE_WORDS);
}

/* StartHL2's .ranres: settle highlight game a0 at random when tied, and mark it final */
static void hl_random_result(struct Matchup *gs)
{
    if (gs->score1 == gs->score2) {
        uint16_t d0 = vdp_hv16() & 1;
        gs->score1 += d0;
        gs->score2 += d0 ^ 1;
    }
    gs->per = 5;                                        /* final */
}

/* StartHL2's .setteam: refill the energy, personnel and player data of team a2 */
static void hl_set_team(struct Team *tm)
{
    reenergizeteam(tm);
    setpersonel(tm);
    forcepldata(tm);
    ST_W(tm->lasttouch);
    g.sflags8 &= (uint8_t)~0x80;
    ST_W(tm->assist1);
    ST_W(tm->assist2);
}

/* penalty94.asm:StartHL2. Play the highlight of game a0 (d1 = its number): its ticker score,
 * "Highlight from game:", then the two teams on the rink with a random 60-180 seconds left, the
 * computer playing until 180 frames after the clock stops or C / start (a tie is settled at
 * random); the game's RAM is kept aside meanwhile. Still tied: another highlight. Start skips
 * it with a random result (sf3sbut). */
void StartHL2(struct Matchup *gs, int16_t game)
{
    for (;;) {
        struct Matchup *a0 = gs;
        if (g.sflags3 & (1 << sf3sbut)) { hl_random_result(a0); return; }
        hl_save();
        struct Matchup *t = a0;
        int16_t d3 = CheckGameTickerStatus(game, &t);
        a0 = t;
        NewTicker3(d3, a0);
        if (g.sflags3 & (1 << sf3sbut)) { hl_restore(); hl_random_result(a0); g.sflags3 |= 1 << sf3sbut; return; }
        g.printx = 4;
        g.printy -= 6;
        Framer(0x18, 3);
        g.printx += 1;
        g.printy -= 2;
        printz(str_StartHL2_1, sizeof str_StartHL2_1);
        if (waitxsr(0xB4) & 0x80) { hl_restore(); hl_random_result(a0); g.sflags3 |= 1 << sf3sbut; return; }
        ST_W(g.zamx);
        g.CwdExciteLvl = 0;
        g.HomeTeam = a0->team1;
        g.VisTeam = a0->team2;
        struct Team *home = team_struct(0), *away = team_struct(1);
        home->goalie = 0;
        away->goalie = 0;
        clearTeamStats();
        restoreteams();
        ST_W(g.c1playernum);
        ST_W(g.c2playernum);
        g.cont1team = 0;
        g.cont2team = 0;
        g.gameclock = (uint16_t)(randomd0(0x78) + 0x3C);
        g.gsp = (uint16_t)(a0->per - 1);
        home->score = a0->score1;
        away->score = a0->score2;
        g.gmode = 1 << gmhl;
        if (g.gsp & 1) g.gmode |= 1 << gmdir;
        g.sflags &= 0x00FF;                               /* clr.b: the high byte */
        g.sflags2 = (uint16_t)(HI(sf2drec) | (g.sflags2 & 0xFF));
        g.sflags3 = 0;
        g.disflags &= (uint16_t)~HI(4);
        g.disflags |= HI(dfclock);
        g.glovecords = 0;
        g.iflags = 0;
        ST_W(g.RefCnt);
        g.puckcross[0].frames |= 0xFF00;                  /* (st puckcross2: the high byte) */
        g.puckcross[1].frames |= 0xFF00;
        p_turnoff();
        setupice_highlight();
        ClrHor();
        vdp_ctrl16(SYM_VDPR_WINX);
        vdp_ctrl16(SYM_VDPR_WINY);
        DecompressGraphicsPlain(ART(RefsMap) + 8, g.ExtraChars);   /* the ref cam */
        g.Vpos = 0;
        g.Hpos = 0;
        ResetBench();
        hl_set_team(home);
        hl_set_team(away);
        resetplstuff();
        for (int i = 0; i < 12; i++) {
            struct SortObj *o = sortobj(i);
            int16_t p = o->position;
            if (!PFLAG(o, pfgoal)) p += 6;
            o->Xpos = (int16_t)hl_postab[2 * p];
            o->Ypos = (int16_t)hl_postab[2 * p + 1];
            o->Xvel = 0;
            o->Yvel = 0;
        }
        SprSort();
        g.palcount = 0x18;
        g.oldvcount = g.vcount;
        int16_t after = 0xB4;                             /* frames after the clock stops */
        for (;;) {
            DoGameFrame();
            if ((g.gmode & (1 << gmclock)) && --after < 0) break;
            uint16_t d1 = orjoy();
            if ((d1 & 0x20) || (d1 & 0x80)) {
                if (home->score == away->score) {                       /* tied: a random winner */
                    uint16_t d0 = vdp_hv16() & 1;
                    home->score += d0;
                    away->score += d0 ^ 1;
                }
                break;
            }
        }
        KillCrowd();                                      /* the scores go to a0 as pushed: CheckGameTickerStatus's */
        p_turnoff();
        a0->score1 = home->score;
        a0->score2 = away->score;
        forceblack();
        for (int i = 0; i < 0x10; i++) sortobj(i)->Xpos = 0;
        hl_restore();
        setplayercolors();
        SetHor();
        setvideo();
        SetupPauseScreen();
        DrawMenuScreen();
        g.palcount = 0x18;
        song(SYM_SngEOG);
        gs->per = 4;                                    /* overtime */
        if (gs->score1 == gs->score2) continue;           /* still tied: another highlight */
        gs->per = 5;                                      /* final */
        t = gs;
        d3 = CheckGameTickerStatus(game, &t);
        NewTicker3(d3, t);
        if (waitxsr(0xB4) & 0x80) { g.sflags3 |= 1 << sf3sbut; return; }
        ClearTickerArea();
        if (waitxsr(0x3C) & 0x80) g.sflags3 |= 1 << sf3sbut;
        return;
    }
}

/* penalty94.asm:StartHL. Play the highlight of game a0 (d1 = its number): StartHL2. */
void StartHL(struct Matchup *gs, int16_t game) { StartHL2(gs, game); }

/* penalty94.asm:DoHiLights. The highlights: each game of the round (GetShifter, last first)
 * whose highlight flag is set (gsfhl, cleared here), unless the series is over (gsfso). */
void DoHiLights(void)
{
    int16_t n = (int16_t)GetShifter(NULL);
    do {
        struct Matchup *gs = &g.matchups[n];
        if (!(gs->flags & MU_SERIES_OVER)) {
            int hl = gs->flags & MU_HIGHLIGHT;
            gs->flags &= (uint8_t)~MU_HIGHLIGHT;
            if (hl) StartHL(gs, n);
        }
    } while (n-- != 0);
}

/* penalty94.asm:Intermission. Between periods (and after the game, which first updates the
 * records): the zamboni crosses under the period's menu (the playoff and shootout lists in
 * their modes), the ticker shows the other games' scores, then the highlights; start cuts it
 * short. Waits while a pad is on a team. */
void Intermission(void)
{
    static const uint32_t sslist[10] = {
        SYM_StartGameText, SYM_IntermissionText, SYM_IntermissionText, SYM_IntermissionText, SYM_ExitGameText,
        SYM_StartGameTextPO, SYM_IntermissionText, SYM_IntermissionText, SYM_IntermissionText, SYM_ExitGameTextPO,
    };
    if (g.gsp == 4) {
        g.gameclock = 0;
        UpdateRecords();
    }
    SetupTeamForIntermission();
    for (int i = 0; i < 0x10; i++) sortobj(i)->Xpos = 0;
    DecompressGraphicsPlain(ART(ZamFrameList) + 8, g.ExtraChars);
    g.zamx = 0x140;
    uint16_t list = g.gsp;
    if (g.OptPlayMode != 0) list += 5;
    uint32_t items = sslist[list];
    if (g.gmode2 & HI(0)) items = SYM_ShootoutIntermissionMenu;
    g.sflags |= HI(sfpz);
    InitMenuState(items, SYM_SetupPauseScreen);
    uint16_t d1;
    g.TickerNum = GetShifter(NULL);
    int skip = 0;
    while ((int16_t)g.TickerNum >= 0) {
        struct Matchup *gs = NULL;
        int16_t d3 = NewTicker2(&gs);
        NewTicker3(d3, gs);
        if (waitxsr(0x96) & 0x80) { skip = 1; break; }
        ClearTickerArea();
        if (waitxsr(0x3C) & 0x80) { skip = 1; break; }
    }
    if (skip) g.sflags3 |= 1 << sf3sbut;
    DoHiLights();
    int cut = (g.sflags3 & (1 << sf3sbut)) != 0;
    g.sflags3 &= (uint8_t)~(1 << sf3sbut);
    if (!cut) {
        do {
            d1 = waitxsr(0x1E0);
            if (d1 & 0x80) break;
        } while (g.cont1team != 0 || g.cont2team != 0);
    }
    ST_W(g.zamx);
}
