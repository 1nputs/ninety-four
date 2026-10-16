/* Shared declarations for the C port. Every routine keeps the asm rebuild's name; the comment
 * on each prototype says which registers carried its arguments and results on the 68k. */
#pragma once
#include <stdint.h>
#include "ram.h"
#include "rom.h"

/* A byte access to a word flag (btst #n,(x).w) tests bit n of the HIGH byte: bit n+8 of the word. */
#define HI(n) (1u << ((n) + 8))
/* st on a word variable sets its high byte: tst.w then sees a negative value */
#define ST_W(x) ((x) = (uint16_t)(0xFF00 | ((x) & 0xFF)))

/* sort object numbers 0-11 are players (6 home, 6 away) */
#define IS_PLAYER(o) ((o)->SCnum <= 0xB)

/* flag bits, from equals.inc */
enum { sfpz, sfpj, sfspdir, sfssdir, sfwrap, sfscrl, sfslock, sfhor };                 /* sflags (word) */
enum { sf2faceoff, sf2refref, sf2drec, sf2replay, sf2shot, sf2pwrplay, sf2pwrtm, sf2offsig }; /* sflags2 (word) */
enum { sf3llcs, sf3rmplay, sf3alttree, sf3sbut };                                     /* sflags3 (byte) */
enum { gmclock, gmdir, gmpen, gmpendel, gmhl, gmoffs };                                /* gmode (byte) */
enum { dfok, df32c, dfng, dfclock };                                                   /* disflags (word): graphics ready, 32 column mode, no graphics, clock changed */
enum { pfdoff, pfna, pfnc, pfjoycon, pfrev, pfalock, pfteam, pfgoal };                 /* pflags */
enum { pf2fight, pf2aip };                                                             /* pflags2 */
enum { pf3oside, pf3break, pf3gctrl, pf3onetimer, pf3wall, pf3fall };                  /* pflags3 */

/* the work RAM the tests and lockstep compare: every named variable lies below it, the 68k's stack
 * above (the native game's stack is C's). The oracle shim's lockstep hash uses the same bound. */
#define RAM_VARS_END 0xDF00u

/* team struct (tmstruct = home, + tmsize = away) */
#define tmsize 0x364
#define tmscore 0xC
#define tmsort 0x22
#define tmap 0x24
#define tmgoalie 0x26
#define tmpdst 0x66
#define tmpde 0x32
#define tmflags 0x30
#define tmline 0x16
#define tmdata 0x1E
#define tmlasttouch 0x18   /* roster number of the last player to touch the puck (byte at +1) */
#define tmassist1 0x1A     /* the two before him */
#define tmassist2 0x1C
#define tmneardist 0x2A    /* long: distance squared from the puck to the team's nearest skater */
#define tmbreakaways 0x358
#define tmsoattempts 0x360
static inline uint8_t *team_struct(int away) { return ram_ptr(RAM_BASE + RAM_OFF_tmstruct) + (away ? tmsize : 0); }

/* big-endian access to untyped RAM (team struct and other tables not yet given a C type) */
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static inline void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
/* a 68k address in ROM or work RAM as a host pointer, and back */
static inline uint8_t *mem_ptr(uint32_t a) { a &= 0xFFFFFF; return a >= 0xE00000 ? ram_ptr(a) : (uint8_t *)rom_ptr(a); }
static inline uint32_t mem_addr(const uint8_t *p)
{
    if (p >= (const uint8_t *)&g && p < (const uint8_t *)&g + RAM_SIZE) return RAM_BASE + (uint32_t)(p - (const uint8_t *)&g);
    return (uint32_t)(p - rom_image);
}
/* the 68k address of something in work RAM, for pointers the game stores (move.l a2,(x)) */
#define ram_addr(p) (RAM_BASE + (uint32_t)((const uint8_t *)(p) - (const uint8_t *)&g))

/* a 16.16 position as the 68k reads it with move.l */
#define POS32(o, f) (((int32_t)(o) << 16) | (f))
/* a sort object from a stored tmsort word (the low word of its RAM address) */
static inline struct SortObj *sortobj_at(uint16_t lo) { return (struct SortObj *)ram_ptr(RAM_BASE + lo); }

/* divu.w: 32/16 unsigned divide, remainder high / quotient low; unchanged on overflow */
static inline uint32_t m68k_divu(uint32_t dividend, uint16_t divisor)
{
    if (divisor == 0) return dividend;
    uint32_t q = dividend / divisor, r = dividend % divisor;
    if (q > 0xFFFF) return dividend;
    return (r << 16) | q;
}

/* divs.w: 32/16 signed divide. Returns the register value: remainder in the high word, quotient
 * in the low word; on overflow (quotient outside 16 bits) the register is left unchanged. */
static inline int32_t m68k_divs(int32_t dividend, int16_t divisor)
{
    if (divisor == 0) return dividend;
    int32_t q = dividend / divisor, r = dividend % divisor;
    if (q < -32768 || q > 32767) return dividend;
    return (int32_t)(((uint32_t)(uint16_t)r << 16) | (uint16_t)q);
}

/* condition codes returned by routines whose callers test flags (ccr & CCR_N / CCR_Z) */
#define CCR_Z 4
#define CCR_N 8

/* d7 on the 68k: frames since the last game loop pass; set once per frame by DoGameFrame */
extern int16_t elapsed;

/* ---- main94.asm: power on (src/start.c) ---- */
void Start(void);                                       /* power on; does not return */
void SegaInit(void);

/* ---- checksum94.asm: the ROM check ---- */
void ValidationRoutine(void);

