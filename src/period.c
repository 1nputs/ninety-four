/* period94.asm: period and game statistics. Ends a shootout, clears the game stats, animates
 * the arena sprites and shows the Period Stats and Game Statistics screens, with the power play
 * time and team ratings they report. The two screens themselves wait for the pad every frame
 * and come with the front end. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "period_strings_gen.h"

#define PFLAG(o, b) ((o)->pflags & (1 << (b)))
#define VSRAM_WRITE 0x40000010u

/* period94.asm:EndShootout. Shootout over: freeze the window, then the 5 skaters of the winning
 * side are set up above or below the view and celebrate (assscore). */
void EndShootout(void)
{
    g.sflags2 |= HI(2);
    freezewindow();
    g.shootoutdelay = 0x60;
    struct SortObj *a3 = sortobj((int16_t)g.homeshootgoals > (int16_t)g.awayshootgoals ? 0 : 6);
    for (int i = 0; i < 5; i++) {
        a3++;
        setplayer(6, a3);
        a3->position = 2;
        int16_t d0 = (int16_t)g.Vpos < 0 ? 0xF0 : -0xF0;
        a3->Ypos = (int16_t)(d0 + (int16_t)g.Vpos);
        a3->Xpos = 0;
        assinsert(7, a3);                                 /* assscore */
        a3->pflags &= (uint8_t)~(1 << pfalock);
        a3->pflags2 &= (uint8_t)~2;
        a3->pflags &= (uint8_t)~(1 << pfnc);
    }
}

/* period94.asm:ClampPositionBox. Unreferenced: clamp temp3 / temp4 of a3 to the box of his
 * position. */
void ClampPositionBox(struct SortObj *o)
{
    const uint8_t *box = rom_ptr(PFLAG(o, pfgoal) ? SYM_PositionBoxesTop : SYM_PositionBoxes) + (o->position - 1) * 2;
    int16_t d0 = (int16_t)rd16(box);
    if (!(d0 < o->temp3)) o->temp3 = d0;
    d0 = (int16_t)rd16(box + 2);
    if (!(d0 > o->temp3)) o->temp3 = d0;
    d0 = (int16_t)rd16(box + 4);
    if (!(d0 < o->temp4)) o->temp4 = d0;
    d0 = (int16_t)rd16(box + 6);
    if (!(d0 > o->temp4)) o->temp4 = d0;
}

/* period94.asm:ClearGameStats. Clear the shootout state, both team structs, PenBuf and the
 * game flags. */
void ClearGameStats(void)
{
    uint8_t *a0 = ram_ptr(RAM_BASE + RAM_OFF_shootoutstate);
    for (int i = 0; i < 0x13; i++) wr16(a0 + i * 2, 0);
    a0 = team_struct(0);
    for (int i = 0; i < 0x364; i++) wr16(a0 + i * 2, 0);
    a0 = ram_ptr(RAM_BASE + RAM_OFF_PenBuf);
    for (int i = 0; i < 0x25; i++) wr16(a0 + i * 2, 0);
    g.BA_PS_flags = 0;
    g.sflags4 = 0;
    g.sflags6 = 0;
    g.gmode2 = 0;
    g.setupcardflags = 0;
    g.featuredplayer = 0;
}

/* period94.asm:AddArenaAnimSprite. Add the frame arenaframe of the arena animation to the
 * sprite table (64 sprites at most) for the pieces inside the view. Not in a reverse angle
 * replay, nor on the horizontal rink. */
