/* optsetup94.asm: the game setup screen. The option menu (up / down move the frame over the nine
 * lines, six shown at a time; left / right change an option within its limits), the team blocks
 * with their logos and the player cards between them, then the game (Start) or, after six
 * minutes without a press, the demo.
 *
 * Options (d7 = line * 2, the offset from OptPlayMode): 0 OptPlayMode (Regular Season, Continue
 * Playoffs, New Playoffs, New Playoffs/7 game, Shootout), 2 OptNOP, 4 Opt1Team, 6 Opt2Team,
 * 8 OptPerlen, $A OptGoalie, $C OptUserRec (0 On), $E OptPen (0 Off), $10 OptLine (0 On, 2 Auto).
 * setupcardflags (high byte) bits: 0 a player card up, 1 the card's side (visitors), 2 / 3 the
 * home / visitors block drawn, 5 C pressed, 6 / 7 the home / visitors logo shown. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "optsetup_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))
#define CARD(n) HI(n)                                    /* setupcardflags bits (btst on the word's high byte) */
static uint16_t option(uint16_t line) { return rd16(RAM_W(RAM_OFF_OptPlayMode) + line); }

/* (rules: more period lengths) their names as the setup prints them, padded like PerLengthValues */
#define LENGTH_TEXT(s) "\x00\x16" s                    /* (a String: its length, 22, then the text) */
static const uint8_t more_lengths[7][22] = {
    LENGTH_TEXT("5 Minutes           "), LENGTH_TEXT("7 Minutes           "), LENGTH_TEXT("10 Minutes          "),
    LENGTH_TEXT("15 Minutes          "), LENGTH_TEXT("20 Minutes          "), LENGTH_TEXT("2 Minutes           "),
    LENGTH_TEXT("3 Minutes           "),
};

/* optsetup94.asm:GameSetUp. The setup screen: the line data and the saved playoff state, the
 * modes' start changes undone (SetupStart), the screen (setoptions), random teams after a demo,
 * the playoff tree, then the menu until Start (SetupStart) or six minutes (SetupDemo). From
 * Opening2. */
