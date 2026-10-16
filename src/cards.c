/* cards94.asm: player cards and the save RAM records. Only the parts the game play calls are
 * here so far; the card and record screens come with the front end. */
#include <string.h>
#include "game.h"
#include "symbols_gen.h"
#include "hal.h"
#include "cards_strings_gen.h"

#define RAM_W(off) ram_ptr(RAM_BASE + (off))

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

/* ---- the player cards of the setup screen --------------------------------------------------- */

#define CARD(n) HI(n)                                    /* setupcardflags bits (see optsetup.c) */
#define MESAREA (RAM_BASE + RAM_OFF_mesarea)
#define TEMPBUF (RAM_BASE + RAM_OFF_TempBuffer)

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
    uint32_t list = rd32(rom_ptr(SYM_FeaturedPictures + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    g.cardroster = rd16(rom_ptr(list + (int16_t)d3 + 4));
    uint32_t pic = rd32(rom_ptr(list + (int16_t)d3));
    uint32_t pal = SYM_PicturePalette + rd32(rom_ptr(SYM_PicturePalette)), map;
    if (rd32(rom_ptr(pic + 4)) == 0) map = SYM_PicturePalette + rd32(rom_ptr(SYM_PicturePalette + 4));   /* no map: the common one */
    else map = pic + rd32(rom_ptr(pic + 4));
    UnpackPicture(pic + 8);
    uint16_t v = g.vcount;
    while (g.vcount == v) hal_wait_vblank();
    uint8_t keep[8];
    memcpy(keep, RAM_W(RAM_OFF_palfadenew + 0x22), 8);
    dobitmap(pal, map, RAM_BASE + RAM_OFF_picturebuf, 0, 0, 6, 6, chars, 2);
    memcpy(RAM_W(RAM_OFF_palfadenew + 0x22), keep, 8);
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
    uint32_t a2 = rd32(rom_ptr(SYM_TeamList + (uint32_t)(int16_t)(uint16_t)(team << 2)));
    a2 += (uint32_t)(int16_t)rd16(rom_ptr(a2));          /* the roster: name String, then 8 bytes */
    for (uint16_t n = g.cardroster; n-- != 0;) a2 += (uint32_t)(int16_t)rd16(rom_ptr(a2)) + 8;
    uint8_t *mes = RAM_W(RAM_OFF_mesarea);
    wr16(mes, 6);
    ConvertByteToDigits((uint16_t)(int16_t)(int8_t)*rom_ptr(a2 + (int16_t)rd16(rom_ptr(a2))), mes + 2);   /* the uniform number */
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
    if (v == 0) { wr16(mem_ptr(buf), 2); return; }
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
    const uint8_t *s = mem_ptr(src);
    uint8_t *d = mem_ptr(dst);
    uint16_t n = (uint16_t)(rd16(s) - 1);
    do *d++ = *s++; while (n-- != 0);
}

/* cards94.asm:AppendNumber. d0 (0-999, no leading zeros) as a String at a1, padded to an even
 * length. */
void AppendNumber(uint16_t n, uint32_t buf)
{
    uint8_t *start = mem_ptr(buf), *p = start + 2;
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
    uint16_t v = (uint16_t)(int16_t)(int8_t)ram_ptr(RAM_BASE)[0];
    uint32_t blk = rd32(rom_ptr(SYM_TeamList + (uint32_t)(int16_t)(uint16_t)(team << 2)));
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
    if (rd16(mem_ptr(TEMPBUF)) == 2) wr16(mem_ptr(buf), 2);
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
static const uint8_t *player_record(uint16_t team, uint16_t player, uint32_t rec)
{
    if (rec == 0) {
        ReadPlayerRecord((uint32_t)(int32_t)(int16_t)player, (uint16_t)(int16_t)team, RAM_BASE);
        rec = RAM_BASE;
    }
    return mem_ptr(rec);
}

/* cards94.asm:AppendRecordHolder. The user name of the record holder (byte 1 of record a0) as a
 * String at a1 (AppendUserName, trimmed). */
void AppendRecordHolder(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec)
{
    const uint8_t *r = player_record(team, player, rec);
    g.sflags6 |= HI(7);
    AppendUserName(buf, (uint16_t)(int16_t)(int8_t)r[1]);
}

/* cards94.asm:AppendRecordVs. The user name (byte 3 of record a0), a space and the opponent's
 * city (byte 2, AppendTeamName) as a String at a1. */
void AppendRecordVs(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec)
{
    const uint8_t *r = player_record(team, player, rec);
    g.sflags6 |= HI(7);
    AppendUserName(buf, (uint16_t)(int16_t)(int8_t)r[3]);
    appstring(SYM_RecSpaceTxt, buf);
    AppendTeamName(r[2], buf);
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
    if (g.sflags6 & HI(6)) WriteSRAM(SYM_NameLogBase, SYM_NameLogBytes, RAM_BASE + RAM_OFF_namelog);
    else ReadSRAM(SYM_NameLogBase, SYM_NameLogBytes, RAM_BASE + RAM_OFF_namelog);
}

/* cards94.asm:AppendUserName. User d2's name (12 bytes of namelog, a 0 as a space) as a String at
 * a1, trimmed (TrimSpaces) with sflags6 bit 7; user 0: an empty String (NoNameTxt). */
void AppendUserName(uint32_t buf, uint16_t user)
{
    uint8_t *b = mem_ptr(buf);
    if (user == 0) {
        CopyString(SYM_NoNameTxt, buf);
        TrimSpaces(buf);
        return;
    }
    const uint8_t *name = mem_ptr(RAM_BASE + RAM_OFF_namelog + (uint32_t)user * 0xC);   /* (mulu.w: any byte) */
    for (int i = 0; i < 12; i++) b[2 + i] = name[i] ? name[i] : ' ';
    wr16(b, 0xE);
    if (g.sflags6 & HI(7)) TrimSpaces(buf);
}

/* cards94.asm:AppendTeamName. Append team d0's city (its TeamList block String) to a1. */
void AppendTeamName(uint16_t team, uint32_t buf)
{
    uint32_t blk = rd32(rom_ptr(SYM_TeamList + (uint32_t)(int16_t)(uint16_t)((int16_t)(int8_t)team << 2)));
    blk += (uint32_t)(int32_t)(int16_t)rd16(rom_ptr(blk + 4));
    blk += (uint32_t)(int16_t)rd16(rom_ptr(blk));
    appstring(blk, buf);
}

/* cards94.asm:TrimSpaces. Drop the trailing spaces of String a2 (zeroed) and pad its length to
 * even. */
void TrimSpaces(uint32_t str)
{
    uint8_t *s = mem_ptr(str), *p = s + rd16(s);
    while (*--p == ' ') {
        *p = 0;
        wr16(s, (uint16_t)(rd16(s) - 1));
    }
    wr16(s, (uint16_t)((rd16(s) + 1) & 0xFE));
}