void AddArenaAnimSprite(uint8_t **sat, uint16_t *link)
{
    if ((int16_t)g.arenaanim < 0) return;
    if (g.arenaspritelist == 0) return;
    if (g.sflags4 & HI(4)) return;
    if (g.sflags2 & HI(3)) return;
    if (g.sflags & HI(sfhor)) return;
    const uint8_t *a1 = rom_ptr(g.arenaspritelist);
    a1 += rd32(a1 + 4);
    int16_t d0 = (int8_t)g.arenaframe;
    if (d0 == 0) return;
    if ((int16_t)*link >= 0x40) return;
    const uint8_t *a0 = a1;
    int16_t n = (int16_t)(((rd16(a0 + d0 * 2 + 2) - rd16(a0 + d0 * 2)) >> 3) - 1);
    a0 += rd16(a0 + d0 * 2);
    int16_t x0 = (int16_t)((int16_t)g.Hpos + 0xC0 - 0x90), x1 = (int16_t)((int16_t)g.Hpos + 0xC0 + 0x80);
    int16_t y0 = (int16_t)(0x170 - (int16_t)g.Vpos - 0x80), y1 = (int16_t)(0x170 - (int16_t)g.Vpos + 0x70);
    uint8_t *a6 = *sat;
    uint16_t d6 = *link;
    for (; n >= 0; n--, a0 += 8) {
        int16_t sx = (int16_t)rd16(a0), sy = (int16_t)rd16(a0 + 2);
        if (y0 > sy || y1 < sy || x0 > sx || x1 < sx) continue;
        wr16(a6, (uint16_t)(sy + 0x70 - y0));
        a6[2] = a0[7];
        a6[3] = (uint8_t)d6;
        wr16(a6 + 4, (uint16_t)((rd16(a0 + 6) & 0xF800) + rd16(a0 + 4) + g.arenaanimchars));
        wr16(a6 + 6, (uint16_t)(sx + 0x70 - x0));
        a6 += 8;
        d6++;
        if (d6 == 0x40) break;
    }
    *sat = a6;
    *link = d6;
}

/* period94.asm:PrintPlayerNameRight. The name of player d0 of team a2, moved left so it ends
 * before column $29 (a trailing pad byte or two not counted), then the hot / cold icon. */
void PrintPlayerNameRight(uint16_t player, uint8_t *tm)
{
    g.iconplayer = player;
    uint32_t a1 = FormatPlayerName(player, tm);
    const uint8_t *a0 = ram_ptr(a1);
    uint16_t d5 = rd16(a0);
    int16_t d0 = (int16_t)d5;
    if (a0[d5 - 1] == 0) {
        d0--;
        if (a0[d5 - 2] == 0) d0--;
    }
    d0 = (int16_t)(d0 + g.printx - 0x29);
    if (d0 >= 0) g.printx -= d0;
    print(a1);
    HotColdIcon(tm);
}

/* period94.asm:SkipNameNumber. Unreferenced: the "NN F. Last" string without its first 2
 * characters. */
uint32_t SkipNameNumber(uint16_t player, uint8_t *tm)
{
    uint32_t a1 = FormatPlayerNumInitial(player, tm);
    uint8_t *p = ram_ptr(a1);
    wr16(p + 2, (uint16_t)(rd16(p) - 2));
    return a1 + 2;
}

/* period94.asm:SetStatScroll. Plane A vertical scroll = VertLineScrolling - $50, with disflags
 * bit 2 set while it writes. */
void SetStatScroll(void)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(2);
    vdp_ctrl(VSRAM_WRITE + (2u << 16));
    vdp_data((uint16_t)(0xFFB0 + g.VertLineScrolling));
    g.disflags = saved;
}

/* period94.asm:StatScrollRow / StatScrollRowEnd. The row of the scroll position. */
uint16_t StatScrollRow(void) { return g.VertLineScrolling >> 4; }
uint16_t StatScrollRowEnd(void) { return (uint16_t)((g.VertLineScrolling >> 4) + 6); }

/* period94.asm:PrintStatScrollArrows. The scroll arrows of the Game Statistics screen. */
void PrintStatScrollArrows(void)
{
    printsmallz(str_PrintStatScrollArrows_1, sizeof str_PrintStatScrollArrows_1);
    if (g.VertLineScrolling != 0) printsmallz(str_PrintStatScrollArrows_2, sizeof str_PrintStatScrollArrows_2);
    else printsmallz(str_PrintStatScrollArrows_3, sizeof str_PrintStatScrollArrows_3);
    if (g.matchupvis != g.VertLineScrolling) printsmallz(str_PrintStatScrollArrows_4, sizeof str_PrintStatScrollArrows_4);
    else printsmallz(str_PrintStatScrollArrows_5, sizeof str_PrintStatScrollArrows_5);
    printsmallz(str_PrintStatScrollArrows_6, sizeof str_PrintStatScrollArrows_6);
}

/* period94.asm:DisplayTeamStatsScreen. The team logos, then each stat row (without the power
 * play rows when penalties are off) centred, with the home value at x $22 and the visitors'
 * at x 9. */
