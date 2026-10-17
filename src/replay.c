/* replay94.asm: the replay recorder (updatereplay), the per-object update loop that drives
 * every assignment (updateplayers, updateanim), the camera (checkwindow, freezewindow) and the
 * instant replay (ReplayMode) with its navigation helpers. */
#include <string.h>
#include "game.h"
#include "symbols_gen.h"
#include "view.h"
#include "hal.h"
#include "rom.h"
#include "replay_strings_gen.h"

#define REPLAY_FRAME 0x62                 /* bytes per recorded frame */
#define REPLAYEND 0xFFFFAF54u             /* the replay buffer is work RAM from M68K_RAM up to here */
#define M68K_RAM RAM_BASE

/* add.l d,pos: a 16.16 position update */
#define ADD_POS32(o, P, F, d) do { int32_t p_ = POS32((o)->P, (o)->F) + (d); (o)->P = (int16_t)(p_ >> 16); (o)->F = (uint16_t)p_; } while (0)

/* replay94.asm:getpzjoy. Read the joystick of the pad that paused: pads 3 and 4 on the four way
 * play (pausepad 3 or 4), else pad 2 when sfpj is set, pad 1 otherwise. */
void getpzjoy(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    if (g.FourWayPlay != 0 && g.pausepad != 0) {
        if (g.pausepad == 3) ReadJoy3(d0, d1, d2, d3);
        else ReadJoy4(d0, d1, d2, d3);
        return;
    }
    if (g.sflags & HI(sfpj)) ReadJoy2(d0, d1, d2, d3);
    else ReadJoy1(d0, d1, d2, d3);
}

/* replay94.asm:ShowReplayIcon. Show the replay icon (8 x 4 from the rink tiles) and start the $F0
 * frame replayicontimer. */
void ShowReplayIcon(void)
{
    g.replayicontimer = 0xF0;
    printz(str_ShowReplayIcon_1, sizeof str_ShowReplayIcon_1);
    uint32_t map = (g.sflags4 & HI(4)) ? SYM_RevRinkTilelist : SYM_Rinktilelist;
    map += rd32(rom_ptr(map) + 4);
    dobitmap(0, map, SYM_NoTiles, 0x20, 0x55, 8, 4, g.rinkvrcset, 0);
}

/* replay94.asm:EraseReplayIcon. Erase the 8 x 4 replay icon. */
void EraseReplayIcon(void)
{
    printz(str_EraseReplayIcon_1, sizeof str_EraseReplayIcon_1);
    eraser(8, 4, 0x7FF);
}

/* replay94.asm:ShowReplayBanner. Show ReplayMap at 0,0 (16 x 11) and stop the icon timer. */
void ShowReplayBanner(void)
{
    ST_W(g.replayicontimer);
    printz(str_ShowReplayBanner_1, sizeof str_ShowReplayBanner_1);
    dobitmap(0, SYM_ReplayMap + rd32(rom_ptr(SYM_ReplayMap) + 4), SYM_NoTiles, 0, 0, 0x10, 0xB, g.ExtraChars, 0);
}

/* replay94.asm:EraseReplayBanner. Erase the 16 x 11 area ShowReplayBanner drew. */
void EraseReplayBanner(void)
{
    printz(str_EraseReplayBanner_1, sizeof str_EraseReplayBanner_1);
    eraser(0x10, 0xB, 0x7FF);
}

/* replay94.asm:suba4. a4 = the current replay frame. Back up one frame (wrapping from M68K_RAM to
 * the end when the buffer has wrapped) and show it. Returns d7: the frame's delay, or 0 at the
 * start of the replay. */
uint16_t suba4(uint32_t *frame, uint8_t *cam)
{
    uint32_t a4 = *frame;
    if (a4 == M68K_RAM) {
        if (!(g.sflags & HI(sfwrap))) goto end;
        a4 = REPLAYEND;
    }
    a4 -= REPLAY_FRAME;
    if (a4 != g.recbpr) { *frame = a4; return RestoreReplayFrame(a4, cam); }
    a4 += REPLAY_FRAME;
end:
    *frame = a4;
    RestoreReplayFrame(a4, cam);
    return 0;
}

/* replay94.asm:adda4. Step forward one frame. First handles a reverse angle switch (sflags4 bit
 * 6): fade out, reload the rink for the other angle, redraw, and show the icon again. With
 * sf2drec (recording stopped) it stops one frame before the record point. */
uint16_t adda4(uint32_t *frame, uint8_t *cam)
{
    if (g.sflags4 & HI(6)) {
        g.sflags4 &= (uint16_t)~HI(6);
        g.sflags4 ^= HI(4);
        uint16_t hpos = g.Hpos, vpos = g.Vpos;
        forceblack2();
        vcountwait();
        vcountwait();
        ReloadRinkGraphics();
        g.Hpos = 0;
        g.Vpos = 0;
        g.Oldrow = 0x7D0;
        for (int i = 0; i < 2; i++) { setvideo(); vcountwait(); }
        g.Vpos = vpos;
        g.Hpos = hpos;
        setvideo();
        for (int i = 0; i < 2; i++) vcountwait();
        sortobj(12)->oldframe = 1;                            /* the nets resend their tiles */
        sortobj(13)->oldframe = 1;
        RevReplayAdj(*frame, cam);
        ShowReplayIcon();
        g.palcount = 0x64;
    }
    if (g.sflags2 & HI(sf2drec)) {
        uint32_t next = *frame;
        adda43(&next);
        if (next == g.recbpr) return 0;
    }
    return adda42(frame, cam);
}

