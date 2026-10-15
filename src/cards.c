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

/* cards94.asm:setSlotBit. sflags6 bit 5 (the slot) = the puck carrier is in the slot: in his
 * attacking zone (y past $58 toward the net he shoots at) within $47 of the middle. */
void setSlotBit(void)
{
    g.sflags6 &= (uint16_t)~HI(5);
    if ((int16_t)g.puckc < 0) return;
    const struct SortObj *o = sortobj((int16_t)g.puckc);
    int16_t y = o->Ypos;
    if (!(o->pflags & 0x80)) y = (int16_t)-y;
    if (y < 0x58 || o->Xpos > 0x47 || o->Xpos < -0x47) return;
    g.sflags6 |= HI(5);
}

/* cards94.asm:CountPlayers. homeplayers / awayplayers = the players of each team. */
void CountPlayers(void)
{
    g.homeplayers = GetPlayerCount(team_struct(0));
    g.awayplayers = GetPlayerCount(team_struct(1));
}

/* cards94.asm:PlayerRecordIO. The 4-byte record of roster slot d0 of team d1
 * (PlayerRecordOffsets) in save RAM from / to a0: written when sflags6 bit 6 is set. */
void PlayerRecordIO(uint32_t slot, uint16_t team, uint32_t buf)
{
    uint32_t at = slot << 2;
    at += (uint32_t)(int32_t)(int16_t)(rd16(rom_ptr(SYM_PlayerRecordOffsets) + (uint16_t)(team << 1)) << 2);
    if (g.sflags6 & HI(6)) WriteSRAM(at, 4, buf);
    else ReadSRAM(at, 4, buf);
}

/* cards94.asm:ReadPlayerRecord. */
void ReadPlayerRecord(uint32_t slot, uint16_t team, uint32_t buf) { g.sflags6 &= (uint16_t)~HI(6); PlayerRecordIO(slot, team, buf); }
/* cards94.asm:WritePlayerRecord. */
void WritePlayerRecord(uint32_t slot, uint16_t team, uint32_t buf) { g.sflags6 |= HI(6); PlayerRecordIO(slot, team, buf); }

/* cards94.asm:TeamRecordIO. Team (or user) d1's 16-byte record (TeamRecBase + d1 * 16) from /
 * to a0. */
void TeamRecordIO(uint32_t team, uint32_t buf)
{
    uint32_t at = (team & 0xFFFF0000u) | (uint16_t)(team << 4);
    at += SYM_TeamRecBase;
    if (g.sflags6 & HI(6)) WriteSRAM(at, 0x10, buf);
    else ReadSRAM(at, 0x10, buf);
}

/* cards94.asm:ReadTeamRecord. */
void ReadTeamRecord(uint32_t team, uint32_t buf) { g.sflags6 &= (uint16_t)~HI(6); TeamRecordIO(team, buf); }
/* cards94.asm:WriteTeamRecord. */
void WriteTeamRecord(uint32_t team, uint32_t buf) { g.sflags6 |= HI(6); TeamRecordIO(team, buf); }
/* cards94.asm:WriteCrowdRecord. */
void WriteCrowdRecord(uint32_t team, uint32_t buf) { g.sflags6 |= HI(6); CrowdRecordIO(team, buf); }

/* cards94.asm:UpdatePlayerRecords. After a game, team a2 (number d1, d5 goalies, opponent d6):
 * each roster slot's record keeps the best game (goals, or saves for the goalies) with the user
 * names and the opponent. a0 = a 4-byte buffer. */
void UpdatePlayerRecords(uint32_t team, uint32_t buf, const uint8_t *tm, uint16_t goalies, uint16_t opponent)
{
    uint8_t *rec = mem_ptr(buf);
    for (uint16_t slot = 0; slot < 0x1A; slot++) {
        ReadPlayerRecord(slot, (uint16_t)team, buf);
        uint8_t best = (int16_t)slot >= (int16_t)goalies ? tm[0xB4 + slot]
                                                         : (uint8_t)(tm[0xE8 + slot] - tm[0xB4 + slot]);   /* goalie: saves */
        if ((int8_t)best <= (int8_t)rec[0]) continue;
        rec[0] = best;
        int home = tm == team_struct(0);
        rec[1] = (uint8_t)(home ? g.homeuser : g.awayuser);
        rec[3] = (uint8_t)(home ? g.awayuser : g.homeuser);
        rec[2] = (uint8_t)opponent;
        WritePlayerRecord(slot, (uint16_t)team, buf);
    }
}

/* add one to a big-endian word kept as two bytes */
static void bump(uint8_t *p) { wr16(p, (uint16_t)(rd16(p) + 1)); }

/* cards94.asm:UpdateTeamRecord. After a game: record d1 (a user's) of team a1 against a2 (team
 * numbers d3 / d4, the other user d2): count the game (up to 9000), the win or the tie, and on
 * a win keep the most goals and the biggest margin (as the 68k computes it: word 0 of a2 minus
 * a2's goals) with who and against whom. a0 = a 16-byte buffer. */
