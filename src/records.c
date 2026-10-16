/* records94.asm: new in 94, the user records. The players enter their names (the NAME ENTRY
 * screen: a name log of seven names, a letter grid to edit one) before a game with user records
 * on; the Record Holders screen of the menus comes later. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "records_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))
#define PRINTZ(s) printz(s, sizeof s)
#define BUF (RAM_BASE + RAM_OFF_nameentrybuf)

/* records94.asm:UserNameEntry. With user records on (OptUserRec 0): the name entry for pad 1's
 * team, then pad 2's when it has the other. */
void UserNameEntry(void)
{
    if (g.OptUserRec != 0) return;
    g.homeuser = 0;
    g.awayuser = 0;
    if (g.cont1team != 0) {
        g.nameentryvis = (uint16_t)(g.cont1team - 1);
        NameEntryScreen();
    }
    if (g.cont2team != 0 && g.cont1team != g.cont2team) {
        g.nameentryvis = (uint16_t)(g.cont2team - 1);
        NameEntryScreen();
    }
}

/* the letter grid cursor: erase the box at letter d5 (eraser with char 1) and print the letter
 * over it without the frame */
static void unframe_letter(uint16_t d5)
{
    uint16_t w, h;
    LetterGridPos(d5, &w, &h);
    uint16_t x = g.printx, y = g.printy;
    eraser(w, h, 1);
    g.printy = (uint16_t)(y + 1);
    g.printx = (uint16_t)(x + 1);
    PrintCurLetter(d5);
}

/* records94.asm:NameEntryScreen. The NAME ENTRY screen for team nameentryvis (with the setup
 * screen's vblank): its name and logo, the name log; up / down pick a name, B edits it in the
 * letter grid (A / C move along the name, the pad moves over the letters, B back to the log),
 * Start stores it (StoreUserName). */
