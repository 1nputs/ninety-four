/* The parts of the front end that are not ported yet end the session when the game reaches
 * them. Weak, so the real ports replace them. */
#include <stdio.h>
#include "game.h"
#include "native.h"

static void no_front_end(const char *what)
{
    fprintf(stderr, "%s: not ported yet, the session ends here\n", what);
    native_quit();
}

__attribute__((weak)) void GameOver(void) { no_front_end("GameOver"); }