/* replay94.asm:adda42. adda4 without the look-ahead: stop at the record point (d7 = 0), else show
 * the frame and step past it. */
uint16_t adda42(uint32_t *frame, uint8_t *cam)
{
    if (*frame == g.recbpr) return 0;
    uint16_t d7 = RestoreReplayFrame(*frame, cam);
    adda43(frame);
    return d7;
}

/* replay94.asm:adda43. a4 += $62, wrapping from replayend to M68K_RAM. */
void adda43(uint32_t *frame)
{
    *frame += REPLAY_FRAME;
    if (*frame == REPLAYEND) *frame = M68K_RAM;
}

/* replay94.asm:UpdateCameraPos. Camera object a5 = object a3's x / y (negated for the reverse
 * angle), Hpos / Vpos clamped to the rink. */
void UpdateCameraPos(const struct SortObj *o, uint8_t *cam)
{
    int16_t d0 = o->Xpos, d1 = o->Ypos;
    if (g.sflags4 & HI(7)) d0 = (int16_t)-d0;
    wr16(cam, (uint16_t)d0);
    if (g.sflags4 & HI(7)) d1 = (int16_t)-d1;
    wr16(cam + 0x14, (uint16_t)d1);
    if (!(d0 < 0x3C)) d0 = 0x3C;
    if (!(d0 > -0x3C)) d0 = -0x3C;
    g.Hpos = (uint16_t)d0;
    if (!(d1 < 0x100)) d1 = 0x100;
    if (!(d1 > -0xC8)) d1 = -0xC8;
    g.Vpos = (uint16_t)d1;
}

/* replay94.asm:RevReplayAdj. Flag a reverse angle frame conversion (sflags4 bit 7), then
 * RestoreReplayFrame. */
uint16_t RevReplayAdj(uint32_t frame, uint8_t *cam)
{
    if (g.sflags4 & HI(4)) g.sflags4 |= HI(7);
    return RestoreReplayFrame(frame, cam);
}

/* replay94.asm:RestoreReplayFrame. Unpack replay frame a4 into the 16 objects and the other
 * replay variables (the inverse of updatereplay). Flips x / y and frames for the reverse angle.
 * Returns d7 = the frame's elapsed count. */
uint16_t RestoreReplayFrame(uint32_t frame, uint8_t *cam)
{
    const uint8_t *a0 = ram_ptr(frame);
    const uint8_t *revtbl = rom_ptr(SYM_revframetbl);
    int rev = (g.sflags4 & HI(4)) != 0;
    for (int d1 = 0xF; d1 >= 0; d1--, a0 += 4) {
        struct SortObj *o = sortobj(0xF - d1);
        int16_t d2 = (int16_t)(rd16(a0 + 2) & 0x3FF);
        if (d2 & 0x200) d2 = (int16_t)(d2 | 0xFC00);
        if (rev) d2 = (int16_t)-d2;
        o->Xpos = d2;
        int32_t l = (int32_t)rd32(a0);
        d2 = (int16_t)((int16_t)(uint16_t)(l >> 4) >> 6);
        if (d2 & 0x200) d2 = (int16_t)(d2 | 0xFC00);
        if (rev) {
            d2 = (int16_t)-d2;
            if (d1 == 0) d2 = (int16_t)(d2 + 2);                 /* the puck shadow */
        }
        o->Ypos = d2;
        int16_t raw = (int16_t)(((int16_t)rd16(a0) >> 4) & 0x3FF);
        d2 = raw;
        if (rev) {
            d2 = (int16_t)rd16(revtbl + d2 * 2);
            if (o->SCnum == 0xF && d2 != (int16_t)SYM_SPFpuck) o->Ypos = (int16_t)0xFC00;
            if (d2 < 1 || d2 >= 0x34E) d2 = raw;
        }
        o->frame = (uint16_t)d2;
        if (d2 >= (int16_t)SYM_SPFbglass && d2 < (int16_t)(SYM_SPFbglass + 5) && rev) o->Ypos = 0x190;
        uint16_t attr = (uint16_t)(((int16_t)rd16(a0) >> 3) & 0x1800);
        o->attribute = (uint16_t)((o->attribute & 0xE7FF) | attr);
        if (rev && (int16_t)o->frame >= (int16_t)SYM_SPFfight && (int16_t)o->frame < (int16_t)SYM_SPFPen)
            o->attribute ^= HI(3);                                /* the old fight frames face the other way */
    }
    for (int i = 0; i < 6; i++) {                                 /* roster numbers and positions, 2 players per 3 bytes */
        struct SortObj *a = sortobj(i * 2), *b = sortobj(i * 2 + 1);
        a->rostnum = *a0++;
        uint16_t d0 = *a0 & 0xF;
        a->position = (int16_t)(d0 == 0xF ? -1 : d0);
        b->rostnum = (uint8_t)(*a0++ >> 4);
        b->rostnum |= (uint8_t)(*a0 << 4);
        d0 = (*a0++ >> 4) & 0xF;
        b->position = (int16_t)(d0 == 0xF ? -1 : d0);
    }
    g.puckz[0] = (uint16_t)(int16_t)(int8_t)*a0++;
    sortobj(0xF)->Zpos = (int8_t)*a0++;
    g.lastsfx = (uint16_t)(int16_t)(int8_t)*a0++;
    uint16_t d7 = *a0++;
    g.PadControlBits = (uint16_t)((g.PadControlBits & 0xF000) | (rd16(a0) & 0xFFF));
    a0 += 2;
    g.crowdframe = rd16(a0);
    a0 += 2;
    g.glovecords = rd16(a0);
    a0 += 2;
    g.PBnum = (uint16_t)((*a0++ << 8) | (g.PBnum & 0xFF));
    g.PadControlBits34 = (uint16_t)((*a0++ << 4) | 0xF00F);
    if (g.sflags & HI(sfscrl)) {
        const struct SortObj *o = (const struct SortObj *)cam;
        if (g.sflags3 & 0x20) {
            wr16(cam + 0x18, 0);
            o = sortobj((int16_t)rd16(cam + 0x16));
        }
        UpdateCameraPos(o, cam);
    } else {
        g.Hpos = rd16(a0);
        g.Vpos = rd16(a0 + 2);
        if (rev) {
            g.Hpos = (uint16_t)-g.Hpos;
            g.Vpos = (uint16_t)-g.Vpos;
            ClampReplayView();
        }
    }
    g.sflags4 &= (uint16_t)~HI(7);
    return d7;
}