/* ---- video94.asm: shared helpers: palettes, random numbers, joysticks, VRAM, unpacking, printing ---- */
uint16_t randomd0(uint16_t range);        /* d0 = range -> d0 = random 0..range-1; advances seed */
uint16_t randomd0s(uint16_t range);       /* d0 -> d0, signed random */
void sfx(int16_t n);                       /* word on stack: sound effect, -1 = none */
void song(int16_t n);                      /* word on stack: song number */
uint16_t sroot(uint32_t v);                /* d0.l -> d0 */
void forceblack(void);
void forcefade(void);
void CopyPaletteToCRAM(void);
void UnpackNibbles(uint32_t src, uint16_t count);                 /* a0, d0 */
void remap(uint32_t src, uint32_t table, uint16_t words, uint16_t vram);   /* a0, a1, d0, d1 */
void waitx(uint16_t frames, uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);   /* d0 -> d0-d3 */
uint16_t waitxsr(uint16_t frames);         /* d0 -> d1 */
uint16_t waitjoy(void);                    /* -> d1 */
uint16_t orjoy(void);                      /* -> d1 */
void nodiag(uint16_t *d1, uint16_t d3);
void ProcessInputWithRepeat(uint16_t *d1, uint16_t d2, uint16_t d3);
void ReadJoy(uint8_t pad, uint16_t *d0, uint16_t *d1);           /* d0.b -> d0, d1 */
void DoDMApro(uint32_t src, uint16_t words, uint16_t vram);      /* a0, d0, d1 */
void DoDMA_nd2(uint16_t words, uint16_t dst, uint16_t src);      /* d0, d1, d2 (VRAM to VRAM) */
void WaitDMA(void);
void setVram(uint16_t colour);             /* d0 */
void setVram_0(void);
uint16_t DecompressGraphicsWithCallback(uint32_t tiles, uint16_t start, uint32_t cb);  /* a2, d4, table after the call -> d4 */
uint16_t DecompressGraphics(uint32_t tiles, uint16_t start);     /* a2, d4 -> d4 */
/* DecompressBytecode's registers: a0 = data, d1 = ring buffer index, d3 = VRAM address */
struct Unpack { uint32_t src; uint8_t idx; uint16_t vram; };
void DecompressBytecode(uint32_t src, uint16_t vram);            /* a0, d1 */
int Opcode_CopyLiteral(struct Unpack *u);                        /* the handlers return 1 at the end code */
int Opcode_ClearBytes(struct Unpack *u);
int Opcode_Fillbytes(struct Unpack *u);
int Opcode_CopyBackwardShort(struct Unpack *u);
int Opcode_CopyBackwardMedium(struct Unpack *u);
int Opcode_CopyBackwardLong(struct Unpack *u);
int Opcode_CopyBackwardExtended1(struct Unpack *u);
int Opcode_CopyBackwardExtended2(struct Unpack *u);
int Opcode_CopyBackwardReverseShort(struct Unpack *u);
int Opcode_CopyBackwardReverseMedium(struct Unpack *u);
int Opcode_CopyBackwardReverseLong(struct Unpack *u);
int CopyBackwardRun(struct Unpack *u, uint16_t count, uint8_t dist);        /* d0 = count-1, d2 = distance */
int CopyBackwardReverseRun(struct Unpack *u, uint16_t count, uint8_t dist);
void FlushOutputBuffer(struct Unpack *u);
void xyVmMap(void);
/* printsmall's registers: a1 = string, d2 = attribute, d3 = bytes left */
struct PrintState { const uint8_t *p; uint16_t attr; uint16_t count; };
void ControlCode_SetMapAndPosition(struct PrintState *st);
void ControlCode_SetMap(struct PrintState *st);
void ControlCode_SetAttribute(struct PrintState *st);
void ControlCode_SetX(struct PrintState *st);
void ControlCode_SetY(struct PrintState *st);
void ControlCode_AddX(struct PrintState *st);
void ControlCode_AddY(struct PrintState *st);
void ControlCode_SetFont(struct PrintState *st);
void FormatAndPrintTime(uint16_t v);       /* d0 */
void printbigz(const uint8_t *s, unsigned n);
/* printbig's pen: d4 = x, d5 = y, d6 = attribute + BigFontChars */
struct BigPen { uint16_t x, y, attr; };
void PrintBigChar(uint8_t c, struct BigPen *pen);                /* d0 */
void PutBigTile(uint16_t tile, const struct BigPen *pen);        /* d3 */
uint16_t AddSmallFont(uint16_t start);     /* d4 -> d4 */
uint16_t AddFramer(uint16_t start);        /* d4 -> d4 */
uint16_t AddTeamBlock(void);               /* -> d4 */
void printz(const uint8_t *s, unsigned n);       /* inline String after the call: n bytes */
void printsmallz(const uint8_t *s, unsigned n);
void eraser(uint16_t w, uint16_t h, uint16_t attr);             /* d0, d1, d2 at printx/printy */
void Framer(uint16_t w, uint16_t h);                            /* d0, d1 */
uint16_t DecompressGraphicsPlain(uint32_t tiles, uint16_t start); /* a2, d4 -> d4 = end char */
uint16_t dobitmap(uint32_t pal, uint32_t map, uint32_t tiles, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t start, uint16_t palbits); /* a0 a1 a2 d0-d5 -> d4 */
uint32_t print(uint32_t str);                           /* a1 = String (ROM or RAM address) -> a1 after it */
uint32_t PushTime(uint16_t t);                          /* d0 -> a1 */
uint32_t PushNumber(uint16_t n);                        /* d0 -> a1 */
uint32_t PushNumberWidth(uint16_t n, uint16_t width);   /* d0, d1 -> a1 */
void appendz(const uint8_t *s, unsigned n, uint32_t buf);   /* inline String appended to a3 */
void appstring(uint32_t src, uint32_t buf);             /* a1, a3 */
void printbig(uint32_t str);                            /* a1 */
uint32_t printsmall(uint32_t str);                      /* a1 -> a1 after it */
void DoFill(uint16_t words, uint16_t vram, uint16_t tile);   /* d0, d1, d2 */
uint16_t WeightedRandomSelect(uint16_t n);              /* d0 -> d0 */
void DoDMA(uint32_t src, uint16_t words, uint16_t vram);   /* a0, d0, d1 */
void Vmaddr(uint16_t vram);                             /* d0 = VRAM address -> a0 = Vdata */
void cramfade(void);
void forceblack2(void);
void ReadJoy1(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);
void ReadJoy2(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);
void ReadJoy3(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);
void ReadJoy4(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);

/* ---- sound94.asm: the 68k half of the sound driver; the Z80 relay (z80.c) ---- */
void p_turnoff(void);
void rtsfx(void);
void rtscmd10(void);
void play_sfx_or_music_track(uint16_t n);  /* d0 */
void stop_current_song(uint16_t d0);       /* d0 = the caller's d0 (its high byte leaks into an index) */
void ReadJoyData(void);
uint8_t ReadPad4Way1(void);                /* -> d0 */
uint8_t ReadPad4Way2(void);
uint8_t ReadPad4Way3(void);
uint8_t ReadPad4Way4(void);
uint8_t ReadPad4WayPort1(void);
uint8_t ReadPad1(void);
uint8_t ReadPad2(void);
uint8_t ReadPadA0(uint32_t port);          /* a0 -> d0 */
uint8_t ReadPad3Button(uint32_t port);     /* a0 -> d0 */
void ResetZ80Bus(void);
void p_music_vblank(void);
void z80_bus_release_delay(void);
void UploadCommandBufferToZ80(void);
void ProcessOneMusicTrack(uint8_t *slot, uint16_t track);        /* a5, d7 */
void handle_command_00(uint32_t ev, uint16_t track);             /* a0 = event, d7 */
void handle_command_10(uint32_t ev, uint16_t track);
void handle_command_30(uint32_t ev, uint16_t track);
void handle_command_40(uint32_t ev, uint16_t track);
void handle_command_60(uint32_t ev, uint16_t track);
void handle_command_skip(uint32_t ev, uint16_t track);
void ReleaseChannelAndNote(uint8_t *ch);                         /* a2 */
void StopPCMChannel(void);
void UpdateChannelFrequencyAndVolume(uint8_t *ch, const uint8_t *voices);   /* a2, a3 */
void SetChannelVolume(uint8_t *ch, const uint8_t *voices);                  /* a2, a3 */
void p_initialZ80(void);
void ClearAllTrackAndSFXSlots(void);

/* ---- hockey94.asm: the game loop, the clock, pause mode ---- */
void ClearShotData(void);
void restoreteams(void);
void ResetClock(void);
uint16_t GetPeriodTime(void);                           /* -> d0 */
void periodicevents(void);
void CheckInjury(void);
void RestoreBenchEnergy(void);
void CheckLateGameSong(void);
void UpdateCwdExcite(void);
void updatecrowdf(void);
void clockcont(void);
void clockcont_0(void);
int EndDemoOnPress(uint16_t d1);                        /* 1 = the demo ended */
void demoread(void);
void startpause(void);
void startpause1(void);
void startpause2(void);
void startpause3(void);
void startpause4(void);
void SetupPauseScreen(void);
uint8_t *seta2(void);                                   /* -> a2 */
void DoGameFrame(void);
void Gameloop(void);                                    /* does not return */
void StartPer(void);                                    /* does not return: resets the stack */
void Pausemode(void);
void VBjsr(void);                                       /* the vblank interrupt: the vbint handler */
void Begin(void);                                       /* does not return */
void StartGame(void);                                   /* does not return */

/* ---- display94.asm: the game play display: sprites, scroll, the vblank ---- */
/* The sprite builders share three registers: a5 = the DMA list being filled (long
 * source, word length in words, word VRAM address per entry), a6 = the sprite attribute table
 * being filled, d6 = the sprite link counter. */
struct Vid { uint8_t *dma; uint8_t *sat; uint16_t link; };
void VBlank(void);                                       /* the game play vblank handler */
void vb2(void);                                          /* the fades-only vblank handler */
void DumpSprites(void);
void DumpSprites2(void);
void DoDMAList(void);
void SetScroll2(void);
void setvideo(void);                                     /* once per game frame: builds the lists, sets dfok */
void updatescroll(struct Vid *v);                        /* a5 */
void showref(struct Vid *v);                             /* a5 */
void checkfo(struct Vid *v);                             /* a6, d6 */
void showzam(struct Vid *v);                             /* a6, d6 */
void SetSframe(uint32_t list, int16_t x, int16_t y, uint16_t frame, uint16_t startchar, struct Vid *v);  /* a0, d0, d1, d2, d3, a6, d6 */
void showcrowd(struct Vid *v);                           /* a6, d6 */
void showclock(struct Vid *v);                           /* a5 */
void checksso(struct Vid *v);                            /* a5, a6, d6 */
void setsortcords(struct Vid *v);
void setffo(struct Vid *v);
void uppads(struct Vid *v);                              /* a5 */
void QueuePadLabel(uint16_t code, uint8_t *pad, struct Vid *v);              /* d1, a0, a5 */
void RenderSmallFontChar(uint16_t ch, uint16_t slot, uint8_t *pad, struct Vid *v);   /* d2, d0, a0, a5 */
void addframe(struct SortObj *o, struct Vid *v);         /* a3, a5, a6, d6 */
void addframe2(int16_t x, int16_t y, struct SortObj *o, struct Vid *v);      /* d0, d1, a3, a5, a6, d6 */
void find3d(int16_t *x, int16_t *y, int16_t z);          /* d0, d1, d2 -> d0, d1 ($4E20 = off screen) */
void updatesound(void);
void KillCrowd(void);

