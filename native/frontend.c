/* The front end is not ported yet: the routines that leave the game for it end the session.
 * Weak, so the real ports replace them. */
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "native.h"

static void no_front_end(const char *what)
{
    fprintf(stderr, "%s: the front end is not ported yet, the session ends here\n", what);
    native_sram_save("out/nhl94.srm");
    exit(0);
}

__attribute__((weak)) void ExitToOpening(void) { no_front_end("ExitToOpening"); }
__attribute__((weak)) void GameOver(void) { no_front_end("GameOver"); }