/* replay94.asm:ClampReplayView. Clamp Hpos to -$3C..$3C and Vpos to -$C8..$100. */
void ClampReplayView(void)
{
    if (view_hpos() < (int16_t)g.Hpos) g.Hpos = (uint16_t)view_hpos();   /* (the wide view's rink: less far) */
    else if (!(-view_hpos() <= (int16_t)g.Hpos)) g.Hpos = (uint16_t)-view_hpos();
    if (0x100 < (int16_t)g.Vpos) g.Vpos = 0x100;
    else if (!(-0xC8 <= (int16_t)g.Vpos)) g.Vpos = (uint16_t)-0xC8;
}

/* replay94.asm:updatereplay. Called every frame to save a replay frame at recbpr: the 16 objects
 * (x, y, frame and palette bits packed in a long each), 6 x 3 bytes of roster numbers and
 * positions, the puck heights, the last sound, the elapsed count, the pad bits and the view.
 * Not in a highlight. With sf2drec (recording stopped) the frame at recbpr is overwritten until
 * replaydelay runs out. */
void updatereplay(void)
{
    if (g.gmode & (1 << gmhl)) return;
    int advance;
    if (!(g.sflags2 & HI(sf2drec))) advance = 1;
    else if (g.replaydelay == 0) advance = 0;
    else {
        g.replaydelay--;
        advance = (int16_t)g.replaydelay >= 0;
        if (!advance) g.replaydelay = 0;
    }
    if (advance) {
        g.recbpr += REPLAY_FRAME;
        if (g.recbpr == REPLAYEND) {
            g.sflags |= HI(sfwrap);
            g.recbpr = M68K_RAM;
        }
    }
    uint8_t *a0 = ram_ptr(g.recbpr);
    for (int i = 0; i < 16; i++, a0 += 4) {
        const struct SortObj *o = sortobj(i);
        uint32_t l = (uint32_t)(o->Xpos & 0x3FF);                 /* clr.l, then x at 2 */
        l |= (uint32_t)(uint16_t)((uint16_t)o->Ypos << 6) << 4;  /* asl.w #6 then asl.l #4 */
        uint16_t hi = (uint16_t)(l >> 16);
        hi |= (uint16_t)(o->frame << 4);
        hi |= (uint16_t)((o->attribute & 0x1800) << 3);
        wr32(a0, ((uint32_t)hi << 16) | (l & 0xFFFF));
    }
    for (int i = 0; i < 6; i++) {
        const struct SortObj *a = sortobj(i * 2), *b = sortobj(i * 2 + 1);
        *a0++ = a->rostnum;
        uint16_t d0 = (uint16_t)a->position;
        if ((int16_t)d0 < 0) d0 = 0xF;
        *a0 = (uint8_t)(d0 & 0xF);
        *a0++ |= (uint8_t)(b->rostnum << 4);
        *a0 = (uint8_t)(b->rostnum >> 4);
        d0 = (uint16_t)b->position;
        if ((int16_t)d0 < 0) d0 = 0xF;
        *a0++ |= (uint8_t)(d0 << 4);
    }
    *a0++ = (uint8_t)g.puckz[0];
    *a0++ = (uint8_t)sortobj(0xF)->Zpos;
    *a0++ = (uint8_t)g.lastsfx;
    g.lastsfx |= 0x80;
    *a0++ = (uint8_t)elapsed;
    wr16(a0, g.PadControlBits);
    a0 += 2;
    wr16(a0, g.crowdframe);
    a0 += 2;
    wr16(a0, g.glovecords);
    a0 += 2;
    *a0++ = (uint8_t)(g.PBnum >> 8);
    *a0++ = (uint8_t)(g.PadControlBits34 >> 4);
    wr16(a0, g.Hpos);
    a0 += 2;
    wr16(a0, g.Vpos);
}

