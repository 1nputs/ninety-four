/* sound94.asm: the 68000 half of the sound driver. It starts and stops sounds and songs, plays
 * the music tracks event by event, sets each channel's patch, volume and pitch in a 33-byte
 * command buffer, and hands that buffer to the Z80 relay program, which drives the FM and PSG
 * chips and streams the PCM samples. The pad readers run from its vblank routine.
 *
 * Its RAM (src/ram.h): the 8 track slots (struct TrackSlot g.fm_slots: the event stream, the
 * delay), the 6 channels just below them (struct Channel g.fm_channels: key, note, volume, patch,
 * output channel, age, key on; the sixth is the PCM channel), the voice of each key (struct Voice
 * g.fm_voices: pitch bend, patch, volume), and Z80_command_buffer: key off, key on, volume,
 * frequency and patch change bits (one per output channel), followed by the attenuation, frequency
 * and patch tables, 33 bytes in all copied to Z80 RAM $02. */
#include "game.h"
#include "hal.h"
#include "rom.h"
#include "symbols_gen.h"
#include <string.h>

#define CMD g.Z80_command_buffer                      /* [0] key off, [1] key on, [2] volume, [3] frequency, [4] patch */
#define VOICES g.fm_voices
#define IO_CT1_PORT 0xA10003u
#define IO_CT2_PORT 0xA10005u

static void bset(uint8_t *p, unsigned bit) { *p |= (uint8_t)(1u << (bit & 7)); }
static void needs_update(void) { g.music_needs_z80_update = 0xFF; }

/* take the Z80 bus and wait for it (move.w #$100,(IO_Z80BUS) / btst #0 / bne) */
static void take_bus(void)
{
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
}

/* the voice table entry of midi channel bits 3-0 of status byte st on track */
static uint16_t voice_key(uint8_t st, uint16_t track) { return (uint16_t)(((st & 0xF) << 3) | track); }
/* a key's voice (the cartridge's streams play midi channels 0-7: the table's 64; past them the
 * 68000 would read the channels after it) */
static struct Voice *voice(struct Voice *voices, uint16_t key) { return &voices[key & 0x3F]; }

/* local tables of UpdateChannelFrequencyAndVolume and SetChannelVolume. The 68k can index past
 * the ends, so they are read from the ROM where they are. */
static uint32_t local_table(uint32_t from, uint32_t len, const uint8_t *head, unsigned n)
{
    static uint32_t cache[3][2];
    for (int i = 0; i < 3; i++) if (cache[i][0] == from) return cache[i][1];
    uint32_t a = rom_find(from, from + len, head, n);
    for (int i = 0; i < 3; i++) if (!cache[i][0]) { cache[i][0] = from; cache[i][1] = a; break; }
    return a;
}
static const uint8_t *fnum_table(void)
{
    static const uint8_t head[] = { 0x01, 0x46, 0x01, 0x59, 0x01, 0x6E };
    return rom_ptr(local_table(SYM_UpdateChannelFrequencyAndVolume, 0x100, head, sizeof head));
}
static const uint8_t *bend_table(void)
{
    static const uint8_t head[] = { 0x40, 0x00, 0x40, 0x3B, 0x40, 0x76 };
    return rom_ptr(local_table(SYM_UpdateChannelFrequencyAndVolume + 1, 0x100, head, sizeof head));
}
static const uint8_t *vel_table(void)
{
    static const uint8_t head[] = { 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0E, 0x0C };
    return rom_ptr(local_table(SYM_SetChannelVolume, 0x80, head, sizeof head));
}

/* sound94.asm:rtsfx. The shared rts. */
void rtsfx(void) {}
/* sound94.asm:rtscmd10. The shared rts. */
void rtscmd10(void) {}
/* sound94.asm:handle_command_skip. Events $2x, $5x and $7x are ignored. */
void handle_command_skip(uint32_t ev, uint16_t track) {}

/* sound94.asm:ClearAllTrackAndSFXSlots. Free the 8 track slots and reset the 6 channel structs
 * (output channels 0, 1, 2, 4, 5, 6). */
void ClearAllTrackAndSFXSlots(void)
{
    for (int i = 0; i < 8; i++) g.fm_slots[i].stream = 0xFFFFFFFF;
    for (int n = 5; n >= 0; n--) {
        struct Channel *ch = &g.fm_channels[n];
        ch->out = (uint8_t)(n >= 3 ? n + 1 : n);
        ch->patch = 0xFF;
        ch->key = 0xFF;
        ch->note = 0;
        ch->flags = 0;
    }
}