/* ---- replay94.asm: the replay recorder and viewer, the per-object update loop, the camera ---- */
void getpzjoy(uint16_t *d0, uint16_t *d1, uint16_t *d2, uint16_t *d3);   /* the pausing pad's joystick */
void ShowReplayIcon(void);
void EraseReplayIcon(void);
void ShowReplayBanner(void);
void EraseReplayBanner(void);
uint16_t suba4(uint32_t *frame, uint8_t *cam);           /* a4 = frame address (in/out), a5 = camera object -> d7 */
uint16_t adda4(uint32_t *frame, uint8_t *cam);           /* a4 (in/out), a5 -> d7 */
uint16_t adda42(uint32_t *frame, uint8_t *cam);         /* a4 (in/out), a5 -> d7 (0 at the record point) */
void adda43(uint32_t *frame);                            /* a4 += $62, wrapping */
void UpdateCameraPos(const struct SortObj *o, uint8_t *cam);   /* a3, a5 */
uint16_t RevReplayAdj(uint32_t frame, uint8_t *cam);     /* a4, a5 -> d7 */
uint16_t RestoreReplayFrame(uint32_t frame, uint8_t *cam);   /* a4, a5 -> d7 (the frame's elapsed count) */
void ClampReplayView(void);
void updatereplay(void);                                 /* d7 = elapsed */
void updateplayers(void);                                /* d7 = elapsed */
void updateanim(struct SortObj *o);                      /* a3 */
void ReplayMode(void);
void freezewindow(void);                                /* lock the view */
void checkwindow(void);                                 /* scroll the view after the puck */

/* ---- checks94.asm: skater, goalie and puck assignments, acceleration ---- */
/* checks94 */
typedef uint16_t (*skate_aux)(uint16_t dir, struct SortObj *o);   /* a0 callback of skateto: d0 -> d0 */
void asswingo(uint8_t *tm, struct SortObj *o);        /* a2 = team, a3 = player */
void asscenterd(uint8_t *tm, struct SortObj *o);
void asscentero(uint8_t *tm, struct SortObj *o);
void assgoaliectrl(uint8_t *tm, struct SortObj *o);
void assgoaliecpu(uint8_t *tm, struct SortObj *o);
void checkanim(uint8_t *tm, struct SortObj *o);
void ClampYPosition(int16_t *d0, int16_t *d1, int16_t goalline, const struct SortObj *o);
void AdjustFacingDirection(int16_t dir, struct SortObj *o);     /* d0 */
void goaliesave(const uint8_t *pcross, int16_t goalline, struct SortObj *gk); /* a0, d3, a3 */
void assgoalietopuck(uint8_t *tm, struct SortObj *o);
int breakaway(struct SortObj *o);                      /* -> N flag (1 = not skating at the net) */
int BreakawayOffsidesFlagSet(struct SortObj *o);       /* -> d0 (1 = breakaway) */
void chkpuckc(uint8_t *tm, struct SortObj *o);
void asspuckc(uint8_t *tm, struct SortObj *o);
uint16_t asspuckc_chkdir(uint16_t dir, struct SortObj *o);   /* its skateto callback (.chkdir) */
int chk4lc(uint8_t *tm, struct SortObj *o);            /* 1 = the assignment ended (CompShoot) */
int chkpk(struct SortObj *o);                          /* -> Z */
int chkpk2(struct SortObj *o);                         /* -> Z */
int chk4shot(struct SortObj *o);                       /* 1 = the assignment ended */
void CompShoot(struct SortObj *o);
int chk4pass(struct SortObj *o);                       /* 1 = the assignment ended */
uint16_t EvadePC(uint16_t dir, struct SortObj *o);
void checkob(struct SortObj *o);
void assbreakaway(uint8_t *tm, struct SortObj *o);
void assnearest(uint8_t *tm, struct SortObj *o);
/* the 68k's d2 at the assignment dispatch (updateplayers: 6 or 9 for a player on the ice, else a
 * pad's changed buttons): assnearest reads it when its team has the puck (a register leak) */
extern uint32_t assign_d2;
void check4check(struct SortObj *o);
void asspassrec(uint8_t *tm, struct SortObj *o);
void SkateToTempTarget(struct SortObj *o);
void assshoot(uint8_t *tm, struct SortObj *o);
void puckshootout(uint8_t *tm, struct SortObj *o);
int SelectPenaltyShotSkater(void);                     /* -> N flag (1 = nobody eligible) */
void puckpenshot(uint8_t *tm, struct SortObj *o);
void UpdatePenaltyShotEnd(struct SortObj *o);
void EndPenaltyShotPlay(void);
void puckfaceoff(uint8_t *tm, struct SortObj *o);
void SetFaceoffComputerLine(uint16_t team, uint8_t *a2, uint8_t *a1);   /* d0, a2, a1 */
void StartFaceoffLineChange(uint16_t obj, struct SortObj *puck);        /* d0, a3 */
void WaitForFaceoffLineChanges(struct SortObj *puck);
void UpdateFaceoffLineChangeTimer(uint16_t off, struct SortObj *puck);  /* d0 = $40 / $42 */
void ChkGoalies(void);
void ReturnGoalies(void);
void CPgoalie(int16_t y, uint8_t *a1, uint8_t *a2);    /* d1, a1 = other team, a2 = team */
void CompLine(uint8_t *a2, uint8_t *a1);
void puckfaceoff2(uint8_t *tm, struct SortObj *o);
void ResetAndSelectPlayers(struct SortObj *o);         /* a3 passes through to changeplayer */
void updatefaceoff(struct SortObj *puck);
void Endfaceoff(struct SortObj *puck);
void pucknorm(uint8_t *tm, struct SortObj *puck);
void ChkOffsides(int16_t d0, struct SortObj *puck);
void ClearOffsidesIfAllPlayers(uint8_t *tm, int16_t *d0);
void a2offsides(struct SortObj *pl, struct SortObj *puck);
void a2touchpuck(struct SortObj *pl, struct SortObj *puck);
int16_t puckIChk(int16_t d0);                          /* d0 passes through unless the goal line is set */
void puckunflip(struct SortObj *puck);
void puckflip(int16_t d0, struct SortObj *puck);       /* d0 bit 0 flips the facing */
void puckshadow(uint8_t *tm, struct SortObj *shadow);
void findpc(void);
void skateto(int16_t x, int16_t y, skate_aux aux, struct SortObj *o);   /* d0, d1, a0, a3 */
void avdgoal(int16_t *x, int16_t *y, struct SortObj *o);
void skatetopuckinit(int16_t *x, int16_t *y);
void skatetopuck(struct SortObj *o);
void assexit(struct SortObj *o);
void assinsert(uint16_t ass, struct SortObj *o);       /* d0, a3 */
void assreplace(uint16_t ass, struct SortObj *o);      /* d0, a3 */
uint16_t vtoa(int16_t x, int16_t y);       /* d0/d1 = vector -> d0 = facing 0-7, 8 = none */
void GetHot(const struct SortObj *o, int16_t *hx, int16_t *hy);  /* long on stack -> d0/d1 */
void SetSPA(struct SortObj *o, uint16_t spa);                     /* a3, d1 */
void doplayeracc(uint16_t dir, struct SortObj *o);     /* d0 */
void goalieacc(uint16_t dir, struct SortObj *o);       /* d0 */
void noturn0(uint16_t d0, int16_t d2, struct SortObj *o);
void noturn(int16_t d2, struct SortObj *o);
void playeracc(int16_t dir, struct SortObj *o);        /* d2 */
void dostop(struct SortObj *o);
void stopna(struct SortObj *o);