void DisplayTeamStatsScreen(void)
{
    printz(str_DisplayTeamStatsScreen_1, sizeof str_DisplayTeamStatsScreen_1);
    PutTeamBlock(0x2C);
    printz(str_DisplayTeamStatsScreen_2, sizeof str_DisplayTeamStatsScreen_2);
    PutTeamBlock(0);
    SetStatScroll();
    printz(str_DisplayTeamStatsScreen_3, sizeof str_DisplayTeamStatsScreen_3);
    int rows = 0xE;
    uint32_t a1 = SYM_TeamStatTextTbl;
    if (g.OptPen == 0) { rows = 0xA; a1 = SYM_TeamStatTextTblNoPen; }
    for (; rows >= 0; rows--) {
        g.printx = (uint16_t)(0x15 - (rd16(rom_ptr(a1)) >> 1));
        uint32_t a0 = print(a1);                          /* print leaves a1 after the string */
        g.printx = 0x22;
        PrintStatValue(rom_ptr(a0), team_struct(0));
        g.printx = 9;
        PrintStatValue(rom_ptr(a0), team_struct(1));
        a1 = a0 + 4;
        g.printy += 2;
    }
}

/* period94.asm:PrintStatValue. Print the stat row a0 (two team struct offsets) for team a2
 * centred at printx: the value (offsets $A and $352 are times), "/second" if any, " (pct%)"
 * for passing; offset $FFFF is the shooting percentage. */
void PrintStatValue(const uint8_t *a0, const uint8_t *tm)
{
    uint32_t mes = RAM_BASE + RAM_OFF_mesarea;
    wr16(ram_ptr(mes), 2);
    uint16_t off = rd16(a0);
    if (off == 0xFFFF) {
        uint32_t d0 = (uint32_t)rd16(tm + 0xC) * 0x64;
        uint16_t d1 = rd16(tm);
        if (d1 != 0) d0 = m68k_divu(d0, d1);
        appstring(PushNumber((uint16_t)d0), mes);
        appendz(str_PrintStatValue_1, sizeof str_PrintStatValue_1, mes);
    } else {
        uint16_t d0 = rd16(tm + off);
        uint32_t a1;
        if (off == 0x352) a1 = PushTime(d0);
        else {
            if (off != 0xA) a1 = PushNumber(d0);
            else a1 = PushTime(d0);
        }
        appstring(a1, mes);
        int16_t off2 = (int16_t)rd16(a0 + 2);
        if (off2 >= 0) {
            appendz(str_PrintStatValue_2, sizeof str_PrintStatValue_2, mes);
            appstring(PushNumber(rd16(tm + off2)), mes);
            if (off == 0x14) {                            /* passing: the percentage */
                appendz(str_PrintStatValue_3, sizeof str_PrintStatValue_3, mes);
                uint32_t p = (uint32_t)rd16(tm + off) * 0x64;
                uint16_t d1 = rd16(tm + off2);
                if (d1 != 0) p = m68k_divu(p, d1);
                appstring(PushNumber((uint16_t)p), mes);
                appendz(str_PrintStatValue_4, sizeof str_PrintStatValue_4, mes);
            }
        }
    }
    g.printx -= rd16(ram_ptr(mes)) >> 1;
    print(mes);
}

/* period94.asm:updatePPTeamTime. One more second of power play time for the team on the power
 * play. */
void updatePPTeamTime(void)
{
    if (!(g.sflags2 & HI(5))) return;
    uint8_t *tm = team_struct((g.sflags2 & HI(6)) != 0);
    wr16(tm + 0x352, (uint16_t)(rd16(tm + 0x352) + 1));
}

/* period94.asm:GetTeamRating. The TeamRatings byte of team $28(a2). */
uint16_t GetTeamRating(const uint8_t *tm)
{
    return rom_ptr(SYM_TeamRatings)[rd16(tm + 0x28)];
}

/* period94.asm:ShortenMsgTimer. Cap msgtimer at 2, unless a second pad is on and a3 is not the
 * puck carrier. */
void ShortenMsgTimer(struct SortObj *o)
{
    if (g.cont2team != 0 && (uint16_t)o->SCnum != g.puckc) return;
    if ((int16_t)g.msgtimer > 2) g.msgtimer = 2;
}