/* sound94.asm:p_turnoff. Silence everything: free all slots, key off and mute every output
 * channel, send the buffer and stop the PCM channel. */
void p_turnoff(void)
{
    ClearAllTrackAndSFXSlots();
    CMD[0] = 0x77;                                            /* key off channels 0-2, 4-6 */
    CMD[2] = 0x77;                                            /* volume change on them */
    for (int i = 0; i < 7; i++) g.per_channel_attenuation_table[i] = 0x7F;
    UploadCommandBufferToZ80();
    StopPCMChannel();
}

/* sound94.asm:play_sfx_or_music_track. d0 = sound 0-$7A ($30 and up are songs, which stop the
 * current song first): start its event stream (SoundStreamTable) in the free track slot (the
 * one with the lowest pointer) and reset that track's eight voices (volume $7F). */
void play_sfx_or_music_track(uint16_t n)
{
    if (n > 0x7A) { rtsfx(); return; }
    if ((int16_t)n >= 0x30) stop_current_song(n);
    struct TrackSlot *slot = &g.fm_slots[0];
    int32_t low = (int32_t)slot->stream;
    uint16_t track = 7;
    for (int i = 1; i < 8; i++)
        if (low > (int32_t)g.fm_slots[i].stream) { slot = &g.fm_slots[i]; low = (int32_t)slot->stream; track = (uint16_t)(7 - i); }
    if ((int32_t)slot->stream >= 0) return;              /* no free slot */
    uint32_t stream = rd32(rom_ptr(SYM_SoundStreamTable + (uint16_t)(n << 2)));
    slot->stream = stream;
    slot->delay = m8(stream);                            /* the first delay */
    for (int i = 0; i < 8; i++) {                        /* the track's voice on each midi channel */
        struct Voice *v = &VOICES[track + 8 * i];
        v->bend = 0;
        v->unused2 = 0;
        v->patch = 0;
        v->volume = 0x7F;
    }
}

/* sound94.asm:stop_current_song. Free the slot playing a song (pointer at or past the first song
 * stream), key off and mute its channels and send the buffer. d0 = the caller's d0: the 68k
 * loads only the low byte of each channel number into d0 and indexes the attenuation table
 * with d0.w, so a high byte of $FF left by the caller puts the mute 256 bytes lower (in
 * pwddatabuffer) and leaves the channel's volume alone. */
void stop_current_song(uint16_t d0)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);                                   /* no interrupts while the slots change */
    int32_t first = (int32_t)rd32(rom_ptr(SYM_SongPointerTable));
    int16_t track = 7;
    int found, i = 0;
    for (;;) {                                           /* cmpa.l / dble */
        found = first <= (int32_t)g.fm_slots[i++].stream;
        if (found || track-- == 0) break;
    }
    if (!found) { cpu_set_sr(sr); return; }
    memset(CMD, 0, 5);
    g.fm_slots[i - 1].stream = 0xFFFFFFFF;
    for (int k = 0; k < 6; k++) {
        struct Channel *ch = &g.fm_channels[k];
        if ((ch->key & 7) != track) continue;
        ReleaseChannelAndNote(ch);
        uint8_t out = ch->out;
        d0 = (uint16_t)((d0 & 0xFF00) | out);
        if (!(d0 & 0xFF00)) g.per_channel_attenuation_table[out] = 0x7F;
        else w8(rp_add(RAM(per_channel_attenuation_table), (int16_t)d0), 0, 0x7F);   /* (the caller's high byte: elsewhere) */
        bset(&CMD[2], out);
    }
    UploadCommandBufferToZ80();
    cpu_set_sr(sr);
}

/* sound94.asm:ResetZ80Bus. Reset the Z80 and wait for its bus. */
void ResetZ80Bus(void)
{
    z80_reset(0x100);
    take_bus();
}

/* request the bus and wait up to $65 polls, then reset the Z80 (the pad readers) */
static void bus_for_pads(void)
{
    z80_busreq(0x100);
    for (int n = 0; n < 0x65; n++) if (!(z80_busack() & 1)) return;
    ResetZ80Bus();
}

