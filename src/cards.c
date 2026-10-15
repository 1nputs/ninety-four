/* cards94.asm: player cards and the save RAM records. Only the parts the game play calls are
 * here so far; the card and record screens come with the front end. */
#include "game.h"
#include "symbols_gen.h"

/* cards94.asm:wallcollduringcheck. During a check, skater a2 (not a goalie) is tested against
 * the corners and walls at his position with his size + $17. */
void wallcollduringcheck(struct SortObj *a2, struct SortObj *a3)
{
    if (a2->position == 0) return;
    g.wcradiusx = (uint16_t)(a2->radiusx + 0x17);
    g.wcradiusy = (uint16_t)(a2->radiusy + 0x17);
    checkcornercoll(a2->Xpos, a2->Ypos, a2);
}

/* cards94.asm:CountGoalies. homegoalies / awaygoalies = the goalies of each team. */
void CountGoalies(void)
{
    g.homegoalies = GetGoalieCount(team_struct(0));
    g.awaygoalies = GetGoalieCount(team_struct(1));
}

/* cards94.asm:CrowdRecordIO. Team d1's 16-byte crowd record (CrowdRecBase + team * 16) in save
 * RAM from / to a0: written when sflags6 bit 6 is set, else read. */
void CrowdRecordIO(uint32_t team, uint32_t buf)
{
    uint32_t at = (team & 0xFFFF0000u) | (uint16_t)(team << 4);   /* asl.w #4 of the long */
    at += SYM_CrowdRecBase;
    if (g.sflags6 & HI(6)) WriteSRAM(at, 0x10, buf);
    else ReadSRAM(at, 0x10, buf);
}

/* cards94.asm:ReadCrowdRecord. Read team d1's crowd record to a0. */
void ReadCrowdRecord(uint32_t team, uint32_t buf)
{
    g.sflags6 &= (uint16_t)~HI(6);
    CrowdRecordIO(team, buf);
}