void UpdateTeamRecord(uint32_t buf, uint32_t rec_no, uint16_t other, uint16_t team, uint16_t vs, const uint8_t *a1, const uint8_t *a2)
{
    uint8_t *rec = mem_ptr(buf);
    ReadTeamRecord(rec_no, buf);
    int16_t s1 = (int16_t)rd16(a1 + tmscore), s2 = (int16_t)rd16(a2 + tmscore);
    if ((int16_t)rd16(rec + 0xA) < 0x2328) {
        bump(rec + 0xA);
        if (s1 > s2) bump(rec + 8);
        else if (s1 == s2) bump(rec + 0xC);
    }
    if (s1 > s2) {
        if ((int8_t)s1 > (int8_t)rec[0]) { rec[0] = (uint8_t)s1; rec[1] = (uint8_t)team; rec[2] = (uint8_t)vs; rec[3] = (uint8_t)other; }
        int16_t margin = (int16_t)(rd16(a2) - s2);
        if ((int8_t)margin > (int8_t)rec[4]) { rec[4] = (uint8_t)margin; rec[5] = (uint8_t)team; rec[6] = (uint8_t)vs; rec[7] = (uint8_t)other; }
    }
    WriteTeamRecord(rec_no, buf);
}

/* cards94.asm:UpdateCrowdRecord. Team a1 (number d1, d6 goalies, opponent d2, users d4 / d5):
 * the crowd record keeps the most goals, the most saves by one goalie (worked out in the high
 * byte of TempWord1) and, for the home team, the loudest crowd (CrowdPeak); written when
 * anything changed. a0 = a 16-byte buffer. */
void UpdateCrowdRecord(uint32_t buf, uint32_t team, uint16_t opp, uint16_t user, uint16_t opp_user, uint16_t goalies, const uint8_t *tm)
{
    uint8_t *rec = mem_ptr(buf);
    int changed = 0;
    ReadCrowdRecord(team, buf);
    uint8_t goals = (uint8_t)rd16(tm + tmscore);
    if ((int8_t)goals > (int8_t)rec[0]) {
        rec[0] = goals; rec[1] = (uint8_t)user; rec[3] = (uint8_t)opp_user; rec[2] = (uint8_t)opp;
        changed = 1;
    }
    uint8_t saves = 0;
    uint16_t n = (uint16_t)(goalies - 1);
    do {
        uint8_t s = (uint8_t)(tm[0xE8 + n] - tm[0xB4 + n]);
        g.TempWord1 = (uint16_t)((s << 8) | (g.TempWord1 & 0xFF));
        if ((int8_t)saves < (int8_t)s) saves = s;
    } while (n-- != 0);
    if ((int8_t)saves > (int8_t)rec[4]) {
        changed = 1;
        rec[4] = saves; rec[5] = (uint8_t)user; rec[7] = (uint8_t)opp_user; rec[6] = (uint8_t)opp;
    }
    if ((uint16_t)team == g.HomeTeam && (int16_t)rec[8] < (int16_t)g.CrowdPeak) {
        changed = 1;
        rec[8] = (uint8_t)g.CrowdPeak; rec[9] = (uint8_t)user; rec[0xB] = (uint8_t)opp_user; rec[0xA] = (uint8_t)opp;
    }
    if (changed) WriteCrowdRecord(team, buf);
}

/* cards94.asm:UpdateRecords. After a game, with save RAM and user records on: the player, crowd
 * and team records of both teams (ThreeStars is the buffer), then the save RAM checksum. */
void UpdateRecords(void)
{
    if ((int16_t)g.ValidSRAM < 0 || g.OptUserRec != 0) return;
    CountGoalies();
    CountPlayers();
    uint32_t buf = RAM_BASE + RAM_OFF_ThreeStars;
    UpdatePlayerRecords(g.HomeTeam, buf, team_struct(0), g.homegoalies, g.VisTeam);
    UpdatePlayerRecords(g.VisTeam, buf, team_struct(1), g.awaygoalies, g.HomeTeam);
    UpdateCrowdRecord(buf, g.HomeTeam, g.VisTeam, g.homeuser, g.awayuser, g.homegoalies, team_struct(0));
    UpdateCrowdRecord(buf, g.VisTeam, g.HomeTeam, g.awayuser, g.homeuser, g.awaygoalies, team_struct(1));
    UpdateTeamRecord(buf, g.homeuser, g.awayuser, g.HomeTeam, g.VisTeam, team_struct(0), team_struct(1));
    UpdateTeamRecord(buf, g.awayuser, g.homeuser, g.VisTeam, g.HomeTeam, team_struct(1), team_struct(0));
    MakeSRAMChecksum();
}