/* sound94.asm:ReadPad3Button. Read the 3 button pad at port a0 (TH low, then high), release
 * the bus -> d0 = the pad byte, active low: buttons in bits 4-7, the directions (PadDirTable
 * drops opposite pairs) in bits 0-3. */
uint8_t ReadPad3Button(uint32_t port)
{
    pad_write(port, 0);
    uint8_t lo = pad_read(port);
    pad_write(port, 0x40);
    uint8_t hi = pad_read(port);
    z80_busreq(0);
    uint8_t d0 = (uint8_t)(((lo << 2) & 0xC0) | (hi & 0x3F));
    d0 = (uint8_t)~d0;
    uint8_t dir = (uint8_t)~hi & 0xF;
    d0 = (uint8_t)((d0 & 0xF0) | rom_ptr(SYM_PadDirTable)[dir]);
    return (uint8_t)~d0;
}

/* sound94.asm:ReadPadA0. Take the Z80 bus, then ReadPad3Button of port a0. */
uint8_t ReadPadA0(uint32_t port) { bus_for_pads(); return ReadPad3Button(port); }
/* sound94.asm:ReadPad1. Pad 1 (port 1). */
uint8_t ReadPad1(void) { return ReadPadA0(IO_CT1_PORT); }
/* sound94.asm:ReadPad2. Pad 2 (port 2). */
uint8_t ReadPad2(void) { return ReadPadA0(IO_CT2_PORT); }
/* sound94.asm:ReadPad4WayPort1. The 4 way adaptor's selected pad, read on port 1. */
uint8_t ReadPad4WayPort1(void) { return ReadPad3Button(IO_CT1_PORT); }

/* ReadPad4Way1-4: take the bus, select pad n of the 4 way adaptor on port 2, read it on port 1 */
static uint8_t read_4way(uint8_t select)
{
    bus_for_pads();
    pad_write(IO_CT2_PORT, select);
    return ReadPad4WayPort1();
}
/* sound94.asm:ReadPad4Way1. */
uint8_t ReadPad4Way1(void) { return read_4way(0x0C); }
/* sound94.asm:ReadPad4Way2. */
uint8_t ReadPad4Way2(void) { return read_4way(0x1C); }
/* sound94.asm:ReadPad4Way3. */
uint8_t ReadPad4Way3(void) { return read_4way(0x2C); }
/* sound94.asm:ReadPad4Way4. */
uint8_t ReadPad4Way4(void) { return read_4way(0x3C); }

/* sound94.asm:ReadJoyData. Read the pads every vblank to pad4way1-4: four through the 4 way
 * adaptor with FourWayPlay, else pads 1 and 2. */
void ReadJoyData(void)
{
    if (g.FourWayPlay) {
        g.pad4way1 = ReadPad4Way1();
        g.pad4way2 = ReadPad4Way2();
        g.pad4way3 = ReadPad4Way3();
        g.pad4way4 = ReadPad4Way4();
    } else {
        g.pad4way1 = ReadPad1();
        g.pad4way2 = ReadPad2();
    }
}

/* sound94.asm:p_music_vblank. Vblank: read the pads, clear the change bits, run the 8 track
 * slots (track 7 first), send the buffer if anything changed and age the channels. With
 * PALflag bit 0 the tracks run twice every 6th frame. */
void p_music_vblank(void)
{
    ReadJoyData();
    g.music_needs_z80_update = 0;
    memset(CMD, 0, 5);
    for (;;) {
        for (int i = 0; i < 8; i++) ProcessOneMusicTrack(&g.fm_slots[i], (uint16_t)(7 - i));
        if (!(g.PALflag[0] & 1)) break;
        if ((int16_t)--g.music_tick_divider >= 0) break;
        g.music_tick_divider += 6;
    }
    if (g.music_needs_z80_update) UploadCommandBufferToZ80();
    for (int i = 0; i < 6; i++) {
        struct Channel *ch = &g.fm_channels[i];
        if (++ch->age == 0) ch->age = 0xFF;              /* age, held at $FF */
    }
}

/* sound94.asm:z80_bus_release_delay. The Z80 is busy: give the bus back and wait a little
 * (then UploadCommandBufferToZ80 tries again). */
void z80_bus_release_delay(void)
{
    z80_busreq(0);
}

/* sound94.asm:UploadCommandBufferToZ80. Once the Z80 is idle (Z80 RAM $97 = 0, $96 = $7D), mark
 * it busy ($96 = $D1) and copy the 33-byte command buffer to Z80 RAM $02. */