/* ---- assign94.asm: the bench, penalty box, faceoff and celebration assignments ---- */
int check4bench(struct SortObj *o);                    /* a3; 1 = the calling assignment is over (it pops the return) */
void assbench(uint8_t *tm, struct SortObj *o);
void asseben(uint8_t *tm, struct SortObj *o);
void asspenalty(uint8_t *tm, struct SortObj *o);
void assdopen(uint8_t *tm, struct SortObj *o);
void assepen(uint8_t *tm, struct SortObj *o);
void assgoaliebreakwait(uint8_t *tm, struct SortObj *o);
void assfaceoff(uint8_t *tm, struct SortObj *o);
void assfaceoffpl(uint8_t *tm, struct SortObj *o);
void assfight(uint8_t *tm, struct SortObj *o);
void assfwatch(uint8_t *tm, struct SortObj *o);
void donothing(struct SortObj *o);                      /* a3 */
void assstanley(uint8_t *tm, struct SortObj *o);
void assscore(uint8_t *tm, struct SortObj *o);
void assdefo(uint8_t *tm, struct SortObj *o);
void assdefd(uint8_t *tm, struct SortObj *o);
void asswingd(uint8_t *tm, struct SortObj *o);

/* ---- input94.asm: controller input for skaters ---- */
void doinput(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, int16_t d4, struct SortObj *o); /* dpad, new, changed, held, pad, a3 */
void ClampTargetY(int16_t *d0, int16_t *d1, int16_t d3, const struct SortObj *o);
int16_t getGoalieSCnum(int16_t last);                  /* d0 -> d0 */
void SetLCmode(struct SortObj *o);                     /* a3 */
void SetLCmode2(uint8_t *tm);                          /* a2 */
int16_t getlchoice(int16_t choice, const uint8_t *tm); /* d0, a2 -> d0 */
void lcfound(uint16_t choice, uint8_t *tm, struct SortObj *o);   /* d2, a2, a3 */
void burst(struct SortObj *o);                         /* a3 */
void Acheck(struct SortObj *a0, struct SortObj *o);    /* a0 = the player held, a3 */
void dopass(struct SortObj *o);                        /* a3 */
void changeplayer(uint16_t pad, struct SortObj *o);    /* d4, a3 (sweep checks when the pad keeps its player) */
void Sweepcheck(struct SortObj *o);
void setc1player(uint16_t player);                     /* d0 */
void setc2player(uint16_t player);                     /* d0 */
int Findhittype(uint16_t dir, const struct SortObj *o); /* d0, a3 -> Z (1 = forehand) */
void SetShotMode(struct SortObj *o);
void ShotMode(uint16_t d0, uint16_t d2, struct SortObj *o);      /* dpad, changed buttons, a3 */
void setpads(int16_t pad, struct SortObj *o);          /* d4, a3 */
void doinput_cbut(uint16_t d1, uint16_t d3, struct SortObj *o);
void doinput_chkanim(struct SortObj *o);
void doinput_onetimer(uint16_t d1, int16_t d4, struct SortObj *o);
void doinput_ispc(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, int16_t d4, struct SortObj *o);
void doinput_islocked(uint16_t d1, int16_t d4, struct SortObj *o);
void setpassmode(struct SortObj *o);
void passmode(uint16_t d0, uint16_t d2, struct SortObj *o);
void holdplayer(struct SortObj *o);
void faceoffinput(uint16_t d0, uint16_t d1, struct SortObj *o);
void lineinput(uint16_t d0, uint16_t d1, uint16_t d2, struct SortObj *o);
void lcfound2(uint8_t *tm);
void setlccords(uint8_t *tm, uint16_t *d0, uint16_t *d1);
int16_t getlchoice2(int16_t d0, int16_t d2, const uint8_t *tm);
void passto(struct SortObj *rcv, struct SortObj *o);   /* a0, a3 */
void passtoa0(struct SortObj *rcv, struct SortObj *o); /* a0, a3 */
int16_t restorepl(int16_t old, int16_t new);           /* d1, d0 -> d0 */
void prepshot(struct SortObj *o);
void doshot(struct SortObj *o);
void shotdiradj(struct SortObj *o);
int16_t CalcAngleOffset(int16_t d0, int16_t d1, int16_t d2, int16_t d3, int16_t d4);   /* -> d4 */

/* ---- collide94.asm: collisions, checks, falls, the goal, the puck ---- */
void checkcoll(int16_t x, int16_t y, struct SortObj *o);          /* d2, d3, a3 */
void checkplcoll(int16_t x, int16_t y, struct SortObj *o);        /* d2, d3, a3 */
void checkcx(unsigned obj2, int16_t x, int16_t dy, struct SortObj *o);  /* d4 (object*2), d2, d5, a3 */
void newcheck(void);
void checkint(struct SortObj *a2, struct SortObj *a3);
void checkcheck(struct SortObj *a2, struct SortObj *a3, int16_t impact); /* a2, a3, d4 */
void CCStart(struct SortObj *a2, struct SortObj *a3, int16_t impact);
uint16_t checkagr(struct SortObj *a2, struct SortObj *a3);        /* -> d0 */
void holdcheck(struct SortObj *a2, struct SortObj *a3);
void Bcheck(struct SortObj *a2, struct SortObj *a3);
void FallDown(struct SortObj *a2, struct SortObj *a3);            /* a2 falls, a3 hit him */
void setInjuryType(struct SortObj *a2, struct SortObj *a3);
void checkwallcoll(int16_t x, int16_t y, struct SortObj *o);      /* d2, d3, a3 */
void checkgoal(struct SortObj *goal, struct SortObj *o, int16_t x, int16_t y);   /* a2, a3, d2, d3 */
uint16_t GetPeriodTimeRemaining(void);    /* -> d0 */
void checkgoalp(struct SortObj *goal, struct SortObj *o, int16_t x, int16_t y);  /* a2, a3, d2, d3 */
int CheckBump(struct SortObj *goal, struct SortObj *o);          /* a2, a3; 1 = net knocked (callers return) */
void wallcollb(int16_t c, int16_t s, struct SortObj *o);         /* d0, d1, a3 */
void wallcoll(int16_t c, int16_t s, struct SortObj *o);          /* d0, d1, a3 */
void checkpuckcoll_sfx(void);
void checkpuckcoll(struct SortObj *puck);                        /* a3 */
void puckstick(struct SortObj *p, struct SortObj *puck, uint32_t dsq);   /* a2, a3, d0 */
void puckglue(struct SortObj *p, struct SortObj *puck);          /* a2, a3 */
void puckbody(struct SortObj *p, struct SortObj *puck, uint32_t dsq);    /* a2, a3, d0 */
void puckgoalie(struct SortObj *gk, struct SortObj *puck, int16_t dx, int16_t dy); /* a2, a3, d0, d1 */
void deflect(struct SortObj *puck);                              /* a3 */
uint16_t makepde(int8_t v, struct SortObj *pl);                  /* d0, a0 -> d0 */
int16_t getpde(struct SortObj *pl);                              /* a3 -> d0 (a2 = team, d1 = pnum*2) */
void setpde(uint8_t *team, int16_t d1, int16_t e);               /* a2, d1, d0 */
void setpersonel(uint8_t *team);                                 /* a2 */
void SetPlList(uint8_t *team);                                   /* a2 */
int TryAddPlayerToList(uint8_t *team, uint16_t player, int slot); /* a2, d0, d4 -> d1 (0 = not added) */
void SetupPenaltyShot(uint8_t *team);                            /* a2 */
void forcepldata(uint8_t *team);                                 /* a2 */
void ResetBench(void);
void Setplass(struct SortObj *o);                                /* a3 */
void setplayer(uint16_t roster, struct SortObj *o);              /* d3, a3 */
uint8_t ClampNibble(uint8_t v);                                  /* d3 -> d3 */