void GameSetUp(void)
{
    ReadLineData();
    ReadPassBits(g.pwddatabuffer);
    g.OptLine = g.TmpOptLine2;
    g.OptPlayMode = g.TempOptPlayMode;
    g.sflags7 &= (uint8_t)~2;
    ClearGameStats();
    ReadNameLog();
    setoptions();
    g.setupfirstline = 0;
    g.setupprevline = 0;
    PrintOptionNames();
    if (g.demoflag == 0) {                                /* after a demo: random teams */
        g.Opt1Team = randomd0(league.teams);
        g.Opt2Team = randomd0(league.teams);
    }
    ST_W(g.demoflag);
    if (g.OptPlayMode != 4) {
        if ((int16_t)g.OptPlayMode >= 2) GoNewPlayoffs();
        if (g.OptPlayMode == 1) GoContinuePlayoffs();
    }
    uint16_t d7 = MoveMenuFrame(0, 0);                    /* the frame on line 0 */
    FixModeOptions(d7);
    g.setuphome = g.HomeTeam;
    g.logoteam = g.HomeTeam;
    DrawHomeBlock();
    g.setupvis = g.VisTeam;
    g.logoteam = g.VisTeam;
    DrawVisBlock();
    g.palcount = 0x18;
    g.disflags &= (uint16_t)~HI(dfng);                    /* fade the screen in */
    for (;;) {
        if (g.OptPlayMode == 4) {                         /* Shootout: line changes, penalties and user records off */
            g.OptLine = 1;
            g.OptPen = 0;
            g.OptUserRec = 1;
        }
        uint16_t d1 = ReadSetupPads();
        if (d1 == 0) { SetupDemo(); return; }
        if (d1 & 0x80) { SetupStart(); return; }
        int16_t d0;
        if (d1 & 2) {                                     /* down: the next line, past those that do not apply */
            d0 = g.OptNOP == 0 && d7 == 8 ? 4 : 2;
            if (g.OptPlayMode != 4 && g.OptNOP == 0 && d7 == 8) d0 = 6;
            else if (d7 == 6 && g.OptPlayMode == 4) d0 = g.OptNOP != 0 ? 4 : 0;
            else if (d7 == 0xA) {
                if (g.OptPlayMode == 4) d0 = 0;
                else if ((int16_t)g.ValidSRAM < 0) d0 = 4;
            }
            d7 = MoveMenuFrame(d7, d0);
            FixModeOptions(d7);
            continue;
        }
        if (d1 & 1) {                                     /* up: the same skips */
            if (g.OptNOP == 0 && d7 == 0xC) {
                d0 = -4;
            } else {
                d0 = -2;
                if (g.OptPlayMode != 4 && g.OptNOP == 0 && d7 == 0xE) {
                    d7 = MoveMenuFrame(d7, -6);
                    FixModeOptions(d7);
                    continue;
                }
            }
            if (d7 == 0xA && g.OptPlayMode == 4) d0 = -4;
            else if (d7 == 0xE && (g.OptPlayMode == 4 || (int16_t)g.ValidSRAM < 0)) d0 = -4;
            d7 = MoveMenuFrame(d7, d0);
            FixModeOptions(d7);
            continue;
        }
        int16_t d2;
        if (d1 & 8) d2 = 1;                               /* right */
        else if (d1 & 4) d2 = -1;                         /* left */
        else continue;
        if (g.OptPlayMode != 4) FigureJoy();
        ChangeOption(d7, d2);
        if (g.OptPlayMode == 4 || g.OptPlayMode == 0) {   /* Regular Season and Shootout: the menu's teams */
            g.HomeTeam = g.Opt1Team;
            g.VisTeam = g.Opt2Team;
        }
        g.logoteam = g.HomeTeam;
        if (!(g.setupcardflags & CARD(6)) || g.setuphome != g.HomeTeam) {
            g.setuphome = g.HomeTeam;
            DrawHomeBlock();
        }
        if (!(g.setupcardflags & CARD(7)) || g.setupvis != g.VisTeam) {
            g.setupvis = g.VisTeam;
            g.logoteam = g.VisTeam;
            DrawVisBlock();
            g.logoteam = g.HomeTeam;
        }
        FixModeOptions(d7);
    }
}

/* optsetup94.asm:GoNewPlayoffs. New playoffs: NewPO. */
void GoNewPlayoffs(void)
{
    NewPO();
}

/* optsetup94.asm:GoContinuePlayoffs. Continue playoffs: ContinuePlayoffs. */
void GoContinuePlayoffs(void)
{
    ContinuePlayoffs();
}

/* optsetup94.asm:MoveMenuFrame. Move the frame from line d7 by d0 (+-2 a line): scroll the six
 * lines shown (setupfirstline 0-3; setupprevline the one before, sflags5 bit 2 on a scroll),
 * skip line 1-3 in continued playoffs and Team 2 in new playoffs, redraw the menu background
 * under the old frame and draw the new one -> d7 = the new line. */
uint16_t MoveMenuFrame(uint16_t d7, int16_t d0)
{
    for (;;) {
        g.sflags5 &= (uint8_t)~4;
        if (d0 < 0 && (int16_t)d7 >> 1 == (int16_t)g.setupfirstline) {   /* up past the first line shown */
            g.setupprevline = g.setupfirstline;
            g.sflags5 |= 4;
            if ((int16_t)--g.setupfirstline < 0) g.setupfirstline = 0;
        }
        g.TempWord1 = d7;
        d7 = (uint16_t)(d7 + d0);
        if ((int16_t)d7 < 0) d7 = 0;
        if (d0 >= 0 && ((int16_t)d7 >> 1) - (int16_t)g.setupfirstline > 5) {   /* down past the last */
            g.setupprevline = g.setupfirstline;
            g.sflags5 |= 4;
            g.setupfirstline++;
            if (((int16_t)d7 >> 1) - (int16_t)g.setupfirstline > 5) g.setupfirstline++;
            if ((int16_t)g.setupfirstline > 3) g.setupfirstline = 3;
        }
        if (!(g.sflags5 & 4)) g.setupprevline = g.setupfirstline;
        if ((int16_t)d7 >= 0x12) d7 = (uint16_t)(d7 - d0);   /* past Line Changes */
        if (g.OptPlayMode == 1 && d7 != 0 && d7 <= 6) continue;
        if ((int16_t)g.OptPlayMode >= 2 && g.OptPlayMode != 4 && d7 == 6) continue;
        break;
    }
    uint16_t w, h;
    SetMenuArea(&w, &h);
    struct { uint32_t pal, map; } m = { ART(SetupMenuMap) + rd32(rom_ptr(ART(SetupMenuMap))), ART(SetupMenuMap) + rd32(rom_ptr(ART(SetupMenuMap) + 4)) };
    g.sflags6 |= HI(0);                                   /* the map only */
    dobitmap(m.pal, m.map, ART(SetupMenuMap) + 8, (uint16_t)(g.printx - 1), (uint16_t)(g.printy - 0xE), 0x17, h, g.setupmenuchars, 0);
    g.sflags6 &= (uint16_t)~HI(0);
    SetFrameRect(d7, &w, &h);
    g.printa = 0x6000;
    Framer(w, h);
    return d7;
}