void UploadCommandBufferToZ80(void)
{
    for (;;) {
        take_bus();
        if (z80_ram_read8(0x97) == 0 && z80_ram_read8(0x96) == 0x7D) break;
        z80_bus_release_delay();
    }
    z80_ram_write8(0x96, 0xD1);
    z80_ram_write8(0x97, 0);
    uint8_t buf[0x21];
    ram_read(buf, RAM(Z80_command_buffer), sizeof buf);  /* (the bytes as the 68000 has them, for the Z80) */
    for (int i = 0; i < 0x21; i++) z80_ram_write8((uint16_t)(2 + i), buf[i]);
    z80_busreq(0);
}

/* sound94.asm:ProcessOneMusicTrack. Count down track slot a5 (track d7) and run every event
 * that is due (4 bytes: delay, status, two data bytes; the delay of the next event follows).
 * Status 0 ends the stream, or loops to the pointer after it when that is not negative. */
void ProcessOneMusicTrack(struct TrackSlot *slot, uint16_t track)
{
    static void (*const command_jump_table[8])(uint32_t, uint16_t) = {
        handle_command_00, handle_command_10, handle_command_skip, handle_command_30,
        handle_command_40, handle_command_skip, handle_command_60, handle_command_skip,
    };
    for (;;) {
        uint16_t delay = (uint16_t)(slot->delay - 1);
        slot->delay = delay;
        if ((int16_t)delay >= 0) { rtsfx(); return; }
        uint32_t ev;
        for (;;) {                                       /* .event */
            if ((int32_t)slot->stream < 0) { rtsfx(); return; }
            ev = slot->stream;
            slot->stream = ev + 4;
            uint32_t e = ev;
            slot->delay = m8(e + 4);
            if (m8(e + 1) != 0) break;
            slot->stream |= 0xFF000000;                  /* end of the stream (st: the high byte) */
            if ((int16_t)m16(e + 2) < 0) { rtsfx(); return; }
            slot->stream = m32(e + 2);                   /* loop */
        }
        command_jump_table[(m8(ev + 1) & 0x70) >> 4](ev, track);
    }
}

/* sound94.asm:handle_command_00. Event $0x: key off note +2 on midi channel +1 bits 3-0 of the
 * track: the first channel struct playing that key and note that is on. */
void handle_command_00(uint32_t ev, uint16_t track)
{
    uint32_t e = ev;
    uint16_t key = (uint16_t)(voice_key(m8(e + 1), track) << 8 | m8(e + 2));
    struct Channel *ch = &g.fm_channels[6];
    int16_t n = 5;
    for (;;) {
        int eq;
        do { ch--; eq = (ch->key << 8 | ch->note) == key; } while (!eq && n-- != 0);   /* dbeq */
        if (!eq) { rtsfx(); return; }
        if (ch->flags & 1) break;                                           /* dbne */
        if (n-- == 0) { rtsfx(); return; }
    }
    ReleaseChannelAndNote(ch);
}

/* sound94.asm:ReleaseChannelAndNote. Key off channel struct a2 if it is on; a PCM patch stops
 * the PCM channel instead. */
void ReleaseChannelAndNote(struct Channel *ch)
{
    int on = ch->flags & 1;
    ch->flags &= (uint8_t)~1;
    if (!on) { rtsfx(); return; }
    if ((int8_t)ch->patch >= 0x60) { StopPCMChannel(); return; }
    bset(&CMD[0], ch->out);
    needs_update();
}

/* sound94.asm:StopPCMChannel. Clear the PCM rate byte (Z80 RAM $8E): the PCM channel stops. */
void StopPCMChannel(void)
{
    take_bus();
    z80_ram_write8(0x8E, 0);
    z80_busreq(0);
}

/* sound94.asm:handle_command_10. Event $1x: key on note +2 at volume +3 (0 = key off) on midi
 * channel +1 bits 3-0 of the track. An FM patch takes a free channel (one already set to the
 * patch if there is one) or else the oldest; a PCM patch ($60 up) takes the PCM channel unless
 * it plays a higher patch, and sets the sample start / end in Z80 RAM $23-$28. Then the volume
 * and the frequency. (When no FM channel is free the 68k keys on the last channel it tested,
 * struct 0, though it set the patch of the oldest: kept.) */
