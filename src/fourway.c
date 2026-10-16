/* fourway94.asm: four way play. */
#include "game.h"
#include "hal.h"

/* fourway94.asm:Set4WayPlayerStub. An empty Set4WayPlayer (an rts): forcepldata calls it with
 * SCnum $F when a goalie is pulled. */
void Set4WayPlayerStub(struct SortObj *o) {}

/* fourway94.asm:Detect4WayPlay. Look for EA's 4 Way Play adaptor on port 2 (FourWayPlay = 1
 * when found), with the Z80 held in reset. */
void Detect4WayPlay(void)
{
    z80_reset(0);
    pad_write(0xA10009, 0x40);                            /* port 1: TH an output */
    pad_write(0xA1000B, 0x43);
    pad_write(0xA10005, 0x7C);
    pad_write(0xA1000B, 0x7F);
    pad_write(0xA10005, 0x7C);
    if ((pad_read(0xA10003) & 3) == 0) {
        g.FourWayPlay = 1;
    } else {
        g.FourWayPlay = 0;
        pad_write(0xA1000B, 0x40);
    }
    z80_reset(0x100);
}