/* the assignment dispatch table (data94.asm:asstab), indexed by asslist[assnum] */
typedef void (*assfn)(uint8_t *tm, struct SortObj *o);
static void ass_none(uint8_t *tm, struct SortObj *o) { (void)tm; (void)o; }           /* rtss2 */
static void ass_puckunflip(uint8_t *tm, struct SortObj *o) { (void)tm; puckunflip(o); }
static const assfn asstab[0x24] = {
    ass_none, assdefo, assdefd, asswingd, asswingo, asscenterd, asscentero, assscore,           /* 0-7 */
    assstanley, asseben, assepen, assbench, asspenalty, assdopen, assgoaliecpu, assgoalietopuck, /* 8-$F */
    asspuckc, assnearest, assshoot, asspassrec, assfight, assfwatch, assfaceoff, assfaceoffpl,  /* $10-$17 */
    pucknorm, puckshadow, ass_puckunflip, puckfaceoff, puckfaceoff2, assgoaliectrl, puckshootout, puckpenshot, /* $18-$1F */
    assgoaliebreakwait, chkpuckc, assbreakaway, assonetimer,                                    /* $20-$23 */
};

/* a player in a board / bench check animation is held at the boards */
static void hold_at_boards(struct SortObj *o)
{
    uint16_t spa = o->SPA;
    int16_t fx = (int16_t)g.FallXPos;
    int outside = fx > 0x56 || !(fx > -0x56);
    if (spa == SYM_SPAboardfront) o->Ypos = outside ? 0x116 : 0x124;
    else if (spa == SYM_SPAboardback) o->Ypos = (int16_t)(outside ? 0xFEEA : 0xFEDC);
    else if (spa == SYM_SPAboardright) o->Xpos = (o->attribute & HI(3)) ? (int16_t)0xFF7E : 0x82;
    else if (spa == SYM_SPAbenchcheckr) o->Xpos = (o->attribute & HI(3)) ? (int16_t)0xFF78 : 0x88;
    else if (spa == SYM_SPAboardleft) o->Xpos = (o->attribute & HI(3)) ? 0x82 : (int16_t)0xFF7E;
    else if (spa == SYM_SPAbenchcheckl) o->Xpos = (o->attribute & HI(3)) ? 0x88 : (int16_t)0xFF78;
}

/* sub.b d7 from a byte timer, stopping at 0 */
static uint8_t tick_byte(uint8_t v)
{
    uint8_t r = (uint8_t)(v - (uint8_t)elapsed);
    return (r & 0x80) ? 0 : r;
}

/* the puck carrier on pad 3 or 4: run doinput with the pad 1 / 2 variables swapped for the pad 3 / 4 ones */
#define FOURWAY_INPUT(CT, CT34, CP, CP34, BH, PW, PW34, AH, PCB, d0, d1, d2, d3, pad, o) do { \
        uint16_t ct_ = g.CT, cp_ = g.CP, bh_ = g.BH, pw_ = g.PW, ah_ = g.AH, pcb_ = g.PCB;   \
        g.CT = g.CT34; g.CP = g.CP34; g.BH = g.bholdtimer34; g.PW = g.PW34; g.AH = g.aholdtimer34; g.PCB = g.PadControlBits34; \
        doinput(d0, d1, d2, d3, pad, o);                                                       \
        g.PadControlBits34 = g.PCB; g.PCB = pcb_; g.aholdtimer34 = g.AH; g.AH = ah_;           \
        g.PW34 = g.PW; g.PW = pw_; g.bholdtimer34 = g.BH; g.BH = bh_;                          \
        g.CP34 = g.CP; g.CP = cp_; g.CT34 = g.CT; g.CT = ct_;                                  \
    } while (0)

/* replay94.asm:updateplayers. The per-frame update of all 16 objects: puck height settling, the
 * crowd meter record bit, then for each object the old position copy, the fall-at-the-boards hold,
 * updateanim, the no-puck timers, time on ice, deceleration and movement (with the bounce sound
 * when the puck lands), the puck carrier's pad, the human controllers' input (doinput), the
 * assignment from asstab, collisions and the impact decay. d7 = elapsed frames. */
uint32_t assign_d2 = 6;