void handle_command_10(uint32_t ev, uint16_t track)
{
    uint32_t e = ev;
    if (m8(e + 3) == 0) { handle_command_00(ev, track); return; }
    struct Channel *ch = &g.fm_channels[5];                /* the PCM channel (fm_channel_struct6) */
    struct Voice *voices = VOICES;
    uint16_t key = voice_key(m8(e + 1), track);
    uint8_t patch = voice(voices, key)->patch;
    if ((int8_t)patch >= 0x60) {                          /* PCM */
        int on = ch->flags & 1;
        ch->flags |= 1;
        if (on && (int8_t)patch <= (int8_t)ch->patch) { rtscmd10(); return; }
        ch->patch = patch;
        ch->age = 0;
        ch->key = (uint8_t)key;
        ch->note = m8(e + 2);
        ch->volume = m8(e + 3);
        const uint8_t *t = rom_ptr(SYM_pcm_sample_table + (uint16_t)(((int8_t)patch - 0x60) << 3));
        uint32_t start = rd32(t), end = rd32(t + 4);
        take_bus();
        z80_ram_write8(0x25, (uint8_t)start);
        z80_ram_write8(0x24, (uint8_t)(start >> 8));
        z80_ram_write8(0x23, (uint8_t)(start >> 16));
        z80_ram_write8(0x28, (uint8_t)end);
        z80_ram_write8(0x27, (uint8_t)(end >> 8));
        z80_ram_write8(0x26, (uint8_t)(end >> 16));
        z80_ram_write8(0x96, 0x29);
        z80_ram_write8(0x97, 0);
        z80_busreq(0);
        SetChannelVolume(ch, voices);
        UpdateChannelFrequencyAndVolume(ch, voices);
        return;
    }
    /* the oldest of the five FM channels (ties: the lower struct) */
    struct Channel *oldest = &g.fm_channels[4];
    for (int k = 3; k >= 0; k--)
        if (!(oldest->age > g.fm_channels[k].age)) oldest = &g.fm_channels[k];
    /* a free channel, preferably one with the patch already */
    struct Channel *use = oldest;
    int16_t n = 4;
    int keyon;
    for (;;) {
        int free_;
        do { ch--; free_ = !(ch->flags & 1); } while (!free_ && n-- != 0);                  /* dbeq */
        if (!free_) { keyon = use->patch == patch; break; }                   /* .0 */
        use = ch;
        if (use->patch == patch) { keyon = 1; break; }
        if (n-- == 0) { keyon = 0; break; }
    }
    if (!keyon) {                                         /* .patch */
        ch = use;
        ch->patch = patch;
        uint8_t out = ch->out;
        bset(&CMD[4], out);
        g.per_channel_patch_table[out] = patch;
        needs_update();
    }
    ch->age = 0;                                          /* .keyon */
    ch->flags |= 1;
    ch->key = (uint8_t)key;
    ch->note = m8(e + 2);
    ch->volume = m8(e + 3);
    bset(&CMD[0], ch->out);
    bset(&CMD[1], ch->out);
    SetChannelVolume(ch, voices);
    UpdateChannelFrequencyAndVolume(ch, voices);
}

/* sound94.asm:UpdateChannelFrequencyAndVolume. The frequency of channel struct a2 from its note
 * (octave and the note's F-number) and its key's pitch bend (a3 = voice table) scaled by the
 * patch's bend range (12 for PCM). FM: block and F-number to the frequency table; PCM: the
 * rate to Z80 RAM $8E while the channel is on. */
void UpdateChannelFrequencyAndVolume(struct Channel *ch, struct Voice *voices)
{
    uint32_t oct = m68k_divu(ch->note, 12);
    uint16_t octave = (uint16_t)oct;
    uint16_t f = rd16(fnum_table() + 2 * (oct >> 16));
    int16_t bend = voice(voices, ch->key)->bend;
    if (bend != 0) {
        int16_t range = 0xC;
        if ((int8_t)ch->patch < 0x60) range = (int8_t)rom_ptr(SYM_fm_instrument_patches)[(uint16_t)(ch->patch << 5) + 0x1E];
        int32_t b = ((int32_t)bend * range) >> 2;
        int16_t idx = (int16_t)((int16_t)b >> 7);
        idx = (int16_t)((idx + 0xC0) * 2);
        uint32_t prod = (uint32_t)f * rd16(bend_table() + idx);
        f = (uint16_t)((prod << 1) >> 16);
    }
    if ((int8_t)ch->patch < 0x60) {
        f |= (uint16_t)(octave << 11);
        uint8_t out = ch->out;
        bset(&CMD[3], out);
        g.per_channel_frequency_table[out] = f;
        needs_update();
        return;
    }
    if (!(ch->flags & 1)) return;
    take_bus();
    uint16_t shift = (uint16_t)(8 - octave) & 63;
    z80_ram_write8(0x8E, (uint8_t)(shift >= 16 ? 0 : f >> shift));
    z80_busreq(0);
}

