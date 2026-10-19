/* scout94.asm: the scouting report, the pregame MATCHUPS screen: Ron Barr's text box typing the
 * paragraphs (the text player, UpdateScoutingDisplay in setup94), and seven pages of matchups
 * (the teams' logos and ratings, then the two players of each position) with marks toward the
 * side that has the advantage. */
#include <string.h>
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "scout_strings_gen.h"

#define PRINTZ(s) printz(s, sizeof s)
#define TEMPBUF (RAM_BASE + RAM_OFF_TempBuffer)

/* scout94.asm:ScoutCrowdRecord. The text player's '@' -> a1 = the home team's crowd record in dB
 * (save RAM team record byte 8; 80 when none) as a String at TempBuffer. */
uint32_t ScoutCrowdRecord(void)
{
    ReadCrowdRecord((uint32_t)(int32_t)(int16_t)g.HomeTeam, RAM_BASE + RAM_OFF_teamrecbuf);
    uint16_t db = g.teamrecbuf[8];
    if (db == 0) db = 0x50;
    AppendNumber(db, TEMPBUF);
    return TEMPBUF;
}

/* scout94.asm:ScoutingReport. The MATCHUPS screen before a game (with the fades-only vblank):
 * the hot / cold players, the song, Ron Barr and his text (the paragraph list in TestList: hello,
 * welcome, the teams' form, their hot / cold players, the crowd record, start), the matchups. A /
 * C page them (they turn by themselves every $10E frames), down types fast, Start leaves; it ends
 * when the text is done (Demo), else after about ten minutes more. */