/* ---- penalty94.asm: penalties, stoppages, the intermission and highlights ---- */
uint8_t *loadTeamStruct(const struct SortObj *o, uint8_t **other);   /* a3 -> a2 (a1 = the other team) */
void AddPenalty(uint16_t pen, struct SortObj *o);       /* d0, a3 */
void AddPenalty2(uint16_t pen, struct SortObj *o);      /* d0, a3 */
void PenaltyManager(void);
void chkprogress(void);
uint16_t PenShotChk(uint16_t pen, struct SortObj *hit, struct SortObj *o);   /* d0, a2 = player hit, a3 = checker -> d0 */
uint8_t getBAplayerInfo(uint16_t skater, const struct SortObj *o);          /* d0, a3 -> ccr (N = no penalty shot) */
void InProgress(void);
void coinsearch(uint8_t *tm);                           /* a2 */
void checkfornewpen(void);
void Stop4Pen(const uint8_t *a0, struct SortObj *o);    /* a0 = PenBuf entry + 2 (or the caller's own code), a3 */
void limitfo(void);
void UpdatePA(void);
void SetPA(uint16_t pen);                               /* d0 */
void SetPA2(void);
void PushRef(uint16_t frame);                           /* d0 */
void prefmes(int16_t pen);                              /* d0 */
void ClearPenaltyMessage(void);
void PenGoalStuff(uint8_t *a1, uint8_t *a2);            /* a1 = scored on team, a2 = scoring team */
void ClearPenaltyBuffer(void);
void updatepentime(void);
void ProcessPenaltyList(uint8_t *tm);                   /* a2 */
uint8_t *RemovePlayerFromList(uint8_t *a0);             /* a0 -> a0 */
void chkatop(void);
void releasepl(uint8_t *tm, uint16_t d0);               /* a2, d0 = player * 2 */
uint16_t GetLowestPen(const uint8_t *a2, const uint8_t *a3);   /* -> d0 */
void updatepwrplay(void);
void ClrHor(void);
void SetHor(void);
void printscores1(void);
void PrintTeamNameAndScore(const uint8_t *tm);          /* a2 */
void PrintTeamLogoAndScore(const uint8_t *tm, uint16_t logo);  /* a2, d0 */
void EASNLogo(void);
void USBoard(void);
void pplpen(uint8_t *tm, uint16_t d0);                  /* a2, d0 */
void linebar(uint16_t line, uint8_t *tm);               /* d0, a2 */
uint16_t getlinee(uint16_t line, const uint8_t *tm);    /* d0, a2 -> d0 */
uint16_t AvgCline(uint8_t *tm);                         /* a2 -> d0 */
void ChkShotStat(void);
void SetupTeamForIntermission(void);
void RestoreTeamEnergy(uint8_t *tm);                    /* a2 */
void reenergizeteam(uint8_t *tm);                       /* a2 */
void SetScore(uint8_t *a0);                             /* a0 = gsstruct entry */
void InitScores(void);
void UpdateScores(void);
int16_t NewTicker(const uint8_t **gs);                  /* -> d3, a0 */
int16_t NewTicker2(const uint8_t **gs);
int16_t CheckGameTickerStatus(int16_t d3, const uint8_t **gs);
void NewTicker3(int16_t d3, const uint8_t *gs);
void ClearTickerArea(void);
void SetTickerAreaPosition(uint16_t *d0, uint16_t *d1);
void PrintStringFromList(uint32_t list, uint16_t n);    /* a1, d0 */
uint32_t AdvanceStringPtr(uint32_t list, uint16_t n);   /* a1, d0 -> a1 */
void Intermission(void);
void DoHiLights(void);
void StartHL(uint8_t *gs, int16_t game);                /* a0 = the game's gsstruct, d1 = its number */
void StartHL2(uint8_t *gs, int16_t game);

/* ---- goalie94.asm: Manual Goalie, arena and faceoff animations ---- */
void ManualGoalieMenu(void);
void RunArenaAnim(void);
void NextArenaFrame(void);
void SetCameraTop(void);
void StartArenaAnim(uint16_t n);                        /* d0 */
void SetFaceoffAnim(uint16_t n);                        /* d0 */
void EndArenaAnim(void);

/* ---- onetimer94.asm: the one-timer ---- */
void puckvzadj(struct SortObj *o);                      /* a3 */
void OneTimerPass(struct SortObj *o);
void OneTimerTarget(struct SortObj *o);
void assonetimer(uint8_t *tm, struct SortObj *o);
uint16_t setonetimeranim(struct SortObj *o);            /* -> d1 */
int PuckOnAttackHalf(struct SortObj *o);                /* -> Z (1 = the puck is not on the attack half) */
void onetimershot(struct SortObj *a2, struct SortObj *o);
int CheckOneTimerFacing(uint16_t dir, const struct SortObj *o);   /* d0 -> Z */

/* ---- crowd94.asm: the crowd meter and hot / cold players ---- */
void LoadCrowdRec(void);
void Crowd_Noise(void);
void stopna2(struct SortObj *o);
void checkcornercoll(int16_t x, int16_t y, struct SortObj *o);   /* d2, d3, a3 */
void HotColdLoop(uint8_t *tm, int16_t d1);              /* a0, d1 */
void Create_HotCold_Table(uint8_t *tm);                 /* a0 */
uint16_t Clamp0to100(int16_t v);                        /* d3 -> d3 */
uint8_t AttributeCalc(uint8_t v, struct SortObj *o);    /* d3, a3 -> d3 */
uint16_t NextHomeHotPlayer(uint8_t **team);             /* -> d1, a1 */
uint16_t NextAwayHotPlayer(uint8_t **team);
uint16_t NextHomeColdPlayer(uint8_t **team);
uint16_t NextAwayColdPlayer(uint8_t **team);
void BuildHotColdLists(void);
void SortHotColdStarters(const uint8_t *tm);            /* a0 */
void CopyHottestPlayer(uint8_t *a0);
void CopyColdestPlayer(uint8_t *a0);
uint16_t CompareHotColdTotals(void);                    /* -> d0 */
uint16_t GetHotColdTotal(const uint8_t *tm);            /* a0 -> d1 */

/* ---- period94.asm: period and game statistics ---- */
void EndShootout(void);
void ClampPositionBox(struct SortObj *o);
void ClearGameStats(void);
void AddArenaAnimSprite(uint8_t **sat, uint16_t *link);   /* a6, d6 */
void PrintPlayerNameRight(uint16_t player, uint8_t *tm);  /* d0, a2 */
uint32_t SkipNameNumber(uint16_t player, uint8_t *tm);    /* d0, a2 -> a1 */
void SetStatScroll(void);
uint16_t StatScrollRow(void);                           /* -> d3 */
uint16_t StatScrollRowEnd(void);
void PrintStatScrollArrows(void);
void DisplayTeamStatsScreen(void);
void PrintStatValue(const uint8_t *a0, const uint8_t *tm);
void updatePPTeamTime(void);
uint16_t GetTeamRating(const uint8_t *tm);              /* a2 -> d0 */
void ShortenMsgTimer(struct SortObj *o);
void PeriodStatsScreen(void);
void GameStatisticsScreen(void);
void rtsStatTables(void);