/* optsetup94.asm:SetMenuArea. The menu area: printx $10, printy $E -> d0 = 23 x d1 = 13. */
void SetMenuArea(uint16_t *w, uint16_t *h)
{
    printz(str_SetMenuArea_1, sizeof str_SetMenuArea_1);
    *w = 0x17;
    *h = 0xD;
}

/* optsetup94.asm:SetFrameRect. The frame of line d3: at row $E + d3 - setupfirstline * 2,
 * -> d0 = 23 x d1 = 3, or 5 for Team 1 in the new playoffs (a two-line frame). */
void SetFrameRect(uint16_t line, uint16_t *w, uint16_t *h)
{
    printz(str_SetFrameRect_1, sizeof str_SetFrameRect_1);
    g.printy += line;
    g.printy -= (uint16_t)(g.setupfirstline * 2);
    *w = 0x17;
    *h = 3;
    if ((int16_t)g.OptPlayMode < 2 || g.OptPlayMode == 4 || line != 4) return;
    *h = 5;
}

/* optsetup94.asm:rtsSetup. The shared rts. */
void rtsSetup(void) {}

/* optsetup94.asm:FixModeOptions. What a mode does not allow: Shootout and Demo turn User
 * Records off, Demo sets both goalies to Auto Control; then the names after a scroll and the
 * values (PrintOptions) in the setup font (sflags6 bit 3). */
void FixModeOptions(uint16_t d7)
{
    if (g.OptPlayMode == 4) g.OptUserRec = 1;
    if (g.OptNOP == 0) {
        g.OptUserRec = 1;
        g.OptGoalie = 1;                                  /* (0 first, then 1) */
        g.goaliemode1 = 1;
        g.goaliemode2 = 1;
    }
    if (g.setupfirstline != g.setupprevline) PrintOptionNames();
    g.sflags6 |= HI(3);
    PrintOptions(d7);
    g.sflags6 &= (uint16_t)~HI(3);
}

/* optsetup94.asm:ChangeOption. Add d2 to option d7 (d2 = 0: only keep it in its range) and wrap
 * it (WrapOption). Entering Shootout keeps User Records, Penalties and Line Changes
 * (TmpOptUserRec ...) and leaving it puts them back; Team 1 in the new playoffs ranges over the
 * playoff teams; Players depends on the mode and FourWayPlay (3 is skipped in a shootout). A new
 * play mode starts its playoffs (ContinuePlayoffs, or past it when nothing is saved; NewPO). */