void ScoutingReport(void)
{
    static const uint8_t bigfont[8] = { 0x05, 0x43, 0x15, 0x67, 0x79, 0xAB, 0xCD, 0xEF };
    static const uint8_t smallfont[8] = { 0x0A, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t framer[8] = { 0x05, 0x42, 0x25, 0x61, 0x89, 0xAB, 0xCD, 0xEF };
    ReadNameLog();
    BuildHotColdLists();
    g.advframe = 0;
    song(SYM_SngScout);
    g.vbint = SYM_vb2;
    clearTeamStats();
    Create_HotCold_Table(team_struct(0));
    Create_HotCold_Table(team_struct(1));
    BuildHotColdLists();
    g.disflags &= (uint16_t)~HI(df32c);
    g.Map1col1 = 6;
    g.Map2col1 = 6;
    setVram(0);
    uint16_t c = 2;
    g.homepicchars = c;
    c += 0x24;
    g.vispicchars = c;
    c += 0x24;
    g.BigFontChars = c;
    c = DecompressGraphicsWithCallback(ART(BigFontMap) + 8, c, rom_find(SYM_ScoutingReport, SYM_PageMatchup, bigfont, 8));
    c = AddSmallFont(c);
    g.smallfont2chars = c;
    c = DecompressGraphicsWithCallback(ART(SmallFontMap) + 8, c, rom_find(SYM_ScoutingReport, SYM_PageMatchup, smallfont, 8));
    g.framercset = c;
    c = DecompressGraphicsWithCallback(ART(FramerMap) + 8, c, rom_find(SYM_ScoutingReport, SYM_PageMatchup, framer, 8));
    PRINTZ(str_ScoutingReport_1);
    uint32_t img = ART(ScoutMap);
    c = dobitmap(img + rd32(rom_ptr(img)), img + rd32(rom_ptr(img + 4)), img + 8, 0, 0, 0x28, 0x1C, c, 3);
    printbigz(str_ScoutingReport_2, sizeof str_ScoutingReport_2);
    g.printfontset = 0;
    PRINTZ(str_ScoutingReport_3);
    PRINTZ(str_ScoutingReport_4);
    Framer(0x21, 9);                                      /* the text box */
    PRINTZ(str_ScoutingReport_5);
    Framer(8, 8);                                         /* the visitors' picture */
    PRINTZ(str_ScoutingReport_6);
    Framer(8, 8);                                         /* the home picture */
    PRINTZ(str_ScoutingReport_7);
    img = ART(RonBarrMap);
    uint32_t map = img + rd32(rom_ptr(img + 4));
    c = dobitmap(img + rd32(rom_ptr(img)), map, img + 8, 0, 0, rd16(rom_ptr(map)), rd16(rom_ptr(map) + 2), c, 0xC);
    g.hoticonchars = c;
    c = DecompressGraphicsPlain(ART(HotIconMap) + 8, c);
    g.coldiconchars = c;
    DecompressGraphicsPlain(ART(ColdIconMap) + 8, c);
    g.matchup = 0;
    g.scoutunused = 0;
    g.matchuptimer = 0x10D;
    GetMatchupPlayers();
    DrawMatchupPictures();
    PrintMatchupRatings();
    g.palcount = 0x18;
    g.vbint = SYM_vb2;
    cpu_set_sr(0x2500);
    rp list = InitScoutingDisplay();                      /* the paragraphs of ScoutTextScript */
    #define PARA(n) (w16(list, 0, n), list = rp_add(list, 2))
    PARA(0x1B);                                           /* "Hi, I'm Ron Barr ..." */
    PARA(0x1D);                                           /* (a blank line) */
    PARA(0x1A);                                           /* "Welcome to a sold out ^ ..." */
    PARA(0x1D);
    int16_t form = (int16_t)CompareHotColdTotals();
    if (form >= 0) { PARA((uint16_t)form); PARA(0x1D); }
    PARA(0x1E);                                           /* the home hot / cold player */
    PARA(0x1D);
    if (g.HomeTeam != g.VisTeam) PARA(0x1F);              /* the visitors' */
    PARA(0x1D);
    PARA(0x20);                                           /* "The ^ crowd record is @ dB." */
    PARA(0x1D);
    PARA(0x21);                                           /* "Hit the start button ..." */
    w16(list, 0, 0xFFFF);
    g.asv = 0;
    g.screentimer = 0x7FFF;
    g.scoutwait = 0x8CA0;
    for (;;) {
        uint16_t d0, d1, d2, d3;
        waitx(0, &d0, &d1, &d2, &d3);
        if ((int16_t)--g.matchuptimer < 0) g.matchuptimer = 0x10E;
        PrintAdvantageMarks();
        if (d1 & 0x80) break;                             /* Start */
        int page = 0;
        if (g.matchuptimer == 0x10E) {
            page = 1;                                     /* the timer ran out: the next matchup */
        } else if (d1 & 0x20) {                           /* C: the next */
            g.screentimer = 0x7FFF;
            g.scoutwait = 0x8CA0;
            page = 1;
        } else if (d1 & 0x40) {                           /* A: the previous */
            g.screentimer = 0x7FFF;
            g.scoutwait = 0x8CA0;
            page = -1;
        } else {
            g.asv = 0;
            if (d1 & 2) ST_W(g.asv);                      /* down: the rest without delays */
        }
        if (page) {
            RestartAdvantageMarks();
            PageMatchup((int16_t)page);
        }
        if (g.waitxpad & 2) ST_W(g.asv);                  /* down held */
        UpdateScoutingDisplay();
        if ((int16_t)--g.screentimer >= 0) continue;
        if (g.OptNOP == 0) break;                         /* Demo: done when the text is */
        g.screentimer = 0xFFFF;
        if ((int32_t)--g.scoutwait < 0) break;
    }
    g.printfontset = 0;
}

/* scout94.asm:PageMatchup. The matchup by d0 (+1 / -1, wrapping in 0-6), the page timer again,
 * the players, pictures and ratings. */
void PageMatchup(int16_t by)
{
    g.matchuptimer = 0x10E;
    int16_t m = (int16_t)(by + g.matchup);
    if (m < 0) m = 6;
    else if (m >= 7) m = 0;
    g.matchup = (uint16_t)m;
    GetMatchupPlayers();
    DrawMatchupPictures();
    PrintMatchupRatings();
}

/* scout94.asm:PrintMatchupRating. Team a0's rating (GetTeamRating) as 2 digits at printx / printy
 * over two spaces -> d0 = the rating. */
uint16_t PrintMatchupRating(struct Team *tm)
{
    PrintTwoSpaces();
    uint16_t r = GetTeamRating(tm);
    printsmall(PushNumberWidth(r, 2));
    return r;
}

/* scout94.asm:PrintTwoSpaces. Two spaces at printx / printy, printx kept. */
void PrintTwoSpaces(void)
{
    uint16_t x = g.printx;
    printsmall(SYM_TwoSpacesTxt);
    g.printx = x;
}

/* scout94.asm:DrawMatchupPictures. The matchup's two 6 x 6 pictures, the visitors' at x 2, the
 * home team's at x $20, y $F: for matchup 0 the team logos, else the players' pictures. */
void DrawMatchupPictures(void)
{
    if (g.matchup == 0) {
        uint16_t d3 = g.VisTeam;
        uint32_t logo = GetTeamLogo(&d3);
        g.printx = 2;
        g.printy = 0xF;
        g.printa = 0x6000;
        DrawMatchupLogo(logo, d3, g.vispicchars, 4);
        d3 = g.HomeTeam;
        logo = GetTeamLogo(&d3);
        g.printx = 0x20;
        g.printy = 0xF;
        g.printa = 0;
        DrawMatchupLogo(logo, d3, g.homepicchars, 2);
    } else {
        uint32_t pic = DrawPlayerPicture(g.matchupvis, g.VisTeam);
        g.printx = 2;
        g.printy = 0xF;
        g.printa = 0;
        DrawMatchupPicture(pic, g.vispicchars, 2);
        pic = DrawPlayerPicture(g.matchuphome, g.HomeTeam);
        g.printx = 0x20;
        g.printy = 0xF;
        g.printa = 0;
        DrawMatchupPicture(pic, g.homepicchars, 0);
    }
    g.palcount = 0x64;
}

/* scout94.asm:DrawMatchupPicture. Player picture a0 (UnpackPicture) at printx / printy with the
 * PicturePalette palette (and its map when the picture has none). */
void DrawMatchupPicture(uint32_t pic, uint16_t chars, uint16_t palbits)
{
    uint32_t pal = ART(PicturePalette) + rd32(rom_ptr(ART(PicturePalette))), map;
    if (rd32(rom_ptr(pic + 4)) == 0) map = ART(PicturePalette) + rd32(rom_ptr(ART(PicturePalette) + 4));
    else map = pic + rd32(rom_ptr(pic + 4));
    UnpackPicture(pic + 8);
    DrawMatchupBitmap(pal, map, RAM_BASE + RAM_OFF_picturebuf, chars, palbits);
}

/* scout94.asm:DrawMatchupLogo. Team logo a0 at printx / printy, its palette from
 * TeamLogoPalettes by d3 (the team x 4, from GetTeamLogo) in colour family 1 (d5 = 2) or 2. */
void DrawMatchupLogo(uint32_t logo, uint16_t d3, uint16_t chars, uint16_t palbits)
{
    uint16_t off = (uint16_t)((uint16_t)(d3 << 3) - (palbits == 4 ? 0x40 : 0x20));
    DrawMatchupBitmap(LG(TeamLogoPalettes) + (uint32_t)(int16_t)off, logo + rd32(rom_ptr(logo + 4)), logo + 8, chars, palbits);
}

/* scout94.asm:DrawMatchupBitmap. A 6 x 6 dobitmap, then the 8 x 3 name area below it erased (at
 * y $19, x 2 or $20). */
void DrawMatchupBitmap(uint32_t pal, uint32_t map, uint32_t tiles, uint16_t chars, uint16_t palbits)
{
    dobitmap(pal, map, tiles, 0, 0, 6, 6, chars, palbits);
    g.printx = (int16_t)g.printx > 0x14 ? 0x20 : 2;
    g.printy = 0x19;
    eraser(8, 3, 0x7FF);
}

/* scout94.asm:GetTeamLogo. d3 = team -> a0 = its logo bitmap (TeamLogoBitmaps), d3 = team x 4. */
uint32_t GetTeamLogo(uint16_t *d3)
{
    *d3 = (uint16_t)(*d3 << 2);
    return rd32(rom_ptr(LG(TeamLogoBitmaps) + (uint32_t)(int16_t)*d3));
}

/* the rating of player d0 of team a2 (CalcAttrib with the overall weights of a skater or, in the
 * goalie matchup, a goalie) as a percentage, adjusted (AttribAdjust) */
static uint16_t player_rating(uint16_t player, struct Team *tm)
{
    uint32_t mask = rd32(rom_ptr(g.matchupslot ? SYM_PAttribOverallMask : SYM_GAttribOverallMask));
    uint16_t div, v = CalcAttrib(player, tm, mask, &div);
    return AttribAdjust((uint16_t)m68k_divu((uint32_t)v * 100u, div));
}

/* scout94.asm:PrintMatchupRatings. Matchup 0: 'Overall' and the teams' ratings (home at x $22,
 * visitors at x 4, y $16); 1-6: the position, the two players' names on row $17 (the home
 * player's right justified) and their ratings. The ratings to homerating / visrating. */
void PrintMatchupRatings(void)
{
    if (g.matchup == 0) {
        printsmallz(str_PrintMatchupRatings_1, sizeof str_PrintMatchupRatings_1);
        uint32_t a1 = SYM_ScoutRatingText;
        uint16_t n = 0;
        do {                                              /* only the tenth: ' Overall ' */
            g.printx = (uint16_t)(0x16 - ((int16_t)rd16(rom_ptr(a1)) >> 1));
            if (n == 9) {
                g.printfontset = 2;
                uint16_t y = g.printy;
                g.printy -= 3;
                a1 = printsmall(a1);
                g.printy = y;
            } else {
                a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
            }
            n++;
        } while ((int16_t)rd16(rom_ptr(a1)) >= 0);
        g.printfontset = 2;
        PRINTZ(str_PrintMatchupRatings_2);
        PRINTZ(str_PrintMatchupRatings_3);
        g.homerating = PrintMatchupRating(team_struct(0));
        g.printfontset = 2;
        PRINTZ(str_PrintMatchupRatings_4);
        g.visrating = PrintMatchupRating(team_struct(1));
        return;
    }
    uint32_t a1 = SYM_MatchupPosNames;                    /* the position */
    for (uint16_t n = (uint16_t)(g.matchup - 1); n-- != 0;) a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
    printsmallz(str_PrintMatchupRatings_5, sizeof str_PrintMatchupRatings_5);
    printsmall(a1);
    g.printy = 0x16;
    g.printfontset = 2;
    PRINTZ(str_PrintMatchupRatings_6);                    /* the home player */
    PRINTZ(str_PrintMatchupRatings_7);
    PrintPlayerNameRight(g.matchuphome, team_struct(0));
    uint16_t r = player_rating(g.matchuphome, team_struct(0));
    g.homerating = r;
    a1 = PushNumberWidth(r, 2);
    PRINTZ(str_PrintMatchupRatings_8);
    PrintTwoSpaces();
    printsmall(a1);
    PRINTZ(str_PrintMatchupRatings_9);                    /* the visitors' */
    PrintPlayerNameRight(g.matchupvis, team_struct(1));
    r = player_rating(g.matchupvis, team_struct(1));
    g.visrating = r;
    a1 = PushNumberWidth(r, 2);
    PRINTZ(str_PrintMatchupRatings_10);
    PrintTwoSpaces();
    printsmall(a1);
}

/* scout94.asm:GetMatchupPlayers. matchuphome / matchupvis = the two teams' players of the
 * matchup. */
void GetMatchupPlayers(void)
{
    g.matchuphome = GetMatchupPlayer(team_struct(0));
    g.matchupvis = GetMatchupPlayer(team_struct(1));
}

/* scout94.asm:GetMatchupPlayer. -> d0 = the roster index of team a0's player for the matchup:
 * slot MatchupLineSlots[matchup] (matchupslot; 0 goalie, 1 / 2 defense, 3 / 5 wings, 4 center)
 * of the first of its line sets. */
uint16_t GetMatchupPlayer(struct Team *tm)
{
    uint32_t data = tm->data;
    const uint8_t *lines = rom_ptr(data + (int16_t)rd16(rom_ptr(data + 6)));
    int16_t slot = (int8_t)rom_ptr(SYM_MatchupLineSlots)[g.matchup];
    g.matchupslot = (uint16_t)slot;
    return (uint16_t)((int8_t)lines[slot] - 1);
}

/* scout94.asm:RestartAdvantageMarks. The advantage marks from nothing again. */
void RestartAdvantageMarks(void)
{
    g.advcount = 0;
    g.advframe = 0;
}

/* scout94.asm:PrintAdvantageMarks. Each frame: the marks at x $13, y $16, ']' growing toward the
 * home team when its rating is higher, '[' toward the visitors when lower (one more every 7
 * frames, up to 3), blank when even. */
void PrintAdvantageMarks(void)
{
    if ((int16_t)++g.advcount >= 7) {
        g.advcount = 0;
        if ((int16_t)++g.advframe >= 6) g.advframe = 5;
    }
    uint16_t at = (uint16_t)(g.advframe << 2);
    uint32_t a1;
    if ((int16_t)g.homerating > (int16_t)g.visrating) a1 = rd32(rom_ptr(SYM_HomeAdvMarks + at));
    else if (g.homerating == g.visrating) a1 = SYM_EvenAdvTxt;
    else a1 = rd32(rom_ptr(SYM_VisAdvMarks + at));
    g.printfontset = 2;
    PRINTZ(str_PrintAdvantageMarks_1);
    printsmall(a1);
    g.printfontset = 0;
}

/* the team data Strings after the city (city, abbreviation, nickname, arena): String n of team a2 */
static uint32_t team_string(struct Team *tm, int n)
{
    uint32_t a1 = tm->data;
    a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1 + 4));
    while (n-- > 0) a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
    return a1;
}

/* scout94.asm:GetTeamNickname. The text player's '=' (a2 the home team) and '*' -> a1 = the
 * team's nickname, or 'Home' / 'Visitors' when it has none (the all-star teams). */
uint32_t GetTeamNickname(struct Team *tm)
{
    uint32_t a1 = team_string(tm, 2);
    if (rd16(rom_ptr(a1)) == 2) a1 = tm == team_struct(0) ? SYM_HomeTxt : SYM_VisitorsTxt;
    return a1;
}

/* scout94.asm:GetTeamArena. The text player's '^' and ';' -> a1 = the team's arena. */
uint32_t GetTeamArena(struct Team *tm)
{
    return team_string(tm, 3);
}