/* ---- title94.asm: the title and credits, songs, shootout paths, record lines and other 94 helpers ---- */
void ChooseSong(void);
void EndOneTimer(struct SortObj *o);                    /* a3 */
uint16_t getFgtbyte(struct SortObj *a2, struct SortObj *a3);   /* -> d0 */
int chkFgtBit1(struct SortObj *a2, struct SortObj *a3);        /* -> Z flag (1 = zero) */
uint8_t ReadGoaliePulled(struct SortObj *o);            /* a3 -> ccr of tst.w tmgoalie */
int ShootoutShootCheck(struct SortObj *o);              /* a3 -> Z */
void SkatePath(int16_t *x, int16_t *y, struct SortObj *o); /* d0, d1, a3 -> d0, d1 */
void NextPathPoint(struct SortObj *o);                  /* a3 */
void StartShootoutPath(struct SortObj *o);              /* a3 passes through to NextPathPoint */
void CheckScoreLeader(void);
uint16_t RandomFaceoffAnim(void);                       /* -> d0 */
void LeadSong(void);
void ClearLeadSong(void);
void LeadSongExit(uint16_t sflags2);                    /* the sflags2 LeadSong saved */
void chgplayer(uint16_t pad, struct SortObj *o);        /* d4, a3 */
void rtsChgplayer(void);
void swpchk(struct SortObj *o);                         /* a3 */
void j_setc1player(uint16_t player);                    /* d0 */
void j_setc2player(uint16_t player);                    /* d0 */
void CountButtonPress(int16_t pad, struct SortObj *o);  /* d4, a3 */
void PSandSOpassdir(struct SortObj *o);                 /* a3 */
uint16_t LoadHomeTeamGfx(uint16_t start);               /* d4 -> d4 */
void HotColdIcon(uint8_t *tm);                          /* a2 */
void clrTmPdst(uint8_t *tm);                            /* a0 */
void ClearPenalties(void);
void PlaceBoardFall(struct SortObj *o);                 /* a3 */
void SetSflags8Bit1(void);
void PrintParenNumber(const uint8_t *table, uint16_t player);   /* a2 = team + table offset, d0 */
void PrintPlayerGoals(uint16_t player, uint8_t *tm);    /* d0, a2 */
void PrintPlayerAssists(uint16_t player, uint8_t *tm);  /* d0, a2 */
void ReadLineData(void);
void HiScoreScreen(void);
void newTitleScreen(void);
uint32_t CreditsPrintRow(uint32_t a1);                  /* a1 -> a1 */
int CreditsWait(void);                                  /* -> Start pressed (the 68k returns from its caller too) */
void VBlank_TitleScreen(void);
void CreditsLineScroll(void);
void CreditsScrollStep(void);
void UnpackPicture(uint32_t pic);                       /* a2 */
void DrawPlayoffSprite(void);
uint32_t PlayedByHome(void);                            /* -> a1 */
uint32_t PlayedByAway(void);
uint32_t PlayedByText(uint16_t user);                   /* d0 -> a1 */
uint32_t PlayedByNone(void);
uint32_t PlayedByExit(uint32_t a1);
uint16_t AttribAdjust(uint16_t v);                      /* d0 -> d0 */
void WriteLineData(void);
uint16_t GetLeagueCrowdRecord(void);                    /* -> d0 */
void ClearWinRecords(void);
uint8_t *GetTeamStruct(uint16_t team);                  /* d2 -> a2 */

/* ---- data94.asm: the boxes over the rink, player names, the playoff tree and password ---- */
void box(void);
void ShowGoalBox(void);
void PenaltyShotBox(void);
void ShowThreeStars(void);
void CalcStarScores(uint16_t d2);                       /* d2 = extra ice time a goalie needs */
int16_t CalcTeamStarScores(uint8_t *tm, const uint8_t *other, uint8_t **out, uint16_t mintime);   /* a2, a3, a4, d5 -> d3 */
uint16_t TakeTopStarScore(uint8_t **tm);                /* -> d0, a2 */
uint32_t GetPlayerNamePointer(uint16_t player, const uint8_t *tm);   /* d0, a2 -> a0 */
uint8_t *ConvertByteToDigits(uint16_t bcd, uint8_t *dst);          /* d0, a1 -> a1 */
uint32_t FinalizeTextBuffer(uint8_t *end);              /* a1 -> a1 = mesarea */
uint32_t FormatPlayerName(uint16_t player, uint8_t *tm);        /* d0, a2 -> a1 */
uint32_t FormatPlayerNumInitial(uint16_t player, uint8_t *tm);  /* d0, a2 -> a1 */
uint32_t getname(uint16_t player, const uint8_t *tm);  /* d0, a2 -> a1 */
uint32_t GetPlayerName(void);                           /* -> a1 */
uint32_t GetPlayerNumInitial(void);                     /* -> a1 */
void ShowInjuryBox(void);
uint16_t GetShifter(uint16_t *first_bit);               /* -> d1, d2 */
void AddPlayoffTeamStats(void);
void ReadTeamStats(void);
void DefaultMenus(void);
uint16_t SuperDiv(uint8_t *bits, uint16_t div);         /* a3, d0 -> d0 */
void ReadPassBits(uint8_t *bits);                       /* a3 */
uint16_t GetRandomUnusedTeam(uint32_t *used);           /* d3 -> d0 */
void InitializeGameStructures(void);
void FigureJoy(void);
void maketree(void);
void NewPO(void);
void ContinuePlayoffs(void);
void ClrPassBits(uint8_t *bits);                        /* a3 */
void SuperAdd(uint8_t *bits, uint32_t v);               /* a3, d0 */
void SuperMult(uint8_t *bits, uint16_t m);              /* a3, d0 */
void PushBits(uint8_t *bits, uint16_t v, uint16_t range);   /* a3, d0, d1 */
void WritePassBits(uint8_t *bits);                      /* a3 */
void ResolveGames(void);
void EncodePW(void);
uint32_t FormatLastName(uint32_t name, uint8_t *out);   /* a0, a1 -> a1 = mesarea */
uint32_t FormatPlayerNameShort(uint16_t player, const uint8_t *tm);  /* d0, a2 -> a1 */
uint32_t FormatPlayerNameLast(uint16_t player, const uint8_t *tm);   /* d0, a2 -> a1 */

/* ---- setup94.asm: the rink, teams and period setup, the front end's flow, the playoff screen ---- */
void PeriodOver(void);                                  /* does not return on the 68k */
void resetplstuff(void);
void SprSort(void);
uint16_t setupEASNmap(void);                            /* -> d4 */
void setupIceRinkPal(void);
uint16_t ReloadEnergyBarTiles(void);                    /* -> d4 */
uint16_t ReloadCrowdTiles(void);                        /* -> d4 */
void setplayercolors(void);
void setupice(void);
uint16_t defaultsprites(uint16_t chars);                /* d4 -> d4 */
uint16_t defaultsprites2(uint16_t chars);               /* d4 -> d4 */
uint16_t setupTeamBlocksMap(uint16_t chars);            /* d4 -> d4 */
uint8_t *CopyTeamBlockMapData(uint32_t map, uint16_t team, uint16_t vram, uint8_t *dst);  /* a0, d0, d1, a1 -> a1 */
uint16_t ReloadRefHorTiles(void);                       /* -> d4 */
void setupice_highlight(void);
void clearTeamStats(void);                              /* falls into setteams */
void setteams(void);
void InitTeamStructure(uint16_t team, uint8_t *tm);     /* d0, a2 */
void InitTeamLines(uint8_t *tm);                        /* InitTeamStructure+$10, a2 */
uint16_t ReloadRefTiles(void);                          /* -> d4 */
uint16_t ReloadFaceOffTiles(void);                      /* -> d4 */
uint16_t ReloadFaceOffMap(void);                        /* -> d4 */
void Opening(void);                                     /* none of the three returns */
void Opening2(void);
void Opening3(void);
void PlayoffScreen(void);                               /* the playoff tree */
int HandlePlayoffInput(void);                           /* -> 1: Start (PlayoffScreenExit) */
void UpdatePlayoffScroll(void);
void PlayoffScreenExit(void);
void PlayoffScreen_waitvsync(void);
void PrintSeriesScore(const uint8_t *gs);               /* a2 */
void DrawTreeArrow(uint16_t col);                       /* d0 */
void DrawTeamBlocks(uint16_t row);                      /* d1 */
void VBlank_Playoff(void);
uint8_t *InitScoutingDisplay(void);                     /* -> a0 */
void UpdateScoutingDisplay(void);
void ScrollDisplayUp(void);
void GameOver(void);                                    /* the password, the playoff screens, back to the title */
void IntermissionStart(void);                           /* the intermission, then StartPer or GameOver */
void ExitToOpening(void);                               /* restarts the front end */

/* ---- optsetup94.asm: the game setup screen ---- */
void GameSetUp(void);                                   /* the setup screen */
void GoNewPlayoffs(void);
void GoContinuePlayoffs(void);
uint16_t MoveMenuFrame(uint16_t line, int16_t by);      /* d7, d0 -> d7 */
void SetMenuArea(uint16_t *w, uint16_t *h);             /* -> d0, d1 */
void SetFrameRect(uint16_t line, uint16_t *w, uint16_t *h);   /* d3 -> d0, d1 */
void rtsSetup(void);
void FixModeOptions(uint16_t line);                     /* d7 */
void ChangeOption(uint16_t line, int16_t by);           /* d7, d2 */
uint16_t WrapOption(uint16_t v, int16_t by, uint16_t hi, uint16_t lo);   /* d3, d2, d4, d5 -> d3 */
void PrintOptions(uint16_t line);                       /* d7 */
void PrintOptionValue(uint16_t line);                   /* d7 */
void PrintScrollMarks(void);
void PrintOptTeamName(uint16_t line);                   /* d7 */
void PrintOptionNames(void);
void setoptions(void);
uint16_t ReadSetupPads(void);                           /* -> d1 */
void SetupDemo(void);
void SetupStart(void);
uint16_t LoadSetupTiles(void);                          /* -> d4 */
void UpdateRosterPlayers(void);
void EraseCard(void);
void DrawHomeBlock(void);
void DrawVisBlock(void);
void DrawTeamLogo(void);
void LogoBoxRight(void);
void LogoBoxHome(void);
void LogoBoxLeft(void);
void LogoBoxVis(void);
void DrawLogoBox(uint16_t chars);                       /* d4 */