void ChangeOption(uint16_t line, int16_t d2)
{
    if (line == 0 && d2 != 0) {
        int16_t mode = (int16_t)(g.OptPlayMode + d2);
        if (mode < 0) mode = 4;
        if (mode == 4) {                                  /* into Shootout */
            g.TmpOptUserRec = g.OptUserRec;
            g.TmpOptPen = g.OptPen;
            g.TmpOptLine = g.OptLine;
        }
        if (g.OptPlayMode == 4) {                         /* out of it */
            g.OptUserRec = g.TmpOptUserRec;
            g.OptPen = g.TmpOptPen;
            g.OptLine = g.TmpOptLine;
        }
    }
    uint8_t *opt = RAM_W(RAM_OFF_OptPlayMode) + line;
    uint32_t limits = g.FourWayPlay ? SYM_OptionLimits4Way : SYM_OptionLimits;
    uint16_t lo = 0, hi = rd16(rom_ptr(limits + line)), v = rd16(opt);
    if (line == 4 || line == 6) hi = league.teams;        /* (the league's; the cartridge's table says 28) */
    if ((int16_t)g.OptPlayMode >= 2 && g.OptPlayMode != 4 && line == 4) hi = league.playoff_teams;
    if (line == 8 && rules[RULE_PERIOD_LENGTHS]) hi = 7;  /* (rules: more period lengths) */
    if (line == 2) {                                      /* Players */
        if (g.OptPlayMode == 4) {
            int16_t t = (int16_t)(d2 + g.OptNOP);
            if (t < 0) t += 7;
            if (t > 4) d2 = d2 < 0 ? 4 : (int16_t)(d2 + 2);
            else if ((int16_t)(d2 + g.OptNOP) == 3) d2 = d2 < 0 ? (int16_t)(d2 - 1) : (int16_t)(d2 + 1);
        }
        lo = 5;
        if (g.FourWayPlay) { hi = 0xA; lo += 2; }
        if (g.OptPlayMode == 4 || g.OptPlayMode == 0) { lo = 0; hi = 5; }
        goto more;
    }
    for (;;) {
        v = WrapOption(v, d2, hi, lo);
        if (g.FourWayPlay && line == 2) {
            if (d2 >= 0) { if (v == 9 || v == 0xA) v = 0xB; }
            else if (v == 0xA) v = 8;
        }
        wr16(opt, v);
        if (d2 == 0) return;
        if (g.OptPlayMode == 1 && line == 0) {
            ContinuePlayoffs();
            if ((g.gamelevel | g.bosgames) != 0) return;
            goto more;                                    /* nothing saved: on to the next mode */
        }
        if ((int16_t)g.OptPlayMode < 2 || g.OptPlayMode == 4) return;
        if (line == 0 || line == 4) GoNewPlayoffs();
        return;
    more:
        if (g.FourWayPlay) hi += 2;
    }
}

/* optsetup94.asm:WrapOption. d3 + d2 wrapped into d5 ... d4 - 1 -> d3. */
uint16_t WrapOption(uint16_t v, int16_t d2, uint16_t hi, uint16_t lo)
{
    v = (uint16_t)(v + d2);
    if ((int16_t)v < (int16_t)lo) v = (uint16_t)(hi - 1);
    if ((int16_t)v >= (int16_t)hi) v = lo;
    return v;
}

/* optsetup94.asm:PrintOptions. Keep each option in its range (ChangeOption, d2 = 0) and print
 * the values of the lines shown, the scroll marks and the matchup bitmaps; unless a player card
 * is up, restart the card timer. */
void PrintOptions(uint16_t d7)
{
    for (uint16_t line = 0;; line += 2) {
        ChangeOption(line, 0);
        if ((int16_t)line >> 1 >= (int16_t)g.setupfirstline) PrintOptionValue(line);
        if (line == 0x10) break;
    }
    PrintScrollMarks();
    DrawMatchupBitmaps();
    if (!(g.setupcardflags & CARD(0))) ResetCardTimer();
}

/* optsetup94.asm:PrintOptionValue. The value of option d7 at x $11 on its row (none past row
 * $1B): the team names (PrintOptTeamName), 'N/A' for the period length of a shootout, the
 * FourWayPlay list of Players. */