void updateplayers(void)
{
    if (!(g.sflags & HI(sfhor))) {
        g.PadControlBits |= 0xF;
        if ((int16_t)g.puckz[0] > 5) {
            if (g.TmpPuckZ != g.puckz[0]) {
                g.TmpPuckZ = g.puckz[0];
                g.PuckZCntr = 0;
            } else if ((int16_t)++g.PuckZCntr >= 0x10) {
                g.PuckZCntr = 0;
                g.puckz[0]--;
            }
        } else {
            g.PuckZCntr = 0;
        }
    }
    g.sflags7 &= (uint8_t)~0x40;                                  /* the crowd meter record bit */
    if (!((int16_t)g.CurCrowdMeter < (int16_t)g.CrowdRecord)) g.sflags7 |= 0x40;
    SetSflags8Bit1();
    if ((int16_t)g.puckc >= 0) {
        g.sflags8 &= (uint8_t)~0x80;
        g.sflags6 &= (uint16_t)~HI(1);
        if (sortobj(g.puckc)->pflags3 & (1 << pf3break)) g.gmode2 &= (uint16_t)~HI(4);
    }
    for (int i = 0; i < 16; i++) {
        struct SortObj *o = sortobj(i);
        o->OldXpos = o->Xpos; o->OldXfrac = o->Xfrac;
        o->OldYpos = o->Ypos; o->OldYfrac = o->Yfrac;
        o->OldZpos = o->Zpos; o->OldZfrac = o->Zfrac;
        if (o->pflags3 & (1 << pf3fall)) {
            o->Xvel = 0;
            o->Yvel = 0;
            if (o->SPAnum == 0) PlaceBoardFall(o);
            else hold_at_boards(o);
        }
        if (o->position >= 0) {                                   /* on the ice */
            if (o->SCnum != (int16_t)g.puckc) o->pflags3 &= (uint8_t)~((1 << pf3break) | (1 << pf3oside));
            updateanim(o);
            o->nopuck[0] = tick_byte(o->nopuck[0]);
            o->nopuck[1] = tick_byte(o->nopuck[1]);
            if ((g.sflags3 & 0x40) && !(o->pnum & 0x80)) {       /* the penalty timer is up: time on ice */
                uint8_t *tm = loadTeamStruct(o, NULL);
                uint16_t off = (uint16_t)(0x136 + o->pnum * 2);
                wr16(tm + off, (uint16_t)(rd16(tm + off) + 1));
            }
            if (o->SPA != SYM_SPAinjuryfall && o->SPA != SYM_SPAinjury1) {
                uint16_t d4 = (uint16_t)((g.PalMode ? 0x16 : 0x11) * (uint16_t)elapsed);
                if (o->Zpos == 0) {                               /* deceleration on the ice */
                    int d2 = (o->pflags & (1 << pfdoff)) ? 9 : 6;
                    assign_d2 = (uint32_t)d2;
                    int puck = i == puckscnum && rules[RULE_PUCK_SLIDE];   /* (rules: the puck's slide) */
                    int16_t d0 = o->Xvel;
                    if (d0 != 0) {
                        d0 = (int16_t)(d0 >> d2);
                        if (puck) d0 = (int16_t)rule_scale(RULE_PUCK_SLIDE, d0);
                        if (d0 == 0) d0 = 1;
                        o->Xvel = (int16_t)(o->Xvel - d0);
                    }
                    d0 = o->Yvel;
                    if (d0 != 0) {
                        d0 = (int16_t)(d0 >> d2);
                        if (puck) d0 = (int16_t)rule_scale(RULE_PUCK_SLIDE, d0);
                        if (d0 == 0) d0 = 1;
                        o->Yvel = (int16_t)(o->Yvel - d0);
                    }
                }
                if (o->Xvel != 0) ADD_POS32(o, Xpos, Xfrac, (int32_t)o->Xvel * (int16_t)d4);
                if (o->Yvel != 0) ADD_POS32(o, Ypos, Yfrac, (int32_t)o->Yvel * (int16_t)d4);
                if (o->Zpos >= 0 && (o->Zpos != 0 || o->Zvel != 0)) {   /* in the air: gravity */
                    d4 = (uint16_t)(d4 << 1);
                    o->Zvel = (int16_t)(o->Zvel - d4);
                    d4 = (uint16_t)(d4 << 1);
                    o->Zvel = (int16_t)(o->Zvel - d4);
                    d4 = (uint16_t)(d4 >> 2);
                    int32_t p = POS32(o->Zpos, o->Zfrac) + (int32_t)o->Zvel * (int16_t)d4;
                    o->Zpos = (int16_t)(p >> 16);
                    o->Zfrac = (uint16_t)p;
                    if (p < 0) {                                  /* landed: bounce */
                        o->Zpos = 0;
                        o->Zfrac = 0;
                        o->Zvel = (int16_t)(-o->Zvel);
                        o->Zvel = (int16_t)(o->Zvel >> 1);
                        uint8_t b = (uint8_t)(5 - (uint8_t)(o->Zvel >> 8));
                        if (b & 0x80) b = 0;
                        if (b <= 3) sfx((int16_t)(SYM_SFXpuckice + b));
                    }
                }
            }
            int16_t d6 = o->SCnum;
            if (d6 == (int16_t)g.puckc && !(o->pflags2 & (1 << pf2fight)) && !(g.sflags & HI(sfhor)))
                setpads(-2, o);
            if (!(g.sflags2 & HI(sf2faceoff)) && g.holdreset != 0) {
                g.holdreset--;
                if ((int16_t)g.holdreset <= 0) {
                    g.holdreset = 0;
                    g.bholdtimer = 0x1111;
                    g.bholdtimer34 = 0x1111;
                }
            }
            uint16_t d0, d1, d2, d3;
            if (d6 == (int16_t)g.c1playernum) {
                ReadJoy1(&d0, &d1, &d2, &d3);
                assign_d2 = d2;                                   /* (doinput's own d2 not followed) */
                g.joypuckcarrier = 0xFFFF;
                doinput(d0, d1, d2, d3, 0, o);
            } else if (d6 == (int16_t)g.c2playernum) {
                ReadJoy2(&d0, &d1, &d2, &d3);
                assign_d2 = d2;
                g.joypuckcarrier = 0xFFFF;
                doinput(d0, d1, d2, d3, 2, o);
            }
            if (d6 <= 0xB) {                                      /* pads 3 and 4 of the four way play */
                if (g.cont3team != 0 && d6 == (int16_t)g.c3playernum) {
                    ReadJoy3(&d0, &d1, &d2, &d3);
                    assign_d2 = d2;
                    g.joypuckcarrier = 0;
                    FOURWAY_INPUT(cont1team, cont3team, c1playernum, c3playernum, bholdtimer, padword1, padword3, aholdtimer, PadControlBits, d0, d1, d2, d3, 0, o);
                }
                if (g.cont4team != 0 && d6 == (int16_t)g.c4playernum) {
                    ReadJoy4(&d0, &d1, &d2, &d3);
                    assign_d2 = d2;
                    g.joypuckcarrier = 1;
                    FOURWAY_INPUT(cont2team, cont4team, c2playernum, c4playernum, bholdtimer, padword2, padword4, aholdtimer, PadControlBits, d0, d1, d2, d3, 2, o);
                }
            }
            uint8_t *tm = loadTeamStruct(o, NULL);
            asstab[o->asslist[o->assnum]](tm, o);
            o->Wallcos = 0;
            o->Wallsin = 0;
            int16_t x = o->Xpos, y = o->Ypos;
            if (x != o->OldXpos || y != o->OldYpos) checkcoll(x, y, o);
            o->impact = (int16_t)(o->impact - elapsed * 2);
            if (o->impact < 0) o->impact = 0;
        }
        o->limpact = o->impact;
    }
}