/* sound94.asm:SetChannelVolume. The volume of channel struct a2: note volume * the voice volume
 * (a3 + 4) / 128. FM: an attenuation (.veltab by volume / 8) to the attenuation table; PCM: Z80
 * RAM $83 (at least 3). */
void SetChannelVolume(struct Channel *ch, struct Voice *voices)
{
    uint16_t v = (uint16_t)((uint16_t)(ch->volume * voice(voices, ch->key)->volume) >> 7);
    if ((int8_t)ch->patch < 0x60) {
        uint8_t att = vel_table()[v >> 3];
        uint8_t out = ch->out;
        g.per_channel_attenuation_table[out] = att;
        bset(&CMD[2], out);
        needs_update();
        return;
    }
    take_bus();
    v = (uint16_t)((v & 0xFF00) | (uint8_t)((uint8_t)v >> 2));
    if ((int16_t)v <= 3) v = 3;
    z80_ram_write8(0x83, (uint8_t)v);
    z80_busreq(0);
}

/* sound94.asm:handle_command_40. Event $4x: the patch of the key (voice table +3) = +2. */
void handle_command_40(uint32_t ev, uint16_t track)
{
    uint32_t e = ev;
    voice(VOICES, voice_key(m8(e + 1), track))->patch = m8(e + 2);
}

/* sound94.asm:handle_command_60. Event $6x: the pitch bend of the key = word +2 - $2000, and a
 * new frequency for every channel playing that key. */
void handle_command_60(uint32_t ev, uint16_t track)
{
    uint32_t e = ev;
    uint16_t key = voice_key(m8(e + 1), track);
    voice(VOICES, key)->bend = (int16_t)(m16(e + 2) - 0x2000);
    for (int i = 0; i < 6; i++)
        if (g.fm_channels[i].key == (uint8_t)key) UpdateChannelFrequencyAndVolume(&g.fm_channels[i], VOICES);
}

/* sound94.asm:handle_command_30. Event $3x: controller +2 = +3. Only controller 7, the volume:
 * the voice volume (voice table +5) and a new volume for every channel playing that key. */
void handle_command_30(uint32_t ev, uint16_t track)
{
    uint32_t e = ev;
    if (m8(e + 2) != 7) return;
    uint16_t key = voice_key(m8(e + 1), track);
    struct Voice *v = voice(VOICES, key);
    v->volume = (uint16_t)((v->volume & 0xFF00) | m8(e + 3));   /* (move.b: the low byte) */
    for (int i = 0; i < 6; i++)
        if (g.fm_channels[i].key == (uint8_t)key) SetChannelVolume(&g.fm_channels[i], VOICES);
}

/* sound94.asm:p_initialZ80. Free all slots, load the Z80 program ($295 bytes) into Z80 RAM,
 * build 29 tables of 256 bytes below Z80 RAM $2000 ((x - $80) * 8 / n + $80 for n = 8-$24), then
 * reset and start the Z80. */
void p_initialZ80(void)
{
    ClearAllTrackAndSFXSlots();
    z80_reset(0x100);
    take_bus();
    const uint8_t *code = rom_ptr(SYM_Z80_Program_Code);
    for (uint16_t i = 0; i < 0x295; i++) z80_ram_write8(i, code[i]);
    uint16_t at = 0x2000;
    for (int16_t div = 8; div < 8 + 0x1D; div++) {
        for (int16_t x = 0xFF; x >= 0; x--) {
            int32_t q = m68k_divs((int32_t)(int16_t)((x - 0x80) << 3), div);
            z80_ram_write8(--at, (uint8_t)((int16_t)q + 0x80));
        }
        z80_ram_write8(at, 0);
    }
    z80_reset(0);
    z80_busreq(0);
    z80_reset(0x100);
    g.music_needs_z80_update = 0;
}
