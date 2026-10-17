/* The wide view (RULE_VIEW): the rink 320 pixels wide (40 columns) instead of the cartridge's
 * 256. What the game places for 256 (plane A's overlays and scoreboard, the sprites) moves
 * VIEW_SHIFT to the right, to the middle; the rink, 384 pixels wide, shows all but 64 of them,
 * so the camera moves VIEW_HPOS either way instead of $3C; and players count as off screen
 * VIEW_SHIFT further out. The pause screen (the window plane, which cannot scroll) is 256 wide as
 * on the cartridge, as are the screens away from the rink. A rule, not a setting of the window:
 * the computer's players and a pad's goalie go by what is on screen. */
#pragma once
#include "game.h"
#include "rules.h"
#include "symbols_gen.h"

#define VIEW_SHIFT 32
#define VIEW_HPOS 0x20

static inline int view_wide(void) { return rules[RULE_VIEW] != 0; }
/* the extra room each side, for what counts as on screen */
static inline int16_t view_more(void) { return view_wide() ? VIEW_SHIFT : 0; }
/* the camera's reach either way (Hpos) */
static inline int16_t view_hpos(void) { return view_wide() ? VIEW_HPOS : 0x3C; }
/* the shift of what is placed for 256: while the rink is up and not paused */
static inline int16_t view_shift(void)
{
    return view_wide() && (g.disflags & HI(df32c)) && g.vbint == SYM_VBlank && !(g.sflags & HI(sfpz)) ? VIEW_SHIFT : 0;
}
/* the rink's screen mode and plane A's scroll, after a setVram or a screen over the rink */
void view_rink_mode(void);
void view_rink_resume(void);    /* (a snapshot's rink, set up in 32 columns) */