/* replay94.asm:updateanim. Frame switch control on object a3: step SPAcnt by the elapsed frames,
 * advance SPAnum through the animation (SPAlist entry SPA, frame list by facing direction) and
 * pick the frame, with a 4 frame de-glitch minimum between frame switches. */
void updateanim(struct SortObj *o)
{
    if (o->SPA == 0) {
        o->pflags &= (uint8_t)~(1 << pfalock);
        o->pflags2 &= (uint8_t)~(1 << pf2aip);
        return;
    }
    const uint8_t *a0 = rom_ptr(SYM_SPAlist + o->SPA);
    uint16_t d1 = rd16(a0 + 0x10);                                /* animation attributes: bit 0 loops */
    int16_t d0 = o->facedir;
    if ((g.sflags & HI(sfhor)) && o->SCnum < 0xC) d0 = (int16_t)((d0 - 2) & 7);
    if (o->attribute & HI(3)) d0 = (int16_t)((-d0 + 8) & 7);
    a0 += (int16_t)rd16(a0 + d0 * 2);                             /* the frame list for this direction */
    d0 = (int16_t)o->SPAnum;
    uint16_t d2 = rd16(a0 + d0);                                  /* the frame at this index */
    int newtime = (int16_t)o->SPAcnt < 0;
    if (!newtime) {
        o->SPAcnt = (uint16_t)(o->SPAcnt - elapsed);
        if ((int16_t)o->SPAcnt < 0) {
            o->SPAnum += 4;
            d0 = (int16_t)(d0 + 4);
            if ((int16_t)rd16(a0 + d0 - 2) < 0) {                 /* the last frame's time is negative */
                d0 = 0;
                o->SPAnum = 0;
                o->pflags &= (uint8_t)~(1 << pfalock);
                o->pflags2 &= (uint8_t)~(1 << pf2aip);
                o->pflags3 &= (uint8_t)~(1 << pf3fall);
                if (!(d1 & 1)) o->SPA = 0;
            }
            newtime = 1;
        }
    }
    if (newtime) {
        int16_t t = (int16_t)rd16(a0 + d0 + 2);
        if (t < 0) t = (int16_t)-t;
        o->SPAcnt = (uint16_t)t;
    }
    o->glitch = (uint8_t)(o->glitch - (uint8_t)elapsed);
    if (o->glitch & 0x80) {
        o->glitch = 0;
        if (d2 != o->frame) {
            o->frame = d2;
            o->glitch = 4;
        }
    }
}

/* replay94.asm:freezewindow. Lock the view at the current position. */
void freezewindow(void)
{
    g.yc1 = g.Vpos;
    g.xc1 = g.Hpos;
    g.sflags |= HI(sfslock);
}

/* replay94.asm:checkwindow. Move Hpos / Vpos toward the puck (or the puck carrier, leading
 * toward the goal he attacks): the target yc1 / xc1 is followed with a 1/16 step when it leaves
 * a dead zone around the view, clamped to the rink. */
