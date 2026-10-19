/* cards94.asm: player cards and the save RAM records: the setup screen's cards, the Player Cards
 * menu screen, the user records read from and written to save RAM. Complete (CardStub, an rts
 * nothing calls, aside). */
#include <string.h>
#include "game.h"
#include "symbols_gen.h"
#include "hal.h"
#include "cards_strings_gen.h"

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
    uint32_t at = (team & 0xFFFF0000u) + league_crowd_record((uint16_t)team);   /* (asl.w #4 of the long; the league's place) */
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
    if ((int16_t)puck_carrier() < 0) return;
    const struct SortObj *o = sortobj((int16_t)puck_carrier());
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
    at += (uint32_t)(int32_t)(int16_t)(rd16(rom_ptr(LG(PlayerRecordOffsets)) + (uint16_t)(team << 1)) << 2);
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
void UpdatePlayerRecords(uint32_t team, uint32_t buf, struct Team *tm, uint16_t goalies, uint16_t opponent)
{
    uint8_t *rec = ram_bytes(ram_at(buf), 4);
    for (uint16_t slot = 0; slot < 0x1A; slot++) {
        ReadPlayerRecord(slot, (uint16_t)team, buf);
        uint8_t best = (int16_t)slot >= (int16_t)goalies ? tm->pgoals[slot]
                                                         : (uint8_t)(tm->pshots[slot] - tm->pgoals[slot]);                 /* goalie: saves */
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
void UpdateTeamRecord(uint32_t buf, uint32_t rec_no, uint16_t other, uint16_t team, uint16_t vs, struct Team *a1, struct Team *a2)
{
    uint8_t *rec = ram_bytes(ram_at(buf), 0x10);
    ReadTeamRecord(rec_no, buf);
    int16_t s1 = (int16_t)a1->score, s2 = (int16_t)a2->score;
    if ((int16_t)rd16(rec + 0xA) < 0x2328) {
        bump((rec + 0xA));
        if (s1 > s2) bump((rec + 8));
        else if (s1 == s2) bump((rec + 0xC));
    }
    if (s1 > s2) {
        if ((int8_t)s1 > (int8_t)rec[0]) { rec[0] = (uint8_t)s1; rec[1] = (uint8_t)team; rec[2] = (uint8_t)vs; rec[3] = (uint8_t)other; }
        int16_t margin = (int16_t)(a2->shots - s2);
        if ((int8_t)margin > (int8_t)rec[4]) { rec[4] = (uint8_t)margin; rec[5] = (uint8_t)team; rec[6] = (uint8_t)vs; rec[7] = (uint8_t)other; }
    }
    WriteTeamRecord(rec_no, buf);
}

/* cards94.asm:UpdateCrowdRecord. Team a1 (number d1, d6 goalies, opponent d2, users d4 / d5):
 * the crowd record keeps the most goals, the most saves by one goalie (worked out in the high
 * byte of TempWord1) and, for the home team, the loudest crowd (CrowdPeak); written when
 * anything changed. a0 = a 16-byte buffer. */
void UpdateCrowdRecord(uint32_t buf, uint32_t team, uint16_t opp, uint16_t user, uint16_t opp_user, uint16_t goalies, struct Team *tm)
{
    uint8_t *rec = ram_bytes(ram_at(buf), 0x10);
    int changed = 0;
    ReadCrowdRecord(team, buf);
    uint8_t goals = (uint8_t)tm->score;
    if ((int8_t)goals > (int8_t)rec[0]) {
        rec[0] = goals; rec[1] = (uint8_t)user; rec[3] = (uint8_t)opp_user; rec[2] = (uint8_t)opp;
        changed = 1;
    }
    uint8_t saves = 0;
    uint16_t n = (uint16_t)(goalies - 1);
    do {
        uint8_t s = (uint8_t)(tm->pshots[n] - tm->pgoals[n]);
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
    uint32_t buf = ram_addr(g.ThreeStars);
    UpdatePlayerRecords(g.HomeTeam, buf, team_struct(0), g.homegoalies, g.VisTeam);
    UpdatePlayerRecords(g.VisTeam, buf, team_struct(1), g.awaygoalies, g.HomeTeam);
    UpdateCrowdRecord(buf, g.HomeTeam, g.VisTeam, g.homeuser, g.awayuser, g.homegoalies, team_struct(0));
    UpdateCrowdRecord(buf, g.VisTeam, g.HomeTeam, g.awayuser, g.homeuser, g.awaygoalies, team_struct(1));
    UpdateTeamRecord(buf, g.homeuser, g.awayuser, g.HomeTeam, g.VisTeam, team_struct(0), team_struct(1));
    UpdateTeamRecord(buf, g.awayuser, g.homeuser, g.VisTeam, g.HomeTeam, team_struct(1), team_struct(0));
    MakeSRAMChecksum();
}

/* ---- the player cards of the setup screen --------------------------------------------------- */

#define CARD(n) HI(n)                                    /* setupcardflags bits (see optsetup.c) */
#define MESAREA (ram_addr(g.mesarea))
#define TEMPBUF (ram_addr(g.TempBuffer))

/* cards94.asm:ResetCardTimer. The player card timer to $AA ($53 when C was pressed). */
void ResetCardTimer(void)
{
    g.cardtimer = (g.setupcardflags & CARD(5)) ? 0x53 : 0xAA;
}

/* cards94.asm:PlayerCardTimer. Each frame of the setup screen's first minute: once carddelay has
 * run out a card is up (bit 0); count cardtimer down (C skips $50 of it); at $52 the next card,
 * on the other side, with both blocks and logos forgotten (PlayerCardScreen); below 0 the timer
 * starts again. */
void PlayerCardTimer(void)
{
    if ((int16_t)--g.carddelay >= 0) return;
    g.carddelay = 0xFFFF;
    g.setupcardflags |= CARD(0);
    if ((g.setupcardflags & CARD(5)) && (int16_t)g.cardtimer < 0x52) g.cardtimer -= 0x50;
    if ((int16_t)--g.cardtimer < 0) { ResetCardTimer(); return; }
    if (g.cardtimer != 0x52) return;
    g.setupcardflags &= (uint16_t)~(CARD(2) | CARD(3));
    g.setupcardflags ^= CARD(1);
    ST_W(g.setuphome);
    ST_W(g.setupvis);
    g.setupcardflags &= (uint16_t)~(CARD(6) | CARD(7));
    PlayerCardScreen();
}

/* printsmall in the setup font (sflags6 bit 3) */
static void print_setup(uint32_t s)
{
    g.sflags6 |= HI(3);
    printsmall(s);
    g.sflags6 &= (uint16_t)~HI(3);
}

/* cards94.asm:PlayerCardScreen. A player card on one side (bit 1: the visitors' on the left,
 * else the home team's on the right): the team block erased, a logo box with the picture of the
 * team's featured player (FeaturedPictures; the next one after each visitors' card), a framed
 * box with his number and name and, with save RAM, his user records. */
void PlayerCardScreen(void)
{
    int vis = (g.setupcardflags & CARD(1)) != 0;
    printz(str_PlayerCardScreen_1, sizeof str_PlayerCardScreen_1);
    g.printx = vis ? 0x1C : 3;
    g.printy = 5;
    eraser(8, 8, 0x7FF);
    printz(str_PlayerCardScreen_2, sizeof str_PlayerCardScreen_2);
    g.printx = vis ? 3 : 0xB;
    g.printy = 5;
    eraser(0x19, 8, 0x7FF);
    if (!vis) {
        LogoBoxRight();
    } else {
        LogoBoxLeft();
        g.featuredplayer++;
    }
    printz(str_PlayerCardScreen_3, sizeof str_PlayerCardScreen_3);
    if ((int16_t)g.featuredplayer >= 6) g.featuredplayer = 0;
    uint16_t team = vis ? g.VisTeam : g.HomeTeam;
    g.printx = vis ? 4 : 0x1D;
    g.printy = 6;
    uint16_t chars = g.homepicchars, d3 = (uint16_t)(g.featuredplayer * 6);
    uint32_t list = rd32(rom_ptr(LG(FeaturedPictures) + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    g.cardroster = rd16(rom_ptr(list + (int16_t)d3 + 4));
    uint32_t pic = rd32(rom_ptr(list + (int16_t)d3));
    uint32_t pal = ART(PicturePalette) + rd32(rom_ptr(ART(PicturePalette))), map;
    if (rd32(rom_ptr(pic + 4)) == 0) map = ART(PicturePalette) + rd32(rom_ptr(ART(PicturePalette) + 4));   /* no map: the common one */
    else map = pic + rd32(rom_ptr(pic + 4));
    UnpackPicture(pic + 8);
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    uint16_t keep[4];                                     /* (colours $11-$14 kept) */
    for (int i = 0; i < 4; i++) keep[i] = g.palfadenew[0x11 + i];
    dobitmap(pal, map, ram_addr(g.picturebuf), 0, 0, 6, 6, chars, 2);
    for (int i = 0; i < 4; i++) g.palfadenew[0x11 + i] = keep[i];
    printz(str_PlayerCardScreen_4, sizeof str_PlayerCardScreen_4);
    g.printa = 0x6000;
    g.printx = vis ? 0xB : 3;
    g.printy = 5;
    g.cardprintx = (uint16_t)(g.printx + 1);
    g.cardprinty = (uint16_t)(g.printy + 1);
    Framer(0x19, 8);                                      /* 25 x 8 */
    g.printx = g.cardprintx;
    g.printy = g.cardprinty;
    g.printa = 0;
    g.printm = 0;
    team = vis ? g.VisTeam : g.HomeTeam;
    g.cardteamnum = team;
    uint32_t a2 = rd32(rom_ptr(LG(TeamList) + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    a2 += (uint32_t)(int16_t)rd16(rom_ptr(a2));          /* the roster: name String, then 8 bytes */
    for (uint16_t n = g.cardroster; n-- != 0;) a2 += (uint32_t)(int16_t)rd16(rom_ptr(a2)) + 8;
    uint8_t *mes = g.mesarea;
    wr16(mes, 6);
    ConvertByteToDigits((uint16_t)(int16_t)(int8_t)*rom_ptr(a2 + (int16_t)rd16(rom_ptr(a2))), mes + 2);          /* the uniform number */
    wr16(mes + 4, 0x2000);
    appstring(a2, MESAREA);                               /* and the name */
    print_setup(MESAREA);
    if ((int16_t)g.ValidSRAM >= 0) {                      /* the user records */
        PrintRecordValue(MESAREA, g.cardteamnum, g.cardroster);
        if (rd16(mes) != 2) {
            g.printx = g.cardprintx;
            g.printy += 2;
            print_setup(MESAREA);
            PrintRecordHolder(MESAREA, g.cardteamnum, g.cardroster, RAM_BASE);
            g.printx = g.cardprintx;
            if (rd16(mes) != 2) {
                g.printy++;
                print_setup(MESAREA);
            }
            PrintRecordVs(MESAREA, g.cardteamnum, g.cardroster, RAM_BASE);
            g.printx = g.cardprintx;
            g.printy++;
            print_setup(MESAREA);
        }
    }
    g.palcount = 0x64;
}

/* cards94.asm:PrintRecordValue. The record line of player d1 of team d0 at a1: "Record " (a copy
 * of RecordTxt), the value (GetRecordValue) and goals / goal or saves / save; an empty String
 * when there is no value. */
void PrintRecordValue(uint32_t buf, uint16_t team, uint16_t player)
{
    CopyString(SYM_RecordTxt, buf);
    uint16_t goalie;
    uint16_t v = GetRecordValue(team, player, &goalie);
    if (v == 0) { wr16(ram_bytes(ram_at(buf), 2), 2); return; }
    g.TempWord2 = v;
    AppendNumber(v, TEMPBUF);
    appstring(TEMPBUF, buf);
    v = g.TempWord2;
    uint32_t a1 = v == 1 ? SYM_RecSaveTxt : SYM_RecSavesTxt;
    if (!goalie) a1 = v == 1 ? SYM_RecGoalTxt : SYM_RecGoalsTxt;
    appstring(a1, buf);
}

/* cards94.asm:CopyString. Copy String a1 (its length word first) to a3. */
void CopyString(uint32_t src, uint32_t dst)
{
    uint32_t s = src;
    uint16_t n = (uint16_t)(m16(s) - 1);
    uint8_t *d = ram_bytes(ram_at(dst), (uint16_t)(n + 1));
    do *d++ = m8(s++); while (n-- != 0);
}

/* cards94.asm:AppendNumber. d0 (0-999, no leading zeros) as a String at a1, padded to an even
 * length. */
void AppendNumber(uint16_t n, uint32_t buf)
{
    uint8_t *start = ram_bytes(ram_at(buf), 6), *p = start + 2;
    uint16_t len = 0;
    uint32_t d0 = m68k_divu((uint32_t)(int32_t)(int16_t)n, 100);
    if ((uint16_t)d0 != 0) { *p++ = (uint8_t)((uint16_t)d0 + '0'); len++; }
    d0 = m68k_divu((uint32_t)(int32_t)(int16_t)(d0 >> 16), 10);
    uint8_t tens = (uint8_t)((uint16_t)d0 + '0');
    if (tens != '0' || (int16_t)n >= 10) { *p++ = tens; len++; }
    *p++ = (uint8_t)((uint16_t)(d0 >> 16) + '0');
    len++;
    if (len & 1) { *p++ = 0; len++; }
    wr16(start, (uint16_t)(len + 2));
}

/* cards94.asm:GetRecordValue. Player d1 of team d0's record from save RAM (ReadPlayerRecord to
 * $FFFF0000) -> d0 = its value byte, d1 = 1 when he is one of the team's goalies (the first
 * entries, as many as the nibbles of its .sodds word). */
uint16_t GetRecordValue(uint16_t team, uint16_t player, uint16_t *goalie)
{
    ReadPlayerRecord((uint32_t)(int32_t)(int16_t)player, team, RAM_BASE);
    uint16_t v = (uint16_t)(int16_t)(int8_t)m8(RAM_BASE);
    uint32_t blk = rd32(rom_ptr(LG(TeamList) + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    uint16_t odds = rd16(rom_ptr(blk + (int16_t)rd16(rom_ptr(blk + 0xA))));
    int16_t goalies = 0;
    do { goalies++; odds = (uint16_t)(odds << 4); } while (odds != 0);
    *goalie = goalies > (int16_t)player;
    return v;
}

/* cards94.asm:PrintRecordHolder. "by " (RecByTxt) and the record holder's user name
 * (AppendRecordHolder) at a1; an empty String when there is none. */
void PrintRecordHolder(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec)
{
    CopyString(SYM_RecByTxt, buf);
    AppendRecordHolder(TEMPBUF, team, player, rec);
    if (rd16(g.TempBuffer) == 2) wr16(ram_bytes(ram_at(buf), 2), 2);
    else appstring(TEMPBUF, buf);
}

/* cards94.asm:PrintRecordVs. "vs. " (RecVsTxt), the user name and the opponent (AppendRecordVs)
 * at a1. */
void PrintRecordVs(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec)
{
    CopyString(SYM_RecVsTxt, buf);
    AppendRecordVs(TEMPBUF, team, player, rec);
    appstring(TEMPBUF, buf);
}

/* the player record a0, or (a0 = 0) player d1 of team d0's read to $FFFF0000 */
static uint32_t player_record(uint16_t team, uint16_t player, uint32_t rec)
{
    if (rec == 0) {
        ReadPlayerRecord((uint32_t)(int32_t)(int16_t)player, (uint16_t)(int16_t)team, RAM_BASE);
        rec = RAM_BASE;
    }
    return rec;
}

/* cards94.asm:AppendRecordHolder. The user name of the record holder (byte 1 of record a0) as a
 * String at a1 (AppendUserName, trimmed). */
void AppendRecordHolder(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec)
{
    uint32_t r = player_record(team, player, rec);
    g.sflags6 |= HI(7);
    AppendUserName(buf, (uint16_t)(int16_t)(int8_t)m8(r + 1));
}

/* cards94.asm:AppendRecordVs. The user name (byte 3 of record a0), a space and the opponent's
 * city (byte 2, AppendTeamName) as a String at a1. */
void AppendRecordVs(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec)
{
    uint32_t r = player_record(team, player, rec);
    g.sflags6 |= HI(7);
    AppendUserName(buf, (uint16_t)(int16_t)(int8_t)m8(r + 3));
    appstring(SYM_RecSpaceTxt, buf);
    AppendTeamName(m8(r + 2), buf);
}

/* cards94.asm:WriteNameLog. The user name log to save RAM (NameLogIO). */
void WriteNameLog(void)
{
    g.sflags6 |= HI(6);
    NameLogIO();
}

/* cards94.asm:ReadNameLog. The user name log from save RAM to namelog (NameLogIO). */
void ReadNameLog(void)
{
    g.sflags6 &= (uint16_t)~HI(6);
    NameLogIO();
}

/* cards94.asm:NameLogIO. Read (sflags6 bit 6 clear) or write the name log: NameLogBytes at save
 * RAM NameLogBase. */
void NameLogIO(void)
{
    if (g.sflags6 & HI(6)) WriteSRAM(SYM_NameLogBase, SYM_NameLogBytes, ram_addr(g.namelog));
    else ReadSRAM(SYM_NameLogBase, SYM_NameLogBytes, ram_addr(g.namelog));
}

/* cards94.asm:AppendUserName. User d2's name (12 bytes of namelog, a 0 as a space) as a String at
 * a1, trimmed (TrimSpaces) with sflags6 bit 7; user 0: an empty String (NoNameTxt). */
void AppendUserName(uint32_t buf, uint16_t user)
{
    uint8_t *b = ram_bytes(ram_at(buf), 0xE);
    if (user == 0) {
        CopyString(SYM_NoNameTxt, buf);
        TrimSpaces(buf);
        return;
    }
    uint32_t name = RAM_BASE + RAM_OFF_namelog + (uint32_t)user * 0xC;                  /* (mulu.w: any byte) */
    for (int i = 0; i < 12; i++) b[2 + i] = m8(name + i) ? m8(name + i) : ' ';
    wr16(b, 0xE);
    if (g.sflags6 & HI(7)) TrimSpaces(buf);
}

/* cards94.asm:AppendTeamName. Append team d0's city (its TeamList block String) to a1. */
void AppendTeamName(uint16_t team, uint32_t buf)
{
    uint32_t blk = rd32(rom_ptr(LG(TeamList) + (uint32_t)(int16_t)(uint16_t)((int16_t)(int8_t)team << 2)));
    blk += (uint32_t)(int32_t)(int16_t)rd16(rom_ptr(blk + 4));
    blk += (uint32_t)(int16_t)rd16(rom_ptr(blk));
    appstring(blk, buf);
}

/* cards94.asm:TrimSpaces. Drop the trailing spaces of String a2 (zeroed) and pad its length to
 * even. */
void TrimSpaces(uint32_t str)
{
    uint8_t *s = ram_bytes(ram_at(str), m16(str)), *p = s + rd16(s);
    while (*--p == ' ') {
        *p = 0;
        wr16(s, (uint16_t)(rd16(s) - 1));
    }
    wr16(s, (uint16_t)((rd16(s) + 1) & 0xFE));
}

/* ---- the matchups' pictures and ratings (scouting report) ------------------------------------ */

/* cards94.asm:DrawPlayerPicture. -> a0 = the picture of player d0 of team d1: his from the team's
 * FeaturedPictures list, else a stock one (NoPicGoalie1 / 2, NoPicSkater1 / 2 by the bit 0 of his
 * fifth attribute byte; the first as many players as the team's .sodds word has nibbles are its
 * goalies). */
uint32_t DrawPlayerPicture(uint16_t player, uint16_t team)
{
    uint32_t list = rd32(rom_ptr(LG(FeaturedPictures) + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    for (; rd32(rom_ptr(list)) != 0; list += 6)
        if (rd16(rom_ptr(list + 4)) == player) return rd32(rom_ptr(list));
    uint32_t data = rd32(rom_ptr(LG(TeamList) + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    uint32_t a6 = data + (uint32_t)(int16_t)rd16(rom_ptr(data));
    for (uint16_t n = player; n-- != 0;) a6 += (uint32_t)(int16_t)rd16(rom_ptr(a6)) + 8;
    a6 += (uint32_t)(int16_t)rd16(rom_ptr(a6));           /* past his name: his attributes */
    uint16_t odds = rd16(rom_ptr(data + (int16_t)rd16(rom_ptr(data + 0xA)))), goalies = 0;
    do { goalies++; odds = (uint16_t)(odds << 4); } while (odds != 0);
    int pic1 = rom_ptr(a6)[4] & 1;
    if ((int16_t)player < (int16_t)goalies) return pic1 ? ART(NoPicGoalie1) : ART(NoPicGoalie2);
    return pic1 ? ART(NoPicSkater2) : ART(NoPicSkater1);
}

/* cards94.asm:CalcAttrib. The rating of player d0 of team a2 for the attributes set in the high
 * word of d4 (bit 15 down to 0, a nibble each, from the end of his 8 attribute bytes back): each
 * nibble times its weight / 2 (OvrPlayerWgtList / OvrGoalWgtList for the overall masks, else
 * AttribWgtList; the Wgt and Fight nibbles as they are). The overall ratings add the average of
 * his hot / cold bytes ($1A2 of the team struct: OvrDivisor / OvrDividend); the others scale each
 * nibble by 18 and add its hot / cold byte (a negative low byte cleared) -> d0 = the sum, d1 =
 * 100 a nibble counted (100 for the overall ones); d0 at or past d1 becomes d1 - 1. */
uint16_t CalcAttrib(uint16_t player, struct Team *tm, uint32_t mask, uint16_t *div)
{
    g.OvrDivisor = 0;
    g.OvrDividend = 0;
    uint32_t weights = SYM_AttribWgtList;
    if (mask == rd32(rom_ptr(SYM_PAttribOverallMask))) weights = SYM_OvrPlayerWgtList;
    if (mask == rd32(rom_ptr(SYM_GAttribOverallMask))) weights = SYM_OvrGoalWgtList;
    uint32_t a0 = tm->data;
    const uint8_t *hotcold = tm->hotcold[player] + 0x10;                        /* the end of his hot / cold bytes */
    a0 += (uint32_t)(int16_t)rd16(rom_ptr(a0));
    for (uint16_t n = player;; n--) {                     /* past his name and attributes */
        a0 += (uint32_t)(int16_t)rd16(rom_ptr(a0)) + 8;
        if (n == 0) break;
    }
    const uint8_t *attrs = rom_ptr(a0);
    uint16_t d0 = 0, d1 = 0, bits = (uint16_t)(mask >> 16);
    for (int16_t d2 = 0xF; d2 >= 0; d2--) {
        if (!(bits & (1u << d2))) continue;
        uint16_t d3 = attrs[-(d2 >> 1) - 1];
        if (d2 & 1) d3 >>= 4;
        d3 &= 0xF;
        if (d2 != 0xD && d2 != 6) {
            int16_t w = (int8_t)rom_ptr(weights)[d2];
            if (w != 2) d3 = (uint16_t)((int16_t)(uint16_t)(d3 * (uint16_t)w) >> 1);   /* (the 68k adds it w - 1 more times, mod 2^16) */
            if (weights != SYM_AttribWgtList) {
                g.OvrDivisor += (uint16_t)(int8_t)hotcold[-d2 - 1];
                g.OvrDividend++;
            } else {
                d3 = (uint16_t)(d3 * 18);
                uint8_t lo = (uint8_t)(d3 + hotcold[-d2 - 1]);
                if (lo & 0x80) lo = 0;
                d3 = (uint16_t)((d3 & 0xFF00) | lo);
            }
        }
        d0 = (uint16_t)(d0 + d3);
        d1 = (uint16_t)(d1 + 100);
    }
    if (weights != SYM_AttribWgtList) {
        d1 = 100;
        d0 = (uint16_t)(d0 + (uint16_t)m68k_divs((int32_t)(int16_t)g.OvrDivisor, (int16_t)g.OvrDividend));
    }
    if ((int16_t)d0 >= (int16_t)d1) {
        d1 = d0;
        d0--;
    }
    *div = d1;
    return d0;
}

/* ---- the Player Cards menu item ------------------------------------------------------------------ */

/* the card's team: the visitors when cardvis is set */
static uint16_t card_team(void) { return g.cardvis ? g.VisTeam : g.HomeTeam; }

/* cards94.asm:GetCardTeam. a0 = the card's team struct. */
struct Team *GetCardTeam(void) { return team_struct(g.cardvis != 0); }

/* cards94.asm:PlayerCards. The Player Cards (a menu item, with save RAM only): 40 columns, the
 * card screen picture, the corner logo and the arena, the card orders (BuildCardPlayerLists),
 * then the cards of the team of the pad that paused (DrawPlayerCard): left / right and C page,
 * A the other team, Start leaves. Card 0 is the team's records. */
void PlayerCards(void)
{
    static const uint8_t remap[8] = { 0x05, 0x23, 0x41, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    if ((int16_t)g.ValidSRAM < 0) return;
    forceblack();
    g.disflags &= (uint16_t)~HI(dfok);
    uint16_t saved = g.disflags;
    g.disflags |= HI(dfng);
    Vmaddr(g.VSPRITES);
    vdp_data32(0);
    vdp_ctrl16(SYM_VDPR_MODE4 + 0x81);
    g.Map3col1 = 6;
    vdp_ctrl16(SYM_VDPR_HSCROLL);
    Vmaddr(0);
    vdp_data32(0);
    g.disflags = saved;
    g.disflags &= (uint16_t)~HI(df32c);
    CountGoalies();
    CountPlayers();
    DecompressGraphicsWithCallback(ART(SmallFontMap) + 8, g.smallfontchars, rom_find(SYM_PlayerCards, SYM_PlayerCards + 0x100, remap, 8));
    printz(str_PlayerCards_1, sizeof str_PlayerCards_1);
    uint32_t pic = ART(PlayerCardsScreenPic);
    uint16_t c = dobitmap(pic + rd32(rom_ptr(pic)), pic + rd32(rom_ptr(pic + 4)), pic + 8, 0, 0, 0x28, 0x1C, 1, 0xD);
    g.cardlogochars1 = c;
    c = (uint16_t)(c + 0x24);
    g.cardlogochars2 = c;
    c = (uint16_t)(c + 0x24);
    printz(str_PlayerCards_2, sizeof str_PlayerCards_2);
    eraser(0x40, 0x20, 0x7FF);
    printz(str_PlayerCards_3, sizeof str_PlayerCards_3);
    pic = ART(CornerLogoMap);
    c = dobitmap(pic + rd32(rom_ptr(pic)), pic + rd32(rom_ptr(pic + 4)), pic + 8, 0, 0, 6, 6, c, 8);
    printz(str_PlayerCards_4, sizeof str_PlayerCards_4);
    pic = ART(ArenaGfxBank);
    dobitmap(pic + rd32(rom_ptr(pic)), pic + rd32(rom_ptr(pic + 4)), pic + 8, 0, 0, 6, 6, c, 0);
    BuildCardPlayerLists();
    g.palcount = 0x18;
    g.homecardidx = 0;
    g.awaycardidx = 0;
    int second = g.FourWayPlay ? g.pausepad != 3 : (g.sflags & HI(sfpj)) != 0;
    int16_t side = (int16_t)((second ? g.cont2team : g.cont1team) - 1);
    if (side < 0) side = 0;
    g.cardvis = (uint16_t)(side & 1);
    DrawPlayerCard(g.cardvis ? RAM_OFF_awaycardidx : RAM_OFF_homecardidx);
    for (;;) {
        vcountwait();
        uint16_t d0, d1, d2, d3;
        getpzjoy(&d0, &d1, &d2, &d3);
        ProcessInputWithRepeat(&d1, d2, d3);
        if (d3 & 0x80) break;
        int16_t by;
        if (d1 & 0x40) {
            g.cardvis ^= 1;
            by = 0;
        } else if (d1 & 0x20) {
            by = 1;
        } else {
            nodiag(&d1, d3);
            if (d1 & 8) by = 1;
            else if (d1 & 4) by = -1;
            else continue;
        }
        uint16_t idx = RAM_OFF_homecardidx;
        g.cardcount = g.homeplayers;
        if (g.cardvis) {
            idx = RAM_OFF_awaycardidx;
            g.cardcount = g.awayplayers;
        }
        w16(ram_at(RAM_BASE + idx), 0, (uint16_t)(m16(RAM_BASE + idx) + by));
        WrapCardIndex(idx);
        DrawPlayerCard(idx);
    }
    ExitStatsScreen();
}

/* cards94.asm:WrapCardIndex. Card index (a0) wrapped into 0 ... cardcount. */
void WrapCardIndex(uint16_t idx)
{
    rp p = ram_at(RAM_BASE + idx);
    uint16_t n = (uint16_t)(g.cardcount + 1);
    if (r16(p, 0) == n) w16(p, 0, 0);
    if ((int16_t)r16(p, 0) < 0) w16(p, 0, (uint16_t)(n - 1));
}

/* cards94.asm:DrawPlayerCard. The card of index (a0) in the team's colours: card 0 the team's
 * records (PrintCardRecords); else the player's picture, his first and last names, his team, his
 * records and this game's goals and assists or saves, his overall rating and his number and
 * position. */
void DrawPlayerCard(uint16_t idx)
{
    uint32_t pal = LG(TeamPalettes) + (uint32_t)(int16_t)(uint16_t)(card_team() << 6) + 0x26;
    pal_load(0x11, rom_ptr(pal), 2);
    g.palcount = 0x64;
    printz(str_DrawPlayerCard_1, sizeof str_DrawPlayerCard_1);
    eraser(0x28, 0x15, 0x7FF);
    printz(str_DrawPlayerCard_2, sizeof str_DrawPlayerCard_2);
    if (m16(RAM_BASE + idx) == 0) {
        PrintCardRecords();
        return;
    }
    g.cardindex = m16(RAM_BASE + idx);
    DrawPictureBox();
    ClearCardText(0x21, 8);
    g.palcount = 0x64;
    printz(str_DrawPlayerCard_3, sizeof str_DrawPlayerCard_3);
    uint16_t team = card_team();
    g.TempWord2 = team;
    uint32_t a2 = rd32(rom_ptr(LG(TeamList) + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    a2 += (uint32_t)(int16_t)rd16(rom_ptr(a2));
    for (uint16_t n = GetCardPlayer(); n-- != 0;) a2 += (uint32_t)(int16_t)rd16(rom_ptr(a2)) + 8;
    uint8_t *ts = g.ThreeStars;
    CopyString(a2, ram_addr(ts));                         /* his first name */
    uint16_t len = 0;
    while (ts[2 + len] != ' ') len++;
    ts[2 + len] = 0;
    wr16(ts, (uint16_t)(((len + 1) & 0xFE) + 2));
    g.printx = 8;
    g.printy = 0xB;
    printsmall(ram_addr(ts));
    uint16_t player = GetCardPlayer();
    uint32_t a1 = FormatPlayerNameLast(player, GetCardTeam());
    g.printx = 8;
    g.printy = 0xC;
    printsmall(a1);
    PrintCardTeam(8);
    g.printx = 0x15;
    g.printy = 0xC;
    printsmallz(str_DrawPlayerCard_4, sizeof str_DrawPlayerCard_4);
    g.printx = 0x15;
    g.printy++;
    printsmallz(str_DrawPlayerCard_5, sizeof str_DrawPlayerCard_5);
    g.printx = 0x15;
    g.printy = 0x12;
    printsmallz(str_DrawPlayerCard_6, sizeof str_DrawPlayerCard_6);
    g.printx = 0x15;
    g.printy++;
    printsmallz(str_DrawPlayerCard_7, sizeof str_DrawPlayerCard_7);
    team = card_team();
    player = GetCardPlayer();
    ReadPlayerRecord((uint32_t)(int32_t)(int16_t)player, team, ram_addr(ts));
    uint8_t r0 = ts[0], r1 = ts[1], r2 = ts[2], r3 = ts[3];
    uint16_t goalies = g.cardvis ? g.awaygoalies : g.homegoalies;
    if ((int16_t)goalies > (int16_t)g.cardplayer) {
        PrintSaveRecordLine(0x15, 0xE, r0, r1, r3, r2);
        PrintSaves(GetCardPlayer(), 0x15, 0x14);
        ST_W(g.matchup);
    } else {
        PrintGoalRecordLine(0x15, 0xE, r0, r1, r3, r2);
        PrintGoalsAssists(GetCardPlayer(), 0x15, 0x14);
        g.matchup = 0;
    }
    PrintOverallRating(GetCardPlayer(), 3, 0x11);
    PrintCardPosition(GetCardPlayer(), 3, 0xF);
}

/* cards94.asm:PrintCardRecords. Card 0: both sides cleared to the logo, the team's name, its
 * starting line and rating, and its records (the crowd record block): goals, saves and the crowd
 * level. */
void PrintCardRecords(void)
{
    ClearCardText(0x21, 8);
    g.sflags6 |= HI(0);
    ClearCardText(1, 8);
    g.sflags6 &= (uint16_t)~HI(0);
    g.palcount = 0x64;
    printz(str_PrintCardRecords_1, sizeof str_PrintCardRecords_1);
    PrintCardTeam(8);
    PrintStartingLine(3, 0xF);
    PrintTeamRating(8, 0xB);
    g.printx = 0x15;
    g.printy = 0xC;
    printsmallz(str_PrintCardRecords_2, sizeof str_PrintCardRecords_2);
    g.printx = 0x15;
    g.printy++;
    printsmallz(str_PrintCardRecords_3, sizeof str_PrintCardRecords_3);
    const uint8_t *r = g.ThreeStars;
    ReadCrowdRecord((uint32_t)(int32_t)(int16_t)card_team(), ram_addr(r));
    PrintGoalRecordLine(0x15, 0xE, r[0], r[1], r[3], r[2]);
    PrintSaveRecordLine(0x15, 0x12, r[4], r[5], r[7], r[6]);
    PrintCrowdRecordLine(0x15, 0x16, r[8], r[9], r[0xB], r[0xA]);
}

/* cards94.asm:ClearCardText. Erase a card side at (d0, d1): the team's logo bitmap
 * (TeamLogoBitmaps, TeamLogoPalettes) drawn over it, palettes 1's second half kept. */
void ClearCardText(uint16_t x, uint16_t y)
{
    printz(str_ClearCardText_1, sizeof str_ClearCardText_1);
    g.printx = x;
    g.printy = y;
    uint16_t t = (uint16_t)(card_team() << 2);
    uint32_t bm = rd32(rom_ptr(LG(TeamLogoBitmaps) + (int16_t)t));
    uint32_t pal = LG(TeamLogoPalettes) + (uint32_t)(int16_t)(uint16_t)((uint16_t)(t << 3) - 0x20);
    uint16_t keep[4];                                     /* (colours $11-$14 kept) */
    for (int i = 0; i < 4; i++) keep[i] = g.palfadenew[0x11 + i];
    dobitmap(pal, bm + rd32(rom_ptr(bm + 4)), bm + 8, 0, 0, 6, 6, g.cardlogochars1, 2);
    for (int i = 0; i < 4; i++) g.palfadenew[0x11 + i] = keep[i];
}

/* cards94.asm:PrintCardRecordLine. A record line at (d0, d1) when there is one (d2): the value and
 * its label (TempMaxSpd), then the holder (d3) and " vs.", then the opponent's user (d4) and
 * city (d5). */
void PrintCardRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam)
{
    if (value == 0) return;
    g.printx = x;
    g.printy = y;
    AppendNumber(value, MESAREA);
    appstring(g.TempMaxSpd, MESAREA);
    printsmall(MESAREA);
    g.printx = x;
    g.printy++;
    g.sflags6 |= HI(7);
    AppendUserName(MESAREA, (uint16_t)(int16_t)(int8_t)holder);
    appstring(SYM_CardVsTxt, MESAREA);
    printsmall(MESAREA);
    g.printx = x;
    g.printy++;
    g.sflags6 |= HI(7);
    AppendUserName(MESAREA, (uint16_t)(int16_t)(int8_t)vsuser);
    appstring(SYM_CardEmptyTxt, MESAREA);
    AppendTeamName(vsteam, MESAREA);
    printsmall(MESAREA);
}

/* cards94.asm:PrintCrowdRecordLine. The crowd level record line (CrowdLevelTxt). */
void PrintCrowdRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam)
{
    g.TempMaxSpd = SYM_CrowdLevelTxt;
    PrintCardRecordLine(x, y, value, holder, vsuser, vsteam);
}

/* the record label: "<n> Saves" / "Saves by" (with a holder), the singular after it for 1 */
static uint32_t record_label(uint32_t plain, uint32_t by, uint8_t value, uint8_t holder)
{
    uint32_t a1 = holder ? by : plain;
    if (value == 1) a1 += rd16(rom_ptr(a1));
    return a1;
}

/* cards94.asm:PrintSaveRecordLine. The saves record line (CardSavesTxt / CardSavesByTxt). */
void PrintSaveRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam)
{
    g.TempMaxSpd = record_label(SYM_CardSavesTxt, SYM_CardSavesByTxt, value, holder);
    PrintCardRecordLine(x, y, value, holder, vsuser, vsteam);
}

/* cards94.asm:PrintGoalRecordLine. The goals record line (CardGoalsTxt / CardGoalsByTxt). */
void PrintGoalRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam)
{
    g.TempMaxSpd = record_label(SYM_CardGoalsTxt, SYM_CardGoalsByTxt, value, holder);
    PrintCardRecordLine(x, y, value, holder, vsuser, vsteam);
}

/* cards94.asm:PrintOverallRating. "Overall Rating" at (d1, d2): player d0's (CalcAttrib with the
 * goalie mask when matchup is set), x 100 / d1, adjusted (AttribAdjust). */
void PrintOverallRating(uint16_t player, uint16_t x, uint16_t y)
{
    g.printx = x;
    g.printy = y;
    printsmallz(str_PrintOverallRating_1, sizeof str_PrintOverallRating_1);
    g.printx = x;
    g.printy++;
    printsmallz(str_PrintOverallRating_2, sizeof str_PrintOverallRating_2);
    uint32_t mask = rd32(rom_ptr(g.matchup ? SYM_GAttribOverallMask : SYM_PAttribOverallMask));
    uint16_t div, v = CalcAttrib(player, GetCardTeam(), mask, &div);
    v = AttribAdjust((uint16_t)m68k_divu((uint32_t)v * 100u, div));
    AppendNumber(v, MESAREA);
    printsmall(MESAREA);
}

/* cards94.asm:PrintStartingLine. "Starting Line" at (d0, d1), then a row a position
 * (LinePosNames): the team's starter there (its TeamList line at +6, FormatPlayerNameShort). */
void PrintStartingLine(uint16_t x, uint16_t y)
{
    uint16_t team = g.HomeTeam;
    struct Team *tm = GetTeamStruct(team);
    if (g.cardvis) {
        team = g.VisTeam;
        tm = GetTeamStruct(team);
    }
    uint32_t a0 = rd32(rom_ptr(LG(TeamList) + (uint32_t)(int32_t)(int16_t)(uint16_t)(team << 2)));
    a0 += (uint32_t)(int32_t)(int16_t)rd16(rom_ptr(a0 + 6));
    g.printx = x;
    g.printy = y;
    printsmallz(str_PrintStartingLine_1, sizeof str_PrintStartingLine_1);
    g.printy += 2;
    g.printx = x;
    for (uint32_t a1 = SYM_LinePosNames; rd16(rom_ptr(a1)) != 2; a1 += (uint32_t)(int32_t)(int16_t)rd16(rom_ptr(a1))) {
        g.printx = x;
        printsmall(a1);
        uint16_t p = (uint16_t)((int16_t)(int8_t)*rom_ptr(a0++) - 1);
        printsmall(FormatPlayerNameShort(p, tm));
        g.printy++;
    }
}

/* cards94.asm:PrintCardPosition. At (d1, d2) player d0's number (GetJerseyString) and Goalie,
 * Forward or Defenseman (his roster index against GetGoalieCount and GetDefenseStart). */
void PrintCardPosition(uint16_t player, uint16_t x, uint16_t y)
{
    GetJerseyString(player, MESAREA);
    g.printx = x;
    g.printy = y;
    printsmall(MESAREA);
    struct Team *tm = GetCardTeam();
    if ((int16_t)GetDefenseStart(tm) <= (int16_t)player)
        printsmallz(str_PrintCardPosition_3, sizeof str_PrintCardPosition_3);
    else if ((int16_t)GetGoalieCount(tm) <= (int16_t)player)
        printsmallz(str_PrintCardPosition_2, sizeof str_PrintCardPosition_2);
    else
        printsmallz(str_PrintCardPosition_1, sizeof str_PrintCardPosition_1);
}

/* cards94.asm:PrintTeamRating. "Team Rating" at (d0, d1): the card team's (GetTeamRating), 2 wide. */
void PrintTeamRating(uint16_t x, uint16_t y)
{
    g.printx = x;
    g.printy = y;
    printsmallz(str_PrintTeamRating_1, sizeof str_PrintTeamRating_1);
    g.printx = x;
    g.printy++;
    printsmallz(str_PrintTeamRating_2, sizeof str_PrintTeamRating_2);
    printsmall(PushNumberWidth(GetTeamRating(GetCardTeam()), 2));
}

/* cards94.asm:PrintCardTeam. The card team's city and nickname centred on rows d1 and d1 + 1. */
void PrintCardTeam(uint16_t y)
{
    g.printy = y;
    uint32_t a0 = GetCardTeam()->data;
    a0 += (uint32_t)(int16_t)rd16(rom_ptr(a0 + 4));
    PrintCentered(a0);
    a0 += (uint32_t)(int16_t)rd16(rom_ptr(a0));
    a0 += (uint32_t)(int16_t)rd16(rom_ptr(a0));
    g.printy++;
    PrintCentered(a0);
}

/* cards94.asm:PrintCentered. printsmall String a0 centred on column $14. */
void PrintCentered(uint32_t s)
{
    g.printx = (uint16_t)(0x14 - ((uint16_t)(m16(s) - 2) >> 1));
    printsmall(s);
}

/* cards94.asm:GetJerseyString. Player d0 of the card team: his number as a 2 digit String at a1
 * (a space for a leading 0). */
void GetJerseyString(uint16_t player, uint32_t buf)
{
    uint32_t a0 = GetCardTeam()->data;
    a0 += (uint32_t)(int32_t)(int16_t)rd16(rom_ptr(a0));
    uint16_t n = player;
    do a0 += (uint32_t)(int32_t)(int16_t)rd16(rom_ptr(a0)) + 8; while (n-- != 0);
    uint8_t num = *rom_ptr(a0 - 8);
    uint8_t *b = ram_bytes(ram_at(buf), 4);
    wr16(b, 4);
    b[2] = (num >> 4) ? (uint8_t)((num >> 4) + '0') : ' ';
    b[3] = (uint8_t)((num & 0xF) + '0');
}

/* cards94.asm:PrintGoalsAssists. "Goals" and "Assists" at (d1, d2): player d0's this game ($B4
 * and $CE of the team struct). */
void PrintGoalsAssists(uint16_t player, uint16_t x, uint16_t y)
{
    struct Team *tm = GetCardTeam();
    uint16_t goals = (uint16_t)(int16_t)(int8_t)tm->pgoals[(int16_t)player];
    uint16_t assists = (uint16_t)(int16_t)(int8_t)tm->passists[(int16_t)player];
    g.printx = x;
    g.printy = y;
    printsmallz(str_PrintGoalsAssists_1, sizeof str_PrintGoalsAssists_1);
    AppendNumber(goals, MESAREA);
    printsmall(MESAREA);
    g.printx = x;
    g.printy++;
    printsmallz(str_PrintGoalsAssists_2, sizeof str_PrintGoalsAssists_2);
    AppendNumber(assists, MESAREA);
    printsmall(MESAREA);
}

/* cards94.asm:PrintSaves. "Saves" and "Save %" at (d1, d2): goalie d0's shots ($E8 of the team
 * struct) less his goals against ($B4), and that x 100 / the shots (0 with none). */
void PrintSaves(uint16_t player, uint16_t x, uint16_t y)
{
    struct Team *tm = GetCardTeam();
    int16_t against = (int8_t)tm->pgoals[(int16_t)player];
    int16_t shots = (int8_t)tm->pshots[(int16_t)player];
    uint16_t saves = (uint16_t)(shots - against);
    g.printx = x;
    g.printy = y;
    printsmallz(str_PrintSaves_1, sizeof str_PrintSaves_1);
    AppendNumber(saves, MESAREA);
    printsmall(MESAREA);
    g.printx = x;
    g.printy++;
    printsmallz(str_PrintSaves_2, sizeof str_PrintSaves_2);
    uint32_t pct = (uint32_t)saves * 100u;
    pct = shots == 0 ? (pct & 0xFFFF0000u) : m68k_divu(pct, (uint16_t)shots);
    AppendNumber((uint16_t)pct, MESAREA);
    printsmall(MESAREA);
}

/* cards94.asm:DrawPictureBox. The card player's picture (DrawPlayerPicture; no map: the common
 * one) unpacked and drawn at (1, 8), its palette's second half to palette 2. */
void DrawPictureBox(void)
{
    uint16_t player = GetCardPlayer();
    uint32_t pic = DrawPlayerPicture(player, card_team());
    printz(str_DrawPictureBox_1, sizeof str_DrawPictureBox_1);
    uint32_t pal = ART(PicturePalette) + rd32(rom_ptr(ART(PicturePalette))), map;
    if (rd32(rom_ptr(pic + 4)) == 0) map = ART(PicturePalette) + rd32(rom_ptr(ART(PicturePalette) + 4));
    else map = pic + rd32(rom_ptr(pic + 4));
    UnpackPicture(pic + 8);
    pal_load(0x20, rom_ptr(pal + 0x20), 0x10);
    dobitmap(pal, map, ram_addr(g.picturebuf), 0, 0, 6, 6, g.cardlogochars2, 0);
}

/* cards94.asm:GetCardPlayer. cardplayer = the roster index of card cardindex (from 1) of the card
 * team's order (homecardlist / awaycardlist). */
uint16_t GetCardPlayer(void)
{
    uint16_t list = g.cardvis ? RAM_OFF_awaycardlist : RAM_OFF_homecardlist;
    g.cardplayer = (uint16_t)(int16_t)(int8_t)m8(RAM_BASE + ((uint16_t)(list + g.cardindex - 1)));
    return g.cardplayer;
}

/* cards94.asm:BuildCardPlayerLists. Both teams' card orders. */
void BuildCardPlayerLists(void)
{
    BuildCardPlayerList(team_struct(0), g.homecardlist);
    BuildCardPlayerList(team_struct(1), g.awaycardlist);
}

/* cards94.asm:BuildCardPlayerList. The card order of team a0 at a1: its 6 starters (its TeamList
 * line at +6) in roster order, then the others from a1 + 6. */
void BuildCardPlayerList(struct Team *tm, uint8_t *list)
{
    uint32_t a0 = tm->data;
    const uint8_t *line = rom_ptr(a0 + (uint32_t)(int16_t)rd16(rom_ptr(a0 + 6)));
    uint8_t *rest = list + 6;
    for (uint16_t p = 0; p < SYM_RosterSlots; p++) {
        int starter = 0;
        for (int i = 5; i >= 0; i--)
            if ((uint8_t)(line[i] - 1) == (uint8_t)p) { starter = 1; break; }
        if (starter) *list++ = (uint8_t)p;
        else *rest++ = (uint8_t)p;
    }
}