void PrintOptionValue(uint16_t line)
{
    printz(str_PrintOptionValue_1, sizeof str_PrintOptionValue_1);
    g.printy -= (uint16_t)(g.setupfirstline * 2);
    g.printy += line;
    if ((int16_t)g.printy > 0x1B) return;
    g.printy--;
    g.printx = 0x11;
    if (line == 4 || line == 6) { PrintOptTeamName(line); return; }
    uint16_t n = option(line);
    uint32_t a0 = SYM_OptionValueOffsets + (int16_t)rd16(rom_ptr(SYM_OptionValueOffsets + line)), a1;
    if (a0 == SYM_PerLengthValues && g.OptPlayMode == 4) { print(SYM_NATxt); return; }
    if (a0 == SYM_PerLengthValues && rules[RULE_PERIOD_LENGTHS]) { printz(more_lengths[n % 7], 22); return; }
    if (g.FourWayPlay && line == 2) a0 += 0xB0;           /* the 4 Way Play Players list */
    do { a1 = a0; a0 += (uint32_t)(int16_t)rd16(rom_ptr(a0)); } while (n-- != 0);
    print(a1);
}

/* optsetup94.asm:PrintScrollMarks. At x 2: '}' at row $19 when lines follow below, '{' at row
 * $F when lines are above, else a space. */
void PrintScrollMarks(void)
{
    g.printy = 0x19;
    uint32_t a1 = SYM_ScrollClearTxt;
    if (g.setupfirstline != 3 && g.OptPlayMode != 4) a1 = SYM_ScrollDownTxt;
    g.printx = 2;
    print(a1);
    a1 = SYM_ScrollClearTxt;
    g.printy = 0xF;
    if (g.setupfirstline != 0) a1 = SYM_ScrollUpTxt;
    g.printx = 2;
    print(a1);
}

/* optsetup94.asm:PrintOptTeamName. A blank value, then team d3 of option d7 (its TeamList city;
 * the full names of the two New York teams). */
void PrintOptTeamName(uint16_t line)
{
    uint16_t x = g.printx;
    print(SYM_BlankValueTxt);
    g.printx = x;
    uint16_t team = option(line);
    uint32_t a1 = rd32(rom_ptr(LG(TeamList) + (uint16_t)(team << 2)));
    a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1 + 4));
    if (team == 0xE) a1 = SYM_RangersTxt;
    else if (team == 0xD) a1 = SYM_IslandersTxt;
    print(a1);
}

/* optsetup94.asm:PrintOptionNames. The option names (OptionNames) at x 3, two rows apart from
 * row $F - setupfirstline * 2; only rows $F-$19 are printed. */
void PrintOptionNames(void)
{
    g.sflags6 |= HI(3);
    uint32_t a1 = SYM_OptionNames;
    g.printy = (uint16_t)(0xF - g.setupfirstline * 2);
    for (;; g.printy += 2) {
        if ((int16_t)g.printy < 0xF) { a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1)); continue; }
        if ((int16_t)g.printy > 0x19) break;
        g.printx = 3;
        g.printa = 0;
        g.printm = 0;
        a1 = printsmall(a1);
    }
    g.sflags6 &= (uint16_t)~HI(3);
}

/* the dobitmap arguments of a picture: its palettes and map from the two offsets at its start */
static uint32_t pic_pal(uint32_t img) { return img + rd32(rom_ptr(img)); }
static uint32_t pic_map(uint32_t img) { return img + rd32(rom_ptr(img + 4)); }

/* optsetup94.asm:setoptions. Build the setup screen, with its own vblank (VBlank_SetOptions)
 * and the graphics held back (dfng): the team bitmap tiles (LoadSetupTiles), the framer, small
 * font and setup font tiles, the screen and menu bitmaps, the logo box tiles, the sprites. */