void checkwindow(void)
{
    int16_t d2 = (int16_t)g.yc1, d3 = (int16_t)g.xc1;
    if (!(g.sflags & HI(sfslock))) {
        const struct SortObj *o = sortobj(0xE);                   /* the puck */
        if ((int16_t)g.puckc >= 0) {
            o = sortobj(g.puckc);
            int16_t d0 = (int16_t)(elapsed * 2);
            if (o->pflags & (1 << pfgoal)) {
                g.yleader = (uint16_t)(g.yleader + d0);
                if (!((int16_t)g.yleader < 0x32)) g.yleader = 0x32;
            } else {
                g.yleader = (uint16_t)(g.yleader - d0);
                if (!((int16_t)g.yleader > -0x32)) g.yleader = (uint16_t)-0x32;
            }
        }
        d2 = (int16_t)((o->Yvel >> 7) + o->Ypos + (int16_t)g.yleader);
        d3 = o->Xpos;
        g.yc1 = (uint16_t)d2;
        g.xc1 = (uint16_t)d3;
    }
    int16_t d0 = (int16_t)(d2 - (int16_t)g.Vpos), d1;
    int move = 1;
    if (d0 >= -0xA) {
        if (d0 <= 0xA) move = 0;
        else {
            d1 = (int16_t)(d2 - 0xA);
            if (!(d1 < 0x100)) d1 = 0x100;
        }
    } else {
        d1 = (int16_t)(d2 + 0xA);
        if (!(d1 > -0xC8)) d1 = -0xC8;
    }
    if (move) {
        d1 = (int16_t)(d1 - (int16_t)g.Vpos);
        if (d1 != 0) {
            d1 = (int16_t)(d1 >> 4);
            if (d1 == 0) d1 = 1;
            g.Vpos = (uint16_t)(g.Vpos + d1);
        }
    }
    d0 = (int16_t)(d3 - (int16_t)g.Hpos);
    move = 1;
    if (d0 >= -0x28) {
        if (d0 <= 0x28) move = 0;
        else {
            d1 = (int16_t)(d3 - 0x28);
            if (!(d1 <= view_hpos())) d1 = view_hpos();   /* (the wide view's rink: less far) */
        }
    } else {
        d1 = (int16_t)(d3 + 0x28);
        if (!(d1 >= -view_hpos())) d1 = (int16_t)-view_hpos();
    }
    if (move) {
        d1 = (int16_t)(d1 - (int16_t)g.Hpos);
        if (d1 != 0) {
            d1 = (int16_t)(d1 >> 4);
            if (d1 == 0) d1 = 1;
            g.Hpos = (uint16_t)(g.Hpos + d1);
        }
    }
}

/* ReplayMode's .dirtab: the camera step per d-pad direction 0-7 (x, y) */
static const int16_t replay_dirtab[16] = { 0, 2, 2, 2, 2, 0, 2, -2, 0, -2, -2, -2, -2, 0, -2, 2 };

/* replay94.asm:ReplayMode. The instant replay, from the pause menu. Rewind to the oldest
 * frame, then each pass (at most one recorded frame per vblank, d7 = elapsed): the d-pad moves
 * the camera or picks the nearest object in that direction to follow, A rewinds, B steps or
 * fast-forwards, C plays / stops; start shows the banner, where start again resumes the game,
 * B leaves, C reverses the angle. Leaving restores the frame at the record point and the rink. */