/* ---- attract94.asm: the EA screen, matchup pictures, option screen helpers ---- */
void EASportsScreen(void);
void UpdatePlayoffLevel(void);
void DrawMatchupBitmaps(void);
uint16_t DrawTeamBitmap(uint16_t team, uint16_t chars); /* d1, d4 -> d4 */
void VBlank_SetOptions(void);

/* ---- scout94.asm: the scouting report ---- */
void ScoutingReport(void);                              /* the matchups */
uint32_t ScoutCrowdRecord(void);                        /* -> a1 */
void PageMatchup(int16_t by);                           /* d0 */
uint16_t PrintMatchupRating(uint8_t *tm);               /* a0 -> d0 */
void PrintTwoSpaces(void);
void DrawMatchupPictures(void);
void DrawMatchupPicture(uint32_t pic, uint16_t chars, uint16_t palbits);   /* a0, d4, d5 */
void DrawMatchupLogo(uint32_t logo, uint16_t d3, uint16_t chars, uint16_t palbits);   /* a0, d3, d4, d5 */
void DrawMatchupBitmap(uint32_t pal, uint32_t map, uint32_t tiles, uint16_t chars, uint16_t palbits);   /* a0, a1, a2, d4, d5 */
uint32_t GetTeamLogo(uint16_t *d3);                     /* d3 -> a0, d3 */
void PrintMatchupRatings(void);
void GetMatchupPlayers(void);
uint16_t GetMatchupPlayer(const uint8_t *tm);           /* a0 -> d0 */
void RestartAdvantageMarks(void);
void PrintAdvantageMarks(void);
uint32_t GetTeamNickname(const uint8_t *tm);            /* a2 -> a1 */
uint32_t GetTeamArena(const uint8_t *tm);               /* a2 -> a1 */

/* ---- records94.asm: the user records: the name entry, Record Holders ---- */
void UserNameEntry(void);                               /* the name entry */
void NameEntryScreen(void);
void NameLetterGrid(void);
void NameEntryHelp(void);
void PrintNamePrompt(void);
uint16_t SyncLetterCursor(void);                        /* -> d5 */
uint16_t FindLetter(uint16_t letter);                   /* d0 -> d0 */
void PrintNameCursor(uint16_t at);                      /* d4 */
void rtsNameCursor(void);
void PrintCurLetter(uint16_t letter);                   /* d5 */
void LetterGridPos(uint16_t letter, uint16_t *w, uint16_t *h);   /* d5 -> d0, d1 */
void PrintNameLog(void);
void MoveLogArrows(void);
void PrintLogArrows(uint32_t strs);                     /* a1 */
void GetNameLength(void);
void PrintNameField(uint32_t name);                     /* a1 */
void NameEntryFramer(uint16_t w, uint16_t h);           /* d0, d1 */
void GetLogName(uint32_t dst);                          /* a1 */
void StoreUserName(void);
void WriteNameRecord(void);
int SkipOtherUserName(int16_t dir);                     /* d0 (on the stack) -> Z: stepped */
void RecordHoldersScreen(void);                         /* the record holders */
void PrintRecordPage(void);
void PrintRecordTitles(void);
void PrintWinRecords(void);
void ClearRecordArea(uint16_t tile);                    /* d2 (not loaded: the caller's) */
void PrintPlayerRecords(void);
void PrintRecordName(uint16_t row, uint8_t user);       /* d7, a0[-1] */
void CalcWinPercents(void);
void ReadTeamRecords(void);

/* ---- cards94.asm: player cards and the save RAM records ---- */
void wallcollduringcheck(struct SortObj *a2, struct SortObj *a3);
void CountGoalies(void);
void CrowdRecordIO(uint32_t team, uint32_t buf);        /* d1, a0 */
void ReadCrowdRecord(uint32_t team, uint32_t buf);     /* d1, a0 */
void setSlotBit(void);
void CountPlayers(void);                                /* and sram94: the save records */
void PlayerRecordIO(uint32_t slot, uint16_t team, uint32_t buf);   /* d0, d1, a0 */
void ReadPlayerRecord(uint32_t slot, uint16_t team, uint32_t buf);
void WritePlayerRecord(uint32_t slot, uint16_t team, uint32_t buf);
void TeamRecordIO(uint32_t team, uint32_t buf);        /* d1, a0 */
void ReadTeamRecord(uint32_t team, uint32_t buf);
void WriteTeamRecord(uint32_t team, uint32_t buf);
void WriteCrowdRecord(uint32_t team, uint32_t buf);
void UpdatePlayerRecords(uint32_t team, uint32_t buf, const uint8_t *tm, uint16_t goalies, uint16_t opponent);   /* d1, a0, a2, d5, d6 */
void UpdateTeamRecord(uint32_t buf, uint32_t rec_no, uint16_t other, uint16_t team, uint16_t vs, const uint8_t *a1, const uint8_t *a2);   /* a0, d1, d2, d3, d4, a1, a2 */
void UpdateCrowdRecord(uint32_t buf, uint32_t team, uint16_t opp, uint16_t user, uint16_t opp_user, uint16_t goalies, const uint8_t *tm);   /* a0, d1, d2, d4, d5, d6, a1 */
void UpdateRecords(void);
void ResetCardTimer(void);
void PlayerCardTimer(void);
void PlayerCardScreen(void);
void PrintRecordValue(uint32_t buf, uint16_t team, uint16_t player);   /* a1, d0, d1 */
void CopyString(uint32_t src, uint32_t dst);            /* a1, a3 */
void AppendNumber(uint16_t n, uint32_t buf);            /* d0, a1 */
uint16_t GetRecordValue(uint16_t team, uint16_t player, uint16_t *goalie);   /* d0, d1 -> d0, d1 */
void PrintRecordHolder(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec);   /* a1, d0, d1, a0 */
void PrintRecordVs(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec);
void AppendRecordHolder(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec);
void AppendRecordVs(uint32_t buf, uint16_t team, uint16_t player, uint32_t rec);
void ReadNameLog(void);
void NameLogIO(void);
void AppendUserName(uint32_t buf, uint16_t user);       /* a1, d2 */
void AppendTeamName(uint16_t team, uint32_t buf);       /* d0, a1 */
void TrimSpaces(uint32_t str);                          /* a2 */
void WriteNameLog(void);
uint32_t DrawPlayerPicture(uint16_t player, uint16_t team);   /* d0, d1 -> a0 */
uint16_t CalcAttrib(uint16_t player, uint8_t *tm, uint32_t mask, uint16_t *div);   /* d0, a2, d4 -> d0, d1 */
void PlayerCards(void);                                 /* the player cards */
void WrapCardIndex(uint16_t idx);                       /* a0 (a RAM word) */
void DrawPlayerCard(uint16_t idx);                      /* a0 (a RAM word) */
void PrintCardRecords(void);
void ClearCardText(uint16_t x, uint16_t y);             /* d0, d1 */
void PrintCardRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam);   /* d0-d5 */
void PrintCrowdRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam);
void PrintSaveRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam);
void PrintGoalRecordLine(uint16_t x, uint16_t y, uint8_t value, uint8_t holder, uint8_t vsuser, uint8_t vsteam);
void PrintOverallRating(uint16_t player, uint16_t x, uint16_t y);   /* d0, d1, d2 */
void PrintStartingLine(uint16_t x, uint16_t y);         /* d0, d1 */
void PrintCardPosition(uint16_t player, uint16_t x, uint16_t y);    /* d0, d1, d2 */
void PrintTeamRating(uint16_t x, uint16_t y);           /* d0, d1 */
void PrintCardTeam(uint16_t y);                         /* d1 */
void PrintCentered(uint32_t s);                         /* a0 */
void GetJerseyString(uint16_t player, uint32_t buf);    /* d0, a1 */
void PrintGoalsAssists(uint16_t player, uint16_t x, uint16_t y);    /* d0, d1, d2 */
void PrintSaves(uint16_t player, uint16_t x, uint16_t y);           /* d0, d1, d2 */
void DrawPictureBox(void);
uint8_t *GetCardTeam(void);                             /* -> a0 */
uint16_t GetCardPlayer(void);                           /* -> d0 */
void BuildCardPlayerLists(void);
void BuildCardPlayerList(const uint8_t *tm, uint8_t *list);   /* a0, a1 */

