/* goalie94.asm: the Manual Goalie menu item, followed by the arena and faceoff animations. */
#include "game.h"
#include "rom.h"
#include "symbols_gen.h"

/* goalie94.asm:ManualGoalieMenu. The "Manual Goalie" menu item: toggle the goalie mode of the
 * pause pad; in a penalty shot or shootout the pad then takes the skater or the goalie on its
 * side by that mode. */
void ManualGoalieMenu(void)
{
    if (g.OptNOP != 0) {
        if (g.cont1team == g.cont2team) {                 /* both pads on one team: both modes */
            g.goaliemode2 ^= 1;
            g.goaliemode1 ^= 1;
        } else if (g.sflags & HI(sfpj))
            g.goaliemode2 ^= 1;
        else
            g.goaliemode1 ^= 1;
    }
    if (!(g.gmode2 & HI(0)) && !(g.BA_PS_flags & HI(2))) return;
    if (!(g.sflags & HI(sfpj))) {
        if (g.OptNOP == 2) { if ((int16_t)g.BA_Sktr_SCnum > 5) return; }
        else if ((int16_t)g.BA_Sktr_SCnum <= 5) return;
        uint16_t d0 = g.OptNOP == 2 ? 6 : 0;
        if (g.goaliemode1 == 0) d0 = g.OptNOP == 2 ? 0xB : 5;
        setc1player(d0);
        return;
    }
    if (g.OptNOP == 2) { if ((int16_t)g.BA_Sktr_SCnum <= 5) return; }
    else if ((int16_t)g.BA_Sktr_SCnum > 5) return;
    setc2player(g.goaliemode2 != 0 ? 6 : 0xB);
}

/* goalie94.asm:RunArenaAnim. Run the arena animation arenaanim (negative: none): the first
 * time load its frame list and graphics from ArenaAnims, then count the frame time down by
 * the elapsed frames and step. */
void RunArenaAnim(void)
{
    if ((int16_t)g.arenaanim < 0) return;
    if (g.arenaspritelist == 0) {
        const uint8_t *e = rom_ptr(SYM_ArenaAnims) + g.arenaanim * 8;
        g.arenaframelist = rd32(e);
        g.arenaspritelist = rd32(e + 4);
        DecompressGraphicsPlain(rd32(e + 4) + 8, g.arenaanimchars);
        g.arenaframeidx = 0;
        NextArenaFrame();
        return;
    }
    g.arenaframetime = (uint16_t)(g.arenaframetime - elapsed);
    if ((int16_t)g.arenaframetime >= 0) return;
    g.arenaframeidx++;
    NextArenaFrame();
}

/* goalie94.asm:NextArenaFrame. Read frame arenaframeidx of the list: arenaframe = frame,
 * arenaframetime = time; frame $FF loops to the start, $FE ends the animation. */
void NextArenaFrame(void)
{
    for (;;) {
        const uint8_t *a0 = rom_ptr(g.arenaframelist) + g.arenaframeidx * 2;
        g.arenaframe = (uint16_t)(int8_t)a0[0];
        g.arenaframetime = (uint16_t)(int8_t)a0[1];
        if (g.arenaframe != 0xFFFF) break;
        g.arenaframeidx = 0;
    }
    if (g.arenaframe == 0xFFFE) EndArenaAnim();
}

/* goalie94.asm:SetCameraTop. Unreferenced. */
void SetCameraTop(void)
{
    g.sflags |= HI(sfslock);
    g.xc1 = 0;
    g.yc1 = 0x160;
}

/* goalie94.asm:StartArenaAnim. Start arena animation d0; RunArenaAnim loads it. */
void StartArenaAnim(uint16_t n)
{
    g.arenaanim = n;
    g.arenaframelist = 0;
    g.arenaspritelist = 0;
    g.arenaframe = 1;
    g.arenaframeidx = 0;
    ST_W(g.faceoffanim[0]);
}

/* goalie94.asm:SetFaceoffAnim. sflags7 bit 0, faceoffanim = d0. */
void SetFaceoffAnim(uint16_t n)
{
    g.sflags7 |= 1;
    g.faceoffanim[0] = n;
}

/* goalie94.asm:EndArenaAnim. arenaanim = -1, sflags7 bit 0 cleared. */
void EndArenaAnim(void)
{
    g.arenaanim = 0xFFFF;
    g.sflags7 &= (uint8_t)~1;
}