void setoptions(void)
{
    static const uint8_t framer[8] = { 0x03, 0x56, 0x75, 0x64, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t font[8] = { 0x0F, 0xC0, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t font2[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t logobox[8] = { 0x03, 0x41, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    g.vbint = SYM_VBlank_SetOptions;
    cpu_set_sr(0x2500);
    g.disflags &= (uint16_t)~HI(dfok);
    g.disflags |= HI(dfng);
    g.disflags &= (uint16_t)~HI(df32c);
    g.VSCRLPM = 0;
    g.VSPRITES = 0xB400;
    g.VmMap3 = 0xB800;
    g.Map3col1 = 5;
    g.VmMap2 = 0xC000;
    g.Map2col1 = 6;
    g.VmMap1 = 0xE000;
    g.Map1col1 = 6;
    setVram(0);
    orjoy();
    uint16_t c = LoadSetupTiles();
    g.framercset = c;
    c = DecompressGraphicsWithCallback(ART(SetupFramerMap) + 8, c, rom_find(SYM_setoptions, SYM_ReadSetupPads, framer, 8));
    g.smallfontchars = c;
    c = DecompressGraphicsWithCallback(ART(SmallFontMap) + 8, c, rom_find(SYM_setoptions, SYM_ReadSetupPads, font, 8));
    g.setupfontchars = c;
    c = DecompressGraphicsWithCallback(ART(PrintFont2Map) + 8, c, rom_find(SYM_setoptions, SYM_ReadSetupPads, font2, 8));
    printz(str_setoptions_1, sizeof str_setoptions_1);
    eraser(0x28, 0x1C, 0x7FF);
    printz(str_setoptions_2, sizeof str_setoptions_2);
    c = dobitmap(pic_pal(ART(GameSetUpMap)), pic_map(ART(GameSetUpMap)), ART(GameSetUpMap) + 8, 0, 0, 0x28, 0x1C, c, 0xD);
    g.vispicchars = c;
    c += 0x24;
    printz(str_setoptions_3, sizeof str_setoptions_3);
    c = dobitmap(pic_pal(ART(GameSetUpMap2)), pic_map(ART(GameSetUpMap2)), ART(GameSetUpMap2) + 8, 0, 0, 0x28, 4, c, 0);
    printz(str_setoptions_4, sizeof str_setoptions_4);
    g.setupmenuchars = c;
    c = dobitmap(pic_pal(ART(SetupMenuMap)), pic_map(ART(SetupMenuMap)), ART(SetupMenuMap) + 8, 0, 0, 0x26, 0xD, c, 0);
    g.homepicchars = c;
    c += 0x24;
    g.logobox1chars = c;
    c = DecompressGraphicsPlain(ART(LogoBoxMap) + 8, c);
    g.logobox2chars = c;
    c = DecompressGraphicsWithCallback(ART(LogoBoxMap) + 8, c, rom_find(SYM_setoptions, SYM_ReadSetupPads, logobox, 8));
    defaultsprites2(c);
    sortobj(0)->OldXpos = 0x28;
    sortobj(6)->OldXpos = 0x28;
    g.tmstruct[0] = 0xFF;
    g.AwShots[0] = 0xFF;
}

/* optsetup94.asm:ReadSetupPads. Each frame for up to $5460 frames (six minutes): the pads (four
 * with FourWayPlay) with key repeat -> d1 = the first presses other than C (0 at the end). C
 * brings the next player card sooner (setupcardflags bit 5). After a player card, a team block
 * left without its logo is drawn again. In the first $E10 frames, and while a card is up, the
 * player cards run (PlayerCardTimer); otherwise their timer restarts. */
uint16_t ReadSetupPads(void)
{
    uint16_t d6 = 0x5460, d1 = 0;
    do {
        while ((uint16_t)(g.vcount - g.oldvcount) == 0) hal_wait_vblank();
        g.oldvcount = g.vcount;
        g.setupcardflags &= (uint16_t)~CARD(5);
        UpdateRosterPlayers();
        uint16_t d0, d2, d3;
        ReadJoy1(&d0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        if (d1) {
            if (!(d1 & 0x20)) return d1;
            if (d2 & 0x20) g.cardtimer = 0x53;
            g.setupcardflags |= CARD(5);
        }
        ReadJoy2(&d0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        if (d1) {
            if (!(d1 & 0x20)) return d1;
            g.setupcardflags |= CARD(5);
        }
        if (g.FourWayPlay) {
            ReadJoy3(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            if (d1) {
                if (!(d1 & 0x20)) return d1;
                g.setupcardflags |= CARD(5);
            }
            ReadJoy4(&d0, &d1, &d2, &d3);
            ProcessInputWithRepeat(&d1, d2, d3);
            if (d1) {
                if (!(d1 & 0x20)) return d1;
                g.setupcardflags |= CARD(5);
            }
        }
        if (!((g.setupcardflags & CARD(6)) && (g.setupcardflags & CARD(7)))) {   /* a logo missing (after a card) */
            if (g.setupcardflags & CARD(6)) {
                g.logoteam = g.setupvis;
                DrawVisBlock();                           /* (which sets bit 7: the home block follows) */
            }
            if (g.setupcardflags & CARD(7)) {
                g.logoteam = g.setuphome;
                DrawHomeBlock();
            }
        }
        if ((int16_t)d6 > 0x4650 || (g.setupcardflags & CARD(0))) PlayerCardTimer();
        else ResetCardTimer();
    } while (d6-- != 0);
    return d1;
}

/* optsetup94.asm:SetupDemo. No press for six minutes: the demo (demoflag clear, a new seed,
 * Regular Season, Demo, line changes and penalties on); falls into SetupStart. */
void SetupDemo(void)
{
    g.demoflag = 0;
    wr16(RAM_W(RAM_OFF_seed), vdp_hv16());
    wr16(RAM_W(RAM_OFF_seed) + 2, vdp_hv16());
    g.OptPlayMode = 0;
    g.OptNOP = 0;
    g.OptLine = 0;
    g.OptPen = 1;
    SetupStart();
}

/* optsetup94.asm:SetupStart. Start: the Goalies option to both teams, pojoy, a new seed;
 * OptLine and OptPlayMode kept in TmpOptLine2 / TempOptPlayMode while Auto line changes play as
 * On (sflags7 bit 4) and Shootout as Regular Season (gmode2 bit 0, ClearShootout); then the
 * playoff tree (maketree). */
void SetupStart(void)
{
    g.goaliemode1 = g.OptGoalie;
    g.goaliemode2 = g.OptGoalie;
    UpdatePlayoffLevel();
    wr16(RAM_W(RAM_OFF_seed), vdp_hv16());
    wr16(RAM_W(RAM_OFF_seed) + 2, vdp_hv16());
    g.TmpOptLine2 = g.OptLine;
    if (g.OptLine == 2) {
        g.OptLine = 0;
        g.sflags7 |= 0x10;
    }
    g.TempOptPlayMode = g.OptPlayMode;
    if (g.OptPlayMode == 4) {
        g.OptPlayMode = 0;
        g.gmode2 |= HI(0);
        ClearShootout();
    }
    maketree();
}

/* the colour table LoadSetupTiles remaps the visitors' TeamBitmaps with (its 68k address) */
uint32_t SetupTilesRemap(void)
{
    static const uint8_t table[8] = { 0x03, 0x41, 0x25, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    return rom_find(SYM_LoadSetupTiles, SYM_UpdateRosterPlayers, table, 8);
}

/* optsetup94.asm:LoadSetupTiles. The TeamBitmaps tiles from char 2, then again remapped
 * (teambitmapchars) -> d4 = the char after them. */
uint16_t LoadSetupTiles(void)
{
    uint16_t c = DecompressGraphicsPlain(ART(TeamBitmaps) + 8, 2);
    g.teambitmapchars = c;
    return DecompressGraphicsWithCallback(ART(TeamBitmaps) + 8, c, SetupTilesRemap());
}

/* optsetup94.asm:UpdateRosterPlayers. Empty. */
void UpdateRosterPlayers(void) {}

/* optsetup94.asm:EraseCard. If a player card is up (cleared here, and both logos forgotten):
 * erase its logo box (8 x 8) and its text box (25 x 8). */
void EraseCard(void)
{
    if (!(g.setupcardflags & CARD(0))) return;
    g.setupcardflags &= (uint16_t)~(CARD(0) | CARD(6) | CARD(7));
    printz(str_EraseCard_1, sizeof str_EraseCard_1);
    g.printy = 5;
    if (g.setupcardflags & CARD(1)) {                     /* the visitors' card: on the left */
        g.printx = 3;
        eraser(8, 8, 0x7FF);
        printz(str_EraseCard_2, sizeof str_EraseCard_2);
        g.printx = 0xB;
    } else {
        g.printx = 0x1C;
        eraser(8, 8, 0x7FF);
        printz(str_EraseCard_3, sizeof str_EraseCard_3);
        g.printx = 3;
    }
    g.printy = 5;
    eraser(0x19, 8, 0x7FF);
}

/* optsetup94.asm:DrawHomeBlock. The home team block: the card erased, the logo box and the
 * HomeTeam logo (logoteam) at x $19. */
void DrawHomeBlock(void)
{
    EraseCard();
    g.setupcardflags |= CARD(2) | CARD(6);
    LogoBoxHome();
    if (g.setupcardflags & CARD(3)) {
        g.setupcardflags &= (uint16_t)~CARD(3);
        printz(str_DrawHomeBlock_1, sizeof str_DrawHomeBlock_1);
        g.printx = 8;                                     /* (set up for an erase it does not make) */
        g.printy = 5;
    }
    printz(str_DrawHomeBlock_2, sizeof str_DrawHomeBlock_2);
    g.printx = 0x19;
    DrawTeamLogo();
}

/* optsetup94.asm:DrawVisBlock. The same for the visitors: the logo box, the logo at x 9. */
void DrawVisBlock(void)
{
    EraseCard();
    g.setupcardflags |= CARD(3) | CARD(7);
    LogoBoxVis();
    if (g.setupcardflags & CARD(2)) {
        g.setupcardflags &= (uint16_t)~CARD(2);
        printz(str_DrawVisBlock_1, sizeof str_DrawVisBlock_1);
        g.printx = 0x18;                                  /* (the same) */
        g.printy = 5;
    }
    printz(str_DrawVisBlock_2, sizeof str_DrawVisBlock_2);
    g.printx = 9;
    DrawTeamLogo();
}

/* optsetup94.asm:DrawTeamLogo. The logo of team logoteam at printx, y 6 (6 x 6), with its
 * palette from TeamLogoPalettes in colour family 1 (home) or 2 (visitors), the rest of the
 * palettes kept; then $B4 frames before a player card. */
void DrawTeamLogo(void)
{
    g.carddelay = 0xB4;
    g.printy = 6;
    int home = (g.setupcardflags & CARD(2)) != 0;
    uint16_t chars = home ? g.homepicchars : g.vispicchars, team = g.logoteam;
    uint32_t bmp = rd32(rom_ptr(LG(TeamLogoBitmaps) + (uint16_t)(team << 2)));
    uint32_t pal = LG(TeamLogoPalettes) + (uint32_t)(int16_t)(uint16_t)((uint16_t)(team << 5) - (home ? 0x20 : 0x40));
    uint8_t keep[8];
    memcpy(keep, RAM_W(RAM_OFF_palfadenew + 0x22), 8);
    dobitmap(pal, pic_map(bmp), bmp + 8, 0, 0, 6, 6, chars, home ? 2 : 4);
    memcpy(RAM_W(RAM_OFF_palfadenew + 0x22), keep, 8);
    g.palcount = 0x64;
}

/* optsetup94.asm:LogoBoxRight. The logo box at x $1C (logobox1chars). */
void LogoBoxRight(void)
{
    g.printx = 0x1C;
    DrawLogoBox(g.logobox1chars);
}

/* optsetup94.asm:LogoBoxHome. The logo box at x $18 (home block). */
void LogoBoxHome(void)
{
    g.printx = 0x18;
    DrawLogoBox(g.logobox1chars);
}

/* optsetup94.asm:LogoBoxLeft. The logo box at x 3 (logobox2chars). */
void LogoBoxLeft(void)
{
    g.printx = 3;
    DrawLogoBox(g.logobox2chars);
}

/* optsetup94.asm:LogoBoxVis. The logo box at x 8 (visitors block). */
void LogoBoxVis(void)
{
    g.printx = 8;
    DrawLogoBox(g.logobox2chars);
}

/* optsetup94.asm:DrawLogoBox. The LogoBoxMap box (8 x 8) at printx, y 5 from chars d4. */
void DrawLogoBox(uint16_t chars)
{
    g.printa = 0;
    g.printy = 5;
    g.sflags6 |= HI(0);
    dobitmap(pic_pal(ART(LogoBoxMap)), pic_map(ART(LogoBoxMap)), ART(LogoBoxMap) + 8, 0, 0, 8, 8, chars, 0);
    g.sflags6 &= (uint16_t)~HI(0);
}
