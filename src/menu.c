/* menu94.asm: the pause / intermission menu (a title, items with handlers, a selection marker)
 * and two helpers of the stats screens. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include "menu_strings_gen.h"

#define PRINTSMALLZ(s) printsmallz(s, sizeof s)

/* menuitem: the selected item, then (+2) the first item shown */
static uint16_t menu_top(void) { return g.menu.top; }
static void set_menu_top(uint16_t v) { g.menu.top = v; }
static uint16_t menu_item(void) { return g.menu.item; }
static void set_menu_item(uint16_t v) { g.menu.item = v; }
/* the menu list: a title String, the marker String, then the items (a String and a handler long
 * each; an item whose text word is negative ends the list) */
static uint32_t skip_string(uint32_t a) { return a + (int16_t)m16(a); }

/* menu94.asm:SetMenuPrintX. printx = the left edge of the menu box: 5 in 32 column mode, else 9. */
void SetMenuPrintX(void)
{
    g.printx = 5;
    if (g.disflags & HI(df32c)) return;
    g.printx += 4;
}

/* menu94.asm:PrintMenuItem. Print item a1 -> a1 past its text. An item whose text starts with
 * 'x' is the goalie option: "Manual Goalie" or "Auto Goalie" by the pause pad's goalie mode. */
uint32_t PrintMenuItem(uint32_t item)
{
    if (m8(item + 2) != 'x') return printsmall(item);
    static const uint8_t manual[] = "  Manual Goalie   ", automatic[] = "   Auto Goalie    ";
    uint16_t mode = g.sflags & HI(sfpj) ? g.goaliemode2 : g.goaliemode1;
    const uint8_t *text = mode ? automatic : manual;
    printsmall(rom_find(SYM_PrintMenuItem, SYM_PrintMenuItem + 0x80, text, sizeof manual - 1) - 2);
    return skip_string(item);
}

/* menu94.asm:UpdateMenuSelection. Clamp the selection to the items, scroll the four rows shown,
 * and print the menu: the title, the rows (the selected one marked), { and } when there are
 * items above / below. -> d1 = the row counter as the 68k leaves it. */
uint16_t UpdateMenuSelection(void)
{
    int16_t sel = (int16_t)menu_item();
    if (sel < 0) { set_menu_item(0); sel = 0; }
    uint32_t a = skip_string(skip_string(g.menulist));
    int16_t left = sel;
    while (--left != -1) {                               /* dbmi: stop at the last item */
        a = skip_string(a) + 4;
        if ((int16_t)m16(a + 2) < 0) break;
    }
    set_menu_item((uint16_t)(menu_item() - (left + 1)));
    int16_t d0 = (int16_t)menu_item();
    if (d0 < (int16_t)menu_top()) set_menu_top((uint16_t)d0);
    d0 = (int16_t)(d0 - 3);
    if (d0 > (int16_t)menu_top()) set_menu_top((uint16_t)d0);

    SetMenuPrintX();
    g.printy = 0xD;
    uint32_t item = printsmall(g.menulist);                /* the title */
    PRINTSMALLZ(str_UpdateMenuSelection_1);              /* clear the four rows */
    item = skip_string(item);
    for (uint16_t n = menu_top(); n; n--) item = skip_string(item) + 4;
    uint16_t rows = 3;
    g.printy = 0xC;
    SetMenuPrintX();
    uint16_t row = menu_top();
    if (row != 0) PRINTSMALLZ(str_UpdateMenuSelection_2);   /* { */
    int last;
    do {
        SetMenuPrintX();
        PRINTSMALLZ(str_UpdateMenuSelection_3);
        uint32_t marker = printsmall(g.menulist);
        if (row == menu_item()) printsmall(marker);
        item = PrintMenuItem(item);
        row++;
        item += 4;
        last = (int16_t)m16(item + 2) < 0;
    } while (!last && rows-- != 0);
    if (!last) {
        SetMenuPrintX();
        PRINTSMALLZ(str_UpdateMenuSelection_4);           /* } */
    }
    return rows;
}

/* menu94.asm:DrawMenuScreen. Draw the screen behind the menu (menudraw), frame the menu box,
 * print it and fade in. -> d1 as UpdateMenuSelection leaves it. */
uint16_t DrawMenuScreen(void)
{
    call68k(g.menudraw);
    PRINTSMALLZ(str_DrawMenuScreen_1);
    SetMenuPrintX();
    Framer(0x16, 6);
    uint16_t d1 = UpdateMenuSelection();
    g.palcount = 0x18;
    return d1;
}

/* menu94.asm:InitMenuState. Start a menu: a0 = item list, a1 = screen draw routine, item 0
 * selected and shown first; falls into DrawMenuScreen -> d1. */
uint16_t InitMenuState(uint32_t items, uint32_t draw)
{
    g.menulist = items;
    g.menudraw = draw;
    set_menu_item(0);
    set_menu_top(0);
    return DrawMenuScreen();
}

/* menu94.asm:HandleMenuInput. Act on the pad bits d1: down / up move the selection, C runs the
 * selected item's handler (with a2 = the pause pad's team) and redraws the menu unless the
 * handler drew its own screen (sflags5 bit 1). -> Z (1): leave the menu (start, or C on item 0).
 * d1 comes back as the redraw leaves it (a handler's own registers are not followed). */
int HandleMenuInput(uint16_t *d1)
{
    if (*d1 & 0x80) return 1;
    if (*d1 & 2) { set_menu_item((uint16_t)(menu_item() + 1)); *d1 = UpdateMenuSelection(); return 0; }
    if (*d1 & 1) { set_menu_item((uint16_t)(menu_item() - 1)); *d1 = UpdateMenuSelection(); return 0; }
    if (!(*d1 & 0x20)) return 0;
    seta2();
    uint32_t a = skip_string(skip_string(g.menulist));
    a = skip_string(a);
    for (uint16_t n = menu_item(); n; n--) a = skip_string(a + 4);
    call68k(m32(a));
    int keep = (g.sflags5 & 2) != 0;
    g.sflags5 &= (uint8_t)~2;
    if (!keep) *d1 = DrawMenuScreen();
    return menu_item() == 0;
}

/* menu94.asm:PutTeamBlock. Two rows of 11 map words from TeamBlockMap + d0 at printx / printy;
 * printx moves on 11. */
void PutTeamBlock(uint16_t off)
{
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    uint8_t *src = g.TeamBlockMap + off;
    for (int r = 0; r < 2; r++) {
        xyVmMap();
        for (int i = 0; i < 11; i++, src += 2) vdp_data(rd16(src));
        g.printy++;
    }
    g.printx += 0xB;
    g.printy -= 2;
    g.disflags = saved;
}

/* menu94.asm:vcountwait. Wait until vcount moves off oldvcount, then put it back. */
void vcountwait(void)
{
    uint16_t v = g.oldvcount;
    while (g.vcount == v) hal_wait_vblank();
    g.vcount = g.oldvcount;
}