void NameEntryScreen(void)
{
    static const uint8_t frame1[8] = { 0x91, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t frame2[8] = { 0x91, 0x23, 0x45, 0x60, 0x89, 0xAB, 0xCD, 0xEF };
    static const uint8_t bigfont[8] = { 0x71, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    cpu_set_sr(0x2700);
    uint16_t d4 = 2;
    g.vbint = SYM_VBlank_SetOptions;
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
    cpu_set_sr(0x2500);
    orjoy();
    g.framercset = d4;
    d4 = DecompressGraphicsWithCallback(SYM_FramerMap + 8, d4, rom_find(SYM_NameEntryScreen, SYM_NameLetterGrid, frame1, 8));
    g.nameframechars = d4;
    d4 = DecompressGraphicsWithCallback(SYM_FramerMap + 8, d4, rom_find(SYM_NameEntryScreen, SYM_NameLetterGrid, frame2, 8));
    g.smallfontchars = d4;
    d4 = AddSmallFont(d4);
    PRINTZ(str_NameEntryScreen_1);
    eraser(0x28, 0x1C, 0x7FF);
    setupIceRinkPal();
    PRINTZ(str_NameEntryScreen_2);
    uint32_t img = SYM_ScoutMap;
    d4 = dobitmap(img + rd32(rom_ptr(img)), img + rd32(rom_ptr(img + 4)), img + 8, 0, 0, 0x28, 0x1C, d4, 0xD);
    g.homepicchars = d4;
    d4 += 0x24;
    g.logobox1chars = d4;
    d4 = DecompressGraphicsPlain(SYM_LogoBoxMap + 8, d4);
    g.BigFontChars = d4;
    DecompressGraphicsWithCallback(SYM_BigFontMap + 8, d4, rom_find(SYM_NameEntryScreen, SYM_NameLetterGrid, bigfont, 8));
    PRINTZ(str_NameEntryScreen_3);                        /* the team's city, centred on x $1F */
    uint16_t team = g.nameentryvis ? g.VisTeam : g.HomeTeam;
    uint32_t blk = rd32(rom_ptr(SYM_TeamList + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    uint32_t city = blk + (uint32_t)(int16_t)rd16(rom_ptr(blk + 4));
    g.printx -= (uint16_t)((uint16_t)(rd16(rom_ptr(city)) - 2) >> 1);
    print(city);
    PRINTZ(str_NameEntryScreen_4);
    Framer(8, 8);
    PRINTZ(str_NameEntryScreen_5);                        /* its logo */
    uint32_t bmp = rd32(rom_ptr(SYM_TeamLogoBitmaps + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    uint32_t pal = SYM_TeamLogoPalettes + (uint32_t)(int16_t)(uint16_t)((uint16_t)(team << 5) - 0x40);
    dobitmap(pal, bmp + rd32(rom_ptr(bmp + 4)), bmp + 8, 0, 0, 6, 6, g.homepicchars, 4);
    PRINTZ(str_NameEntryScreen_6);
    Framer(0x24, 6);
    printbigz(str_NameEntryScreen_7, sizeof str_NameEntryScreen_7);
    ReadNameLog();
    g.namelogarrows = 0;
    g.namelogsel = 1;
    SkipOtherUserName(0);
    GetLogName(BUF);
    PrintNameLog();
    PRINTZ(str_NameEntryScreen_8);
    g.palcount = 0x18;
    g.disflags &= (uint16_t)~HI(dfng);
    d4 = 0;                                               /* the cursor in the name */
    uint16_t d5 = SyncLetterCursor();                     /* the cursor in the letter grid */
    PrintNameCursor(d4);
    uint16_t w, h, d1;
    int16_t d0;
    LetterGridPos(d5, &w, &h);
    {
        uint16_t x = g.printx, y = g.printy;
        NameEntryFramer(w, h);
        g.printy = (uint16_t)(y + 1);
        g.printx = (uint16_t)(x + 1);
    }
    PrintCurLetter(d5);
    g.cursoron = 1;
    NameLetterGrid();
    d0 = 0;
    goto move_cursor;

loop:
    GetNameLength();
    PRINTZ(str_NameEntryScreen_9);
    if (g.cursoron) {
        PrintNameField(BUF);
        PrintNamePrompt();
    }
    MoveLogArrows();
    for (;;) {                                            /* the next press of this team's pad */
        uint16_t v = g.vcount;
        while (g.vcount == v) hal_wait_vblank();
        uint16_t j0, j2, j3;
        if ((uint16_t)(g.cont1team - 1) == g.nameentryvis) ReadJoy1(&j0, &d1, &j2, &j3);
        else ReadJoy2(&j0, &d1, &j2, &j3);
        if (d1) break;
    }
    if (d1 & 0x80) {                                      /* Start: done */
        StoreUserName();
        return;
    }
    if (!g.cursoron) {                                    /* the name log */
        if (d1 & 2) d0 = 1;
        else if (d1 & 1) d0 = -1;
        else if (d1 & 0x10) { NameLetterGrid(); d0 = 0; }   /* B: edit */
        else goto loop;
        g.namelogsel += (uint16_t)d0;
        do {
            if ((int16_t)g.namelogsel > 7) g.namelogsel = 1;
            if (g.namelogsel == 0) g.namelogsel = 7;
        } while (SkipOtherUserName(d0));
        GetLogName(BUF);
        if (!g.cursoron) { NameEntryHelp(); goto loop; }
        PRINTZ(str_NameEntryScreen_10);                   /* the letter grid: the cursor back to the name's start */
        g.printx += d4;
        PRINTZ(str_NameEntryScreen_11);
        unframe_letter(d5);
        d5 = SyncLetterCursor();
        d0 = (int16_t)-d4;
        goto move_cursor;
    }
    /* the letter grid */
    if (d1 & 0x40) { d0 = -1; goto move_cursor; }         /* A: back a letter */
    if (d1 & 0x20) { d0 = 1; goto move_cursor; }          /* C: on a letter */
    if (d1 & 8) d0 = 1;
    else if (d1 & 4) d0 = -1;
    else if (d1 & 2) d0 = 6;
    else if (d1 & 1) d0 = -6;
    else {
        if (d1 & 0x10) NameLetterGrid();                  /* B: back to the log */
        goto loop;
    }
    goto move_grid;

move_cursor:                                              /* .11: the name cursor by d0 */
    d0 = (int16_t)(d0 + d4);
    if ((uint16_t)d0 > 0xB) goto loop;
    d4 = (uint16_t)d0;
    PrintNameCursor(d4);
    d0 = 0;
    if (g.nameentrybuf[d4] != '-')                        /* the grid cursor to that letter */
        d0 = (int16_t)(FindLetter((uint16_t)(int16_t)(int8_t)g.nameentrybuf[d4]) - d5);

move_grid:                                                /* .12: the grid cursor by d0 */
    d0 = (int16_t)(d0 + d5);
    if ((uint16_t)d0 > 0x1D) goto loop;
    unframe_letter(d5);
    d5 = (uint16_t)d0;
    LetterGridPos(d5, &w, &h);
    {
        uint16_t x = g.printx, y = g.printy;
        if (g.cursoron) NameEntryFramer(w, h);
        g.printy = (uint16_t)(y + 1);
        g.printx = (uint16_t)(x + 1);
    }
    PrintCurLetter(d5);
    g.nameentrybuf[d4] = rom_ptr(SYM_LetterGrid)[d5];
    goto loop;
}

/* records94.asm:NameLetterGrid. Toggle the letter grid (cursoron): on, the grid of letters and
 * its help; off, the name back from the log, its area cleared and the log's help. */
void NameLetterGrid(void)
{
    PRINTZ(str_NameLetterGrid_1);
    eraser(0x16, 7, 0x7FF);
    int was = g.cursoron & 1;
    g.cursoron ^= 1;
    if (!was) {
        PRINTZ(str_NameLetterGrid_2);
        const uint8_t *grid = rom_ptr(SYM_LetterGrid);
        uint8_t *mes = RAM_W(RAM_OFF_mesarea);
        for (int row = 0; row < 5; row++) {               /* 6 letters a row, a space after each */
            wr16(mes, 0xE);
            for (int i = 0; i < 6; i++) { mes[2 + 2 * i] = *grid++; mes[3 + 2 * i] = ' '; }
            print(RAM_BASE + RAM_OFF_mesarea);
            g.printy += 2;
            g.printx -= 0xC;
        }
        PRINTZ(str_NameLetterGrid_3);
        Framer(0x16, 7);
        PRINTZ(str_NameLetterGrid_4);
        g.printx = 2;
        g.printy++;
        uint16_t x = g.printx;
        PRINTZ(str_NameLetterGrid_5);
        g.printx = x;
        g.printy++;
        PRINTZ(str_NameLetterGrid_6);
        g.printx = x;
        g.printy++;
        PRINTZ(str_NameLetterGrid_7);
        g.printx = x;
        g.printy++;
        PRINTZ(str_NameLetterGrid_8);
        return;
    }
    GetLogName(BUF);
    g.printx = 0;
    g.printy = 0xF;
    eraser(0x28, 0xD, 0x7FF);
    PRINTZ(str_NameLetterGrid_9);
    Framer(0x16, 7);
    NameEntryHelp();
}

/* records94.asm:NameEntryHelp. The name log's help text. */
void NameEntryHelp(void)
{
    PRINTZ(str_NameEntryHelp_1);
    PRINTZ(str_NameEntryHelp_2);
    g.printx = 2;
    uint16_t x = g.printx;
    g.printy++;
    PRINTZ(str_NameEntryHelp_3);
    g.printy++;
    g.printx = x;
    PRINTZ(str_NameEntryHelp_4);
    g.printy++;
    g.printx = x;
    PRINTZ(str_NameEntryHelp_5);
}

/* records94.asm:PrintNamePrompt. "Enter new name:" for an empty name log entry namelogsel, else
 * "Select or replace:". */
void PrintNamePrompt(void)
{
    uint16_t at = (uint16_t)(g.namelogsel * 0xC);
    print(g.namelog[(int16_t)at] == 0 ? SYM_EnterNameTxt : SYM_SelectNameTxt);
}

/* records94.asm:SyncLetterCursor. -> d5 = the grid index of the name's first letter (which is
 * set to the grid's letter), 0 when it is not in the grid. */
uint16_t SyncLetterCursor(void)
{
    uint16_t i = FindLetter(g.nameentrybuf[0]);
    if ((int16_t)i >= 0x1E) return 0;
    g.nameentrybuf[0] = rom_ptr(SYM_LetterGrid)[i];
    return i;
}

/* records94.asm:FindLetter. -> d0 = the index of letter d0 in LetterGrid (the first 31), $1E when
 * not there. */
uint16_t FindLetter(uint16_t letter)
{
    const uint8_t *grid = rom_ptr(SYM_LetterGrid);
    for (uint16_t i = 0; i <= 0x1E; i++) if (grid[i] == (uint8_t)letter) return i;
    return 0x1E;
}

/* records94.asm:PrintNameCursor. The " < " cursor under letter d4 of the name (when the grid is
 * on). */
void PrintNameCursor(uint16_t d4)
{
    PRINTZ(str_PrintNameCursor_1);
    g.printx += d4;
    if (!g.cursoron) return;
    PRINTZ(str_PrintNameCursor_2);
}

/* records94.asm:rtsNameCursor. The shared rts. */
void rtsNameCursor(void) {}

/* records94.asm:PrintCurLetter. Grid letter d5 at printx / printy (when the grid is on). */
void PrintCurLetter(uint16_t d5)
{
    if (!g.cursoron) return;
    uint8_t *s = g.ThreeStars;
    wr16(s, 4);
    s[3] = 0;
    s[2] = rom_ptr(SYM_LetterGrid)[d5];
    print(RAM_BASE + RAM_OFF_ThreeStars);
}

/* records94.asm:LetterGridPos. printx / printy of grid letter d5 (6 a row, 2 cells apart) -> d0 /
 * d1 = its 3 x 3 box. */
void LetterGridPos(uint16_t d5, uint16_t *w, uint16_t *h)
{
    PRINTZ(str_LetterGridPos_1);
    uint32_t q = m68k_divu((uint32_t)(int32_t)(int16_t)d5, 6);
    g.printy += (uint16_t)((uint16_t)q << 1);
    g.printx += (uint16_t)((uint16_t)(q >> 16) << 1);
    *w = 3;
    *h = 3;
}

/* records94.asm:PrintNameLog. The seven names of the name log (GetLogName, PrintNameField). */
void PrintNameLog(void)
{
    uint16_t length = g.namelength, sel = g.namelogsel;
    PRINTZ(str_PrintNameLog_1);
    g.namelogsel = 1;
    for (int i = 0; i < 7; i++) {
        GetLogName(RAM_BASE + RAM_OFF_TempBuffer);
        uint16_t x = g.printx;
        PrintNameField(RAM_BASE + RAM_OFF_TempBuffer);
        g.printx = x;
        g.printy++;
        g.namelogsel++;
    }
    g.namelogsel = sel;
    g.namelength = length;
}

/* records94.asm:MoveLogArrows. When the selection namelogsel moved: the old ] [ arrows erased,
 * the new ones printed (PrintLogArrows). */
void MoveLogArrows(void)
{
    if (g.namelogsel == g.namelogarrows) return;
    PrintLogArrows(SYM_LogArrowsClrTxt);
    PrintLogArrows(SYM_LogArrowsTxt);
}

/* records94.asm:PrintLogArrows. The two Strings at a1 on name log row namelogarrows (x 3 and
 * $12); then namelogarrows = namelogsel. */
void PrintLogArrows(uint32_t a1)
{
    if (g.namelogarrows != 0) {
        PRINTZ(str_PrintLogArrows_1);
        g.printy += (uint16_t)(g.namelogarrows - 1);
        print(a1);
        a1 += (uint32_t)(int16_t)rd16(rom_ptr(a1));
        g.printx = 0x12;
        print(a1);
    }
    g.namelogarrows = g.namelogsel;
}

/* records94.asm:GetNameLength. namelength = the length of the name being entered (up to a '-'
 * or 0, at most 12). */
void GetNameLength(void)
{
    g.namelength = 0;
    for (int i = 0; i < 12; i++) {
        uint8_t c = g.nameentrybuf[i];
        if (c == '-' || c == 0) return;
        g.namelength++;
    }
}

/* records94.asm:PrintNameField. The 12-character name field from a1, '-' past namelength. */
void PrintNameField(uint32_t name)
{
    const uint8_t *src = mem_ptr(name);
    uint8_t *s = g.ThreeStars;
    wr16(s, 0xE);
    for (int16_t i = 0; i < 0xC; i++) s[2 + i] = i < (int16_t)g.namelength ? src[i] : '-';
    print(RAM_BASE + RAM_OFF_ThreeStars);
}

/* records94.asm:NameEntryFramer. Framer with the name entry frame tiles (nameframechars). */
void NameEntryFramer(uint16_t w, uint16_t h)
{
    uint16_t saved = g.framercset;
    g.framercset = g.nameframechars;
    Framer(w, h);
    g.framercset = saved;
}

/* records94.asm:GetLogName. Name log entry namelogsel to a1 ('-' for each 0), namelength = its
 * letters. */
void GetLogName(uint32_t dst)
{
    uint8_t *d = mem_ptr(dst);
    uint16_t at = (uint16_t)(g.namelogsel * 0xC);
    g.namelength = 0;
    for (int i = 0; i < 12; i++, at++) {
        uint8_t c = g.namelog[(int16_t)at];
        if (c == 0) { c = '-'; g.namelength--; }
        *d++ = c;
        g.namelength++;
    }
}

/* records94.asm:StoreUserName. Store the name entered ('-' as 0) as name log entry namelogsel
 * when it changed: the log to save RAM (WriteNameLog), the records of whoever had that entry
 * forgotten (WriteNameRecord); then this pad's user is the entry (0 for no name). */
void StoreUserName(void)
{
    uint8_t *log = g.namelog + (int16_t)(uint16_t)(g.namelogsel * 0xC), *buf = g.nameentrybuf;
    int i;
    for (i = 0; i < 12; i++) {
        if (buf[i] == '-') buf[i] = 0;
        if (buf[i] != log[i]) break;
    }
    if (i < 12) {
        for (i = 0; i < 12; i++) log[i] = buf[i] == '-' ? 0 : buf[i];
        WriteNameLog();
        WriteNameRecord();
        MakeSRAMChecksum();
    }
    if (g.nameentrybuf[0] == 0) g.namelogsel = 0;
    if (g.nameentryvis) g.awayuser = g.namelogsel;
    else g.homeuser = g.namelogsel;
}

/* records94.asm:WriteNameRecord. A name log entry replaced: user namelogsel goes from every save
 * RAM record (the player records' holder and opponent user bytes, the crowd records', its team
 * record cleared). */
void WriteNameRecord(void)
{
    uint8_t *r = g.ThreeStars, user = (uint8_t)g.namelogsel;
    uint32_t at = 0;
    for (int n = 0; n <= 0x2D7; n++, at += 4) {           /* the player records */
        int changed = 0;
        ReadSRAM(at, 4, RAM_BASE + RAM_OFF_ThreeStars);
        if (r[1] == user) { r[1] = 0; changed = 1; }
        if (r[3] == user) { r[3] = 0; changed = 1; }
        if (changed) WriteSRAM(at, 4, RAM_BASE + RAM_OFF_ThreeStars);
    }
    at = SYM_CrowdRecBase;
    for (int n = 0; n < (int)SYM_NumTeams; n++, at += SYM_CrowdRecBytes) {   /* the crowd records */
        int changed = 0;
        ReadSRAM(at, 0x10, RAM_BASE + RAM_OFF_ThreeStars);
        for (int b = 1; b <= 0xB; b += 2) if (r[b] == user) { r[b] = 0; changed = 1; }
        if (changed) WriteSRAM(at, 0x10, RAM_BASE + RAM_OFF_ThreeStars);
    }
    at = SYM_TeamRecBase + (uint32_t)(int32_t)(int16_t)(uint16_t)(g.namelogsel << 4);   /* its team record */
    ReadSRAM(at, 0x10, RAM_BASE + RAM_OFF_ThreeStars);
    for (int b = 0; b < 0x10; b++) r[b] = 0;
    WriteSRAM(at, 0x10, RAM_BASE + RAM_OFF_ThreeStars);
    MakeSRAMChecksum();
}

/* records94.asm:SkipOtherUserName. When namelogsel is the name the other pad picked, step past it
 * in the direction d0 (0 counts as forward) -> whether it stepped (Z). */
int SkipOtherUserName(int16_t dir)
{
    uint16_t other = g.nameentryvis ? g.homeuser : g.awayuser;
    if (other != g.namelogsel) return 0;
    g.namelogsel += dir >= 0 ? 1 : (uint16_t)-1;
    return 1;
}
