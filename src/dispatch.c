/* jsr (a0) to a routine address the game keeps in RAM or in a table (menu handlers, the menu
 * draw routine): the C function of that 68k routine. Grows as the targets are ported. */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "symbols_gen.h"

/* the menu handlers that take the pause pad's team (a2, which HandleMenuInput set with seta2) */
static void player_stats(void) { PlayerStatsScreen(seta2()); }
static void rts(void) {}

static const struct { uint32_t addr; void (*fn)(void); } routines[] = {
    { SYM_rtss2, rts },                                   /* Resume Game */
    { SYM_PlayerStatsScreen, player_stats },
    { SYM_PlayoffStatsScreen, PlayoffStatsScreen },
    { SYM_ManualGoalieMenu, ManualGoalieMenu },
    { SYM_ReloadRinkGraphics, ReloadRinkGraphics },
    { SYM_SetupPauseScreen, SetupPauseScreen },
    { SYM_VBlank, VBlank },
    { SYM_vb2, vb2 },
    { SYM_VBcount, VBcount },
    { SYM_VBlank_TitleScreen, VBlank_TitleScreen },
    { SYM_VBlank_SetOptions, VBlank_SetOptions },
    { SYM_VBlank_Playoff, VBlank_Playoff },
    { SYM_ReplayMode, ReplayMode },
    { SYM_PeriodStatsScreen, PeriodStatsScreen },
    { SYM_GameStatisticsScreen, GameStatisticsScreen },
};

/* a target that is not ported yet: the tests replay its recorded call instead */
__attribute__((weak)) void call68k_missing(uint32_t addr)
{
    fprintf(stderr, "call68k: routine %06x is not ported\n", addr);
    abort();
}

void call68k(uint32_t addr)
{
    for (size_t i = 0; i < sizeof routines / sizeof *routines; i++)
        if (routines[i].addr == addr) { routines[i].fn(); return; }
    call68k_missing(addr);
}

/* movea.w #Stack,sp: StartPer and Opening2 restart the program with an empty stack, from
 * however deep the 68k was (IntermissionStart is reached from inside puckfaceoff). The C does
 * the same with a longjmp back to run68k, which then calls the routine. */
#include <setjmp.h>
static jmp_buf top;
static void (*volatile next_entry)(void);
static int running;

void run68k(void (*entry)(void))
{
    next_entry = entry;
    running = 1;
    setjmp(top);
    for (;;) next_entry();                                /* the game never returns */
}

/* the longjmp drops the C frames above run68k (lockstep follows the frames the native game is in) */
__attribute__((weak)) void run68k_unwind(void) {}

void restart_at(void (*entry)(void))
{
    if (!running) { fprintf(stderr, "restart_at: run68k is not running\n"); abort(); }
    run68k_unwind();
    next_entry = entry;
    longjmp(top, 1);
}