void ReplayMode(void)
{
    g.sflags5 &= (uint8_t)~1;
    forceblack();
    uint16_t saved = g.disflags;
    g.sflags2 |= HI(sf2replay);
    g.sflags4 &= (uint16_t)~HI(4);
    ClrHor();
    DoFill(0x400, g.VmMap3, 0x7FF);
    DecompressGraphicsPlain(SYM_ReplayMap + 8, g.ExtraChars);
    ShowReplayIcon();
    g.sflags &= (uint16_t)~HI(sfscrl);
    g.sflags3 &= (uint8_t)~0x20;
    ram_ptr(RAM_BASE + RAM_OFF_replayactive)[0] = 0xFF;
    uint8_t *cam = ram_ptr(RAM_BASE + RAM_OFF_ReplayStarStruct);   /* a5 (not read while sfscrl is clear) */
    uint32_t a4 = g.recbpr;
    do elapsed = (int16_t)suba4(&a4, cam); while (elapsed != 0);
    SprSort();
    setvideo();
    g.sflags3 &= (uint8_t)~2;
    memset(ram_ptr(RAM_BASE + RAM_OFF_padcont), 0, 12);
    g.palcount = 0x18;
    elapsed = 1;
    for (;;) {                                            /* .top */
        while ((uint16_t)elapsed > (uint16_t)(g.vcount - g.oldvcount)) hal_wait_vblank();
        g.oldvcount = g.vcount;
        if ((int16_t)g.replayicontimer >= 0) {
            g.replayicontimer = (uint16_t)(g.replayicontimer - elapsed);
            if ((int16_t)g.replayicontimer < 0) { g.replayicontimer = 0; EraseReplayIcon(); }
        }
        sortobj(12)->oldframe = 1;
        sortobj(13)->oldframe = 1;
        elapsed = 1;
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        if (g.sflags5 & 1) {                              /* the banner is up: no d-pad */
            d0 = (uint16_t)((d0 & 0xFFF0) | 8);
            d1 &= 0xFFF0;
            d3 &= 0xFFF0;
        }
        int manual = !(d0 & 8);
        if ((g.sflags3 & 0x20) && (d1 & 0xF) == 0) manual = 0;
        if (manual) {                                     /* .dir: move or retarget the camera */
            if ((int16_t)rd16(cam) <= -0xA0) wr16(cam, 0);
            if ((int16_t)rd16(cam + 0x14) <= -0x130) wr16(cam + 0x14, 0);
            int scrolling = (g.sflags & HI(sfscrl)) != 0;
            g.sflags |= HI(sfscrl);
            if (!scrolling) { wr16(cam, g.Hpos); wr16(cam + 0x14, g.Vpos); }
            uint16_t dir = (uint16_t)((d0 & 7) ^ 4);
            uint16_t skip = 0xFF;                         /* st d3 */
            int tracking = (g.sflags3 & 0x20) != 0;
            g.sflags3 &= (uint8_t)~0x20;
            if (tracking) skip = rd16(cam + 0x16);
            int16_t cx = (int16_t)rd16(cam), cy = (int16_t)rd16(cam + 0x14);
            uint32_t best = 0x100;
            for (int i = 0; i < 12; i++) {
                const struct SortObj *o = sortobj(i);
                if ((uint16_t)o->SCnum == skip) continue;
                int16_t dx = (int16_t)(cx - o->Xpos), dy = (int16_t)(cy - o->Ypos);
                uint16_t v = vtoa(dx, dy);
                if (v & 8) continue;
                if (((uint16_t)(v - dir + 1) & 7) > 2) continue;
                uint32_t dist = (uint32_t)((int32_t)dx * dx) + (uint32_t)((int32_t)dy * dy);
                if (dist <= 4 || dist > best) continue;
                best = dist;
                wr16(cam + 0x16, (uint16_t)o->SCnum);
                g.sflags3 |= 0x20;
            }
            if (g.sflags3 & 0x20) {                       /* follow the object found */
                struct SortObj *o = sortobj((int16_t)rd16(cam + 0x16));
                setpads(4, o);
                UpdateCameraPos(o, cam);
                setvideo();
                continue;
            }
            g.sflags |= HI(sfscrl);                       /* .scrl: move the camera */
            dir ^= 4;
            int16_t nx = (int16_t)(replay_dirtab[2 * dir] + (int16_t)rd16(cam));
            if (nx <= 0x88 && nx >= -0x88) wr16(cam, (uint16_t)nx);
            int16_t ny = (int16_t)(replay_dirtab[2 * dir + 1] + (int16_t)rd16(cam + 0x14));
            if (ny <= 0x126 && ny >= -0x126) wr16(cam + 0x14, (uint16_t)ny);
            wr16(cam + 0x18, 0);
            g.PadControlBits = (uint16_t)((g.PadControlBits & 0xFFF) | 0xE000);
            UpdateCameraPos((const struct SortObj *)cam, cam);
            setvideo();
            continue;
        }
        cam[0x18] = 0xFF;                                 /* .nomans */
        if ((d1 & 0x20) && !(g.sflags5 & 1)) g.sflags3 ^= 1 << sf3rmplay;
        if ((d3 & 0x40) && !(g.sflags5 & 1)) {            /* A: rewind */
            for (int i = 0; i < 4; i++) elapsed = (int16_t)suba4(&a4, cam);
            SprSort();
            setvideo();
            elapsed = 1;
            g.sflags3 &= (uint8_t)~2;
        }
        if (!(g.sflags5 & 1) && (d3 & 0x10)) {            /* B */
            if (d3 & 0x40) {                              /* with A: stop tracking */
                g.sflags3 &= (uint8_t)~0x20;
                g.sflags &= (uint16_t)~HI(sfscrl);
                g.PadControlBits = (uint16_t)((g.PadControlBits & 0xFFF) | 0xE000);
                continue;
            }
            elapsed = (int16_t)adda4(&a4, cam);           /* step / fast forward */
            SprSort();
            setvideo();
            elapsed = (int16_t)(elapsed << 1);
            g.sflags3 &= (uint8_t)~2;
        }
        if (g.sflags3 & (1 << sf3rmplay)) {               /* play */
            ST_W(g.lastsfx);
            elapsed = (int16_t)adda4(&a4, cam);
            sfx((int16_t)g.lastsfx);
            SprSort();
        }
        setvideo();
        if (!(g.sflags5 & 1)) {
            if (!(d1 & 0x80)) continue;
            ShowReplayBanner();
            g.sflags3 &= (uint8_t)~2;
            int up = g.sflags5 & 1;
            g.sflags5 |= 1;
            if (!up) continue;
        }
        if (d1 & 0x80) {                                  /* start: resume the game */
            wr16(ram_ptr(RAM_BASE + RAM_OFF_menuitem), 0);
            g.sflags5 |= 2;
            break;
        }
        if (d1 & 0x40) continue;
        if (d1 & 0x10) break;                             /* B: leave */
        if (!(d1 & 0x20)) continue;
        g.sflags4 |= HI(6);                               /* C: reverse angle */
        g.sflags5 &= (uint8_t)~1;
        EraseReplayBanner();
        elapsed = (int16_t)adda4(&a4, cam);
        SprSort();
    }
    forceblack();
    g.PadControlBits |= 0xF000;
    g.sflags &= (uint16_t)~HI(sfscrl);
    a4 = g.recbpr;
    g.sflags4 &= (uint16_t)~HI(4);
    RestoreReplayFrame(a4, cam);
    SprSort();
    g.sflags2 &= (uint16_t)~HI(sf2replay);
    g.disflags = saved;
    ReloadRinkGraphics();
    setupEASNmap();
    ReloadEnergyBarTiles();
    ReloadCrowdTiles();
    ReloadRefTiles();
    if (g.disflags & HI(4)) {
        ReloadFaceOffMap();
        ReloadFaceOffTiles();
    }
    SetHor();
    setvideo();
    g.palcount = 0x18;
}