/* ---- stats94.asm: the statistics and menu screens ---- */
uint16_t GetPlayerCount(uint8_t *team);                 /* a2 -> d0 */
uint16_t GetGoalieCount(const uint8_t *team);           /* a2 -> d0 */
void ReloadRinkGraphics(void);
uint16_t SetupScreen(uint16_t row, uint16_t rows);      /* d0, d1 -> d4 */
void ExitStatsScreen(void);                             /* falls into ReloadRinkGraphics */
uint16_t WaitVSyncAndReadInput(uint16_t *d0, uint16_t *d2, uint16_t *d3);   /* -> d1 */
void PlayoffStatsScreen(void);                          /* the player stats lists */
void PlayerStatsScreen(uint8_t *tm);                    /* a2 */
void PlayerStatsList(uint8_t *tm, int playoff);         /* a2, d7 */
void UpdateStatsListScroll(uint8_t *tm, int playoff);
void SetStatsListScrollReg(void);
void DrawStatsRowUp(uint8_t *tm, int playoff);
void DrawStatsRowDown(uint8_t *tm, int playoff);
void DrawStatsList(uint8_t *tm, int playoff);           /* a2, d7 */
void DrawStatsRow(uint16_t row, uint8_t *tm, int playoff);   /* d3, a2, d7 */
uint16_t GetStatColumn(uint16_t player, uint16_t col, uint8_t *tm, int playoff);    /* d0, d1, a2, d7 -> d2 */
uint16_t ReadStatColumn(uint16_t player, uint16_t col, uint8_t *tm, int playoff);   /* d0, d1, a2, d7 -> d2 */
uint16_t GetStatsValue(uint16_t at, uint16_t off, uint16_t v);   /* d1, d0, d2 -> d2 */
void DrawStatsListArrows(void);
void ShowScores(void);                                  /* the other scores */
void UpdateScoresScroll(void);
void UpdateVertScrollReg(void);
void DisplayGameInfo(uint16_t n);
void PrintGameInfo(uint16_t team, uint16_t score);
void DrawScrollArrows(void);
void LineEditor(uint8_t *tm);                           /* a2: the line editor */
int LineEditorRedraw(uint8_t *tm);
int LineEditorMenu(uint8_t *tm);
int SelectLinePlayer(uint8_t *tm);
int LineEditorExit(uint8_t *tm);
void PrintAttribHeader(uint8_t *tm);
void DrawLineIcons(uint8_t *tm);
void DrawLineIcon(uint16_t line, uint8_t *tm);
void ClearLineArea(void);
void DrawTeamScreen(uint8_t *tm);
void ClearLineEditorTop(void);
void ClearMenuFlags(void);
void SetLineSlot(uint8_t player, uint16_t slot, uint8_t *tm);
void DecodeTeamLines(uint8_t *tm);                      /* a2 */
void EncodeTeamLines(uint8_t *tm);                      /* a2 */
void TeamRosterScreen(uint8_t *tm);                     /* a2: the team roster */
void CheckPlayerListScroll(uint8_t *tm);
void UpdatePlayerListScroll(uint16_t v, uint8_t *tm);   /* d0, a2 */
void StopPlayerListScroll(void);
void DisplayPlayerListUp(uint16_t page, uint8_t *tm);
void DisplayPlayerListDown(uint16_t page, uint8_t *tm);
void DisplayPlayerList(uint8_t *tm);
void GetNameandAttrib(uint16_t player, uint32_t sel, uint8_t *tm);   /* d0, d4, a2 */
void AttribStatus(uint16_t player, uint8_t *tm);
void AttribEnergy(uint16_t player, uint8_t *tm);
void AttribFighting(uint16_t v, uint16_t div);          /* d0, d1 */
void AttribRating(uint16_t v, uint16_t div);            /* d0, d1 */
void AttribPrintPct(uint16_t v);
void AttribHanded(uint16_t v);
void AttribWeight(uint16_t v);
void ScoringSummaryScreen(void);
void CheckScoringScroll(void);
void UpdateScoringScroll(uint16_t v);                   /* d0 */
void DrawScoringRowUp(uint16_t entry);
void DrawScoringRowDown(uint16_t entry);
void DrawScoringRow(uint16_t at);                       /* d3 */
void PrintScorerName(uint8_t player, uint8_t *tm);      /* d0, a2 */
void PenaltySummaryScreen(void);
void CheckPenaltyScroll(void);
void UpdatePenaltyScroll(uint16_t v);                   /* d0 */
void DisplayPenaltyLineUp(uint16_t entry);
void DisplayPenaltyLineDown(uint16_t entry);
void DisplayPenaltyEntry(uint16_t at);                  /* d3 */
void DrawScrollArrowsPenalty(void);
void CrowdMeterScreen(void);
void PrintCrowdMeter(void);
void PrintCrowdDB(uint16_t level);                      /* d0 */
void TimeoutMenu(uint8_t *tm);                          /* a2 */
void SelectGoalieMenu(uint8_t *tm);                     /* a2 */
void DrawGoalieSelectList(uint8_t *tm);
uint16_t GetDefenseStart(const uint8_t *tm);            /* a2 -> d0 */
uint16_t GetForwardCount(const uint8_t *tm);            /* a2 -> d0 */

/* ---- shootout94.asm: the shootout ---- */
void ShootoutModeBox(void);
void NextShooter(struct SortObj *o);                    /* a3 passes through to StartShootoutPath */
void CountShootoutGoals(void);
void ShootoutWonBy(void);
void InitShooters(void);
void ClearShootout(void);
void ShootoutShooters(uint8_t *tm);                     /* a2, the shootout shooters */
void ShooterSelectList(uint8_t *tm);
void PrintShooterList(uint8_t *tm);
void ShootersBackground(uint8_t *tm);
void ClearShooterScreen(void);
void ResetShooterScroll(void);
void PrintShooterSlots(void);
void PrintShooterBox(uint8_t *tm);
void PrintShooterNames(uint8_t *tm);

/* ---- menu94.asm: the menus ---- */
void SetMenuPrintX(void);
uint32_t PrintMenuItem(uint32_t item);                  /* a1 -> a1 */
uint16_t UpdateMenuSelection(void);                     /* -> d1 */
uint16_t DrawMenuScreen(void);                          /* -> d1 */
int HandleMenuInput(uint16_t *d1);                      /* d1 = pad -> Z (1 = leave the menu), d1 */
void PutTeamBlock(uint16_t off);                        /* d0 */
void vcountwait(void);
uint16_t InitMenuState(uint32_t items, uint32_t draw);  /* a0, a1 (falls into DrawMenuScreen) -> d1 */

/* ---- sram94.asm: save RAM ---- */
void ReadSRAM(uint32_t at, uint32_t count, uint32_t dst);   /* d0, d1, a0 */
void WriteSRAM(uint32_t at, uint32_t count, uint32_t src);  /* d0, d1, a0 */
void MakeSRAMChecksum(void);
void InitSaveRAM(void);
void SRAMWaitVblank(void);
void VBcount(void);
void ValidateSRAM(void);
void ClearSRAM(void);

/* ---- fourway94.asm: four way play ---- */
void Set4WayPlayerStub(struct SortObj *o);              /* a3 (an rts) */
void Detect4WayPlay(void);

/* ---- dispatch: the C side of jsr (a0) and of the stack resets (dispatch.c) ---- */
void run68k(void (*entry)(void));                       /* call entry, and whatever restart_at names after it */
void restart_at(void (*entry)(void));                   /* drop the C stack and continue at entry */
void run68k_unwind(void);                               /* (weak) restart_at dropped the C frames above run68k: lockstep follows */
void call68k(uint32_t addr);
void call68k_missing(uint32_t addr);
