#!/usr/bin/env python3
"""Generate src/ram.h and src/ram.c: the 68k work RAM ($FFFF0000-$FFFFFFFF) as one packed C
struct at the 68000's offsets, its scalars in the host's byte order, and the map from each machine
byte to the host byte holding it (with the machine image's pack and unpack).

Inputs: ref/NHL94Genesis/src/stubinc/ram_addrs.inc (names, addresses, comments),
        out/ramwidths.json (access widths inferred by tools/ramwidths.py),
        ref/NHL94Genesis/src/stubinc/equals.inc (sort object member offsets).
Every symbol becomes a field at its exact offset; gaps become padding; a symbol whose region
is a multiple of its access width becomes an array. Static asserts pin every offset.
"""
import re, sys, json, pathlib, collections

P = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[1]
REF = P / 'ref/NHL94Genesis/src'
rows = json.load(open(P / 'out/ramwidths.json'))
RAM_BASE, RAM_SIZE = 0xFFFF0000, 0x10000

CTYPE = {'b': 'uint8_t', 'w': 'uint16_t', 'l': 'uint32_t'}
# symbols whose width the access scan cannot see (indexed through an address register)
OVERRIDE = {'OOlistpos': 'w', 'Ylist': 'w', 'OOlist': 'b', 'padcont': 'w', 'per_channel_frequency_table': 'w', 'PushNumberBuf': 'b'}
# symbols kept inside another structure, so no field of their own (a byte area named name_): the
# game reads and writes them by address, RAM(name), and their bytes follow that structure
BY_ADDRESS = {'puckc': "the puck's sort object's newpos and newpnum, read by the 68000 as one word"}
# the fields the game names (g.x in its code): only these must agree with the sort object views
NAMED = set()
for _f in [*(P / 'src').glob('*.c'), *(P / 'src').glob('*.h'), *(P / 'native').glob('*.c'), *(P / 'test').glob('*.c')]:
    if _f.name not in ('ram.h', 'ram.c'):
        NAMED.update(re.findall(r'\bg\.(\w+)', _f.read_text(errors='replace')))
# sort object members: positions are 16.16 fixed point (integer word first), velocities signed
SORT_DECL = {
    'Xpos': (['int16_t Xpos;', 'uint16_t Xfrac;'], 4), 'Ypos': (['int16_t Ypos;', 'uint16_t Yfrac;'], 4),
    'Zpos': (['int16_t Zpos;', 'uint16_t Zfrac;'], 4), 'OldXpos': (['int16_t OldXpos;', 'uint16_t OldXfrac;'], 4),
    'OldYpos': (['int16_t OldYpos;', 'uint16_t OldYfrac;'], 4), 'OldZpos': (['int16_t OldZpos;', 'uint16_t OldZfrac;'], 4),
    'facedir': (['int16_t facedir;', 'uint16_t _pad56;'], 4), 'nopuck': (['uint8_t nopuck[2];'], 2),
}
SORT_SIGNED = {'Xvel', 'Yvel', 'Zvel', 'impact', 'limpact', 'impactp', 'position', 'temp1', 'temp2', 'temp3', 'temp4', 'temp5',
               'radiusx', 'radiusy', 'Wallcos', 'Wallsin', 'SCnum', 'assnum'}
WSIZE = {'b': 1, 'w': 2, 'l': 4}


def pick_width(widths, gap):
    """Choose the scalar width for a region from its access counts and size."""
    acc = {k: v for k, v in widths.items() if k in WSIZE}
    if gap == 2 and ('w' in acc or not acc):
        return 'w'                      # flag words tested by byte still live as one word
    if gap == 1:
        return 'b'
    if gap == 4 and 'l' in acc:
        return 'l'                      # a long stored whole and read by halves (seed, pointers)
    if acc:
        return max(acc, key=acc.get)
    if gap == 4:
        return 'l'
    return 'b'


def cident(name):
    return re.sub(r'\W', '_', name)


# ---- sort object struct from equals.inc ---------------------------------------------------
eq = (REF / 'stubinc/equals.inc').read_text(errors='replace').splitlines()
members = []
in_block = False
for line in eq:
    if 'the sort object structure' in line:
        in_block = True; continue
    if in_block:
        m = re.match(r'^(\w+)\s+equ\s+\$([0-9A-Fa-f]+)\s*(?:;\s*(.*))?$', line)
        if m:
            members.append((m.group(1), int(m.group(2), 16), (m.group(3) or '').strip()))
        elif line.strip() == '' and members:
            break
# access widths for members: name(aN) / name(aN,dM.w)
mw = collections.defaultdict(collections.Counter)
ins_re = re.compile(r'^(?:[.\w]+:?)?\s+([a-z]+)(?:\.([bwls]))?\s+(.*?)\s*(?:;.*)?$')
byte_ops = {'btst', 'bset', 'bclr', 'bchg', 'sf', 'st', 'tas'}
names = {m[0] for m in members}
for f in sorted(REF.glob('*.asm')):
    for line in f.read_text(errors='replace').splitlines():
        m = ins_re.match(line)
        if not m or line.lstrip().startswith(';'):
            continue
        mn, sz, ops = m.groups()
        for om in re.finditer(r'(?<![\w.])(\w+)\(a\d', ops):
            n = om.group(1)
            if n in names:
                w = 'b' if mn in byte_ops else ({'s': 'w'}.get(sz, sz) if sz else '?')
                if mn in ('lea', 'pea'):
                    w = 'addr'
                mw[n][w] += 1

SCSTRUCT = 0x80
members.sort(key=lambda t: t[1])
sort_fields = []
pos = 0
for i, (n, off, com) in enumerate(members):
    if off < pos:
        continue  # overlapping alias (e.g. pflags2 inside a word); first one wins
    if off > pos:
        sort_fields.append(f'    uint8_t _pad{pos:02X}[{off - pos}];')
        pos = off
    nxt = members[i + 1][1] if i + 1 < len(members) else SCSTRUCT
    gap = max(1, min(nxt, SCSTRUCT) - off)
    if n in SORT_DECL:
        decls, size = SORT_DECL[n]
        sort_fields += ['    ' + d + ('  /* ' + com + ' */' if com and d is decls[0] else '') for d in decls]
        pos = off + size
        continue
    w = pick_width(mw.get(n, {}), gap)
    size = WSIZE[w]
    ct = ('int16_t' if n in SORT_SIGNED and w == 'w' else CTYPE[w])
    if gap >= 2 * size and gap % size == 0:
        sort_fields.append(f'    {ct} {cident(n)}[{gap // size}];{"  /* " + com + " */" if com else ""}')
        pos = off + gap
    else:
        sort_fields.append(f'    {ct} {cident(n)};{"  /* " + com + " */" if com else ""}')
        pos = off + size
if pos < SCSTRUCT:
    sort_fields.append(f'    uint8_t _pad{pos:02X}[{SCSTRUCT - pos}];')

# ---- the team struct (HmShots / AwShots, tmsize apart) ------------------------------------
# equals.inc's tm offsets, named as the code and the game's own stats table (TeamStatTextTbl) name
# them. Offsets are asserted; the struct must fill tmsize.
TMSIZE = 0x364
TEAM_DECL = [
    (0x000, 'uint16_t shots;', 'shots (tmshots)'),
    (0x002, 'uint16_t PwrGoals;', 'power play goals'),
    (0x004, 'uint16_t PwrPlays;', 'power plays'),
    (0x006, 'uint16_t Penalties;', 'penalties'),
    (0x008, 'uint16_t Penmin;', 'penalty minutes'),
    (0x00A, 'uint16_t ATOP;', 'attack zone time (seconds)'),
    (0x00C, 'uint16_t score;', ''),
    (0x00E, 'uint16_t faceoffs;', 'faceoffs won'),
    (0x010, 'uint16_t checks;', 'body checks'),
    (0x012, 'uint16_t passes;', 'pass attempts'),
    (0x014, 'uint16_t passcomp;', 'passes completed'),
    (0x016, 'uint16_t line;', 'the line on the ice'),
    (0x018, 'uint16_t lasttouch;', 'the last player to touch the puck (roster number, in the low byte)'),
    (0x01A, 'uint16_t assist1;', 'the two before him ($FFxx: none)'),
    (0x01C, 'uint16_t assist2;', ''),
    (0x01E, 'uint32_t data;', "the team's data (the cartridge's address)"),
    (0x022, 'uint16_t sort;', 'its first sort object (an address)'),
    (0x024, 'uint16_t ap;', 'active players on the ice, 4-6'),
    (0x026, 'uint16_t goalie;', 'the goalie (roster slot; $FFxx: pulled)'),
    (0x028, 'uint16_t team;', 'the team (TeamList index; the asm reads it as Xvel(a2))'),
    (0x02A, 'uint32_t neardist;', "distance squared from the puck to the team's nearest skater"),
    (0x02E, 'uint16_t lchoice;', 'the line the pad chose'),
    (0x030, 'uint8_t flags;', ''),
    (0x031, 'uint8_t _pad31;', ''),
    (0x032, 'uint16_t pde[26];', 'energy of each roster player'),
    (0x066, 'uint16_t pdst[26];', 'where each is: -2 bench, -1 ice, 0 and up the penalty box'),
    (0x09A, 'uint8_t penbox[26];', 'the penalty box: roster offsets (player * 2), ended by a negative byte'),
    (0x0B4, 'uint8_t pgoals[26];', "goals (a goalie's: goals against)"),
    (0x0CE, 'uint8_t passists[26];', 'assists'),
    (0x0E8, 'uint8_t pshots[26];', "shots (a goalie's: shots faced)"),
    (0x102, 'uint8_t ppim[26];', 'penalty minutes'),
    (0x11C, 'uint8_t pchecks[26];', 'body checks'),
    (0x136, 'uint16_t ptoi[26];', 'time on ice'),
    (0x16A, 'uint8_t lines[7][8];', 'the line sets: 7 lines of 8 slots (roster number + 1)'),
    (0x1A2, 'uint8_t hotcold[26][16];', "94: each player's hot / cold bytes (signed)"),
    (0x342, 'uint16_t pergoals[4];', 'goals by period'),
    (0x34A, 'uint16_t pershots[4];', 'shots by period'),
    (0x352, 'uint16_t pptime;', 'power play time'),
    (0x354, 'uint16_t ppshots;', 'power play shots'),
    (0x356, 'uint16_t shgoals;', 'shorthanded goals'),
    (0x358, 'uint16_t breakaways;', ''),
    (0x35A, 'uint16_t bagoals;', 'breakaway goals'),
    (0x35C, 'uint16_t onetimers;', 'one-timer attempts'),
    (0x35E, 'uint16_t otgoals;', 'one-timer goals'),
    (0x360, 'uint16_t psattempts;', 'penalty shot (and shootout) attempts'),
    (0x362, 'uint16_t psgoals;', 'penalty shot goals'),
]

# ---- the night's games and the playoff matchups (gsstruct, gssize $10 apart) --------------------
MATCHUP_DECL = [
    (0x0, 'uint16_t team1;', 'the teams (gst1, gst2: TeamList indexes; flags bit 0, the two flipped)'),
    (0x2, 'uint16_t team2;', ''),
    (0x4, 'uint16_t topwins;', "the playoff tree's top team's wins (gspotwins: best of 7, 4 wins it)"),
    (0x6, 'uint16_t botwins;', "the bottom team's (gspobwins)"),
    (0x8, 'uint16_t per;', 'the period (gsper: 0 not started, 1-3, 4 overtime, 5 final)'),
    (0xA, 'uint16_t score1;', "team1's score (gss1)"),
    (0xC, 'uint16_t score2;', "team2's (gss2)"),
    (0xE, 'uint8_t flags;', 'gsflags: MU_FLIPPED, MU_HIGHLIGHT, MU_SERIES_OVER'),
    (0xF, 'uint8_t _padF;', ''),
]

# ---- the instant replay's frames (updatereplay, RestoreReplayFrame: $62 bytes) -----------------
REPLAY_DECL = [
    (0x00, 'uint32_t obj[16];', 'each sorted object: x (bits 0-9), y (10-19), frame (20-29), its attribute bits 11-12 (30-31)'),
    (0x40, 'uint8_t players[6][3];', 'roster numbers and positions, two players in 3 bytes (a nibble a position, $F none)'),
    (0x52, 'uint8_t puckz;', "the puck's height"),
    (0x53, 'uint8_t shadowz;', "object 15's (its Zpos)"),
    (0x54, 'uint8_t lastsfx;', 'the sound effect started'),
    (0x55, 'uint8_t elapsed;', 'the frames it lasted'),
    (0x56, 'uint16_t padbits;', 'PadControlBits (the low 12 bits)'),
    (0x58, 'uint16_t crowdframe;', ''),
    (0x5A, 'uint16_t glovecords;', ''),
    (0x5C, 'uint8_t pbnum;', "PBnum's high byte"),
    (0x5D, 'uint8_t padbits34;', 'PadControlBits34 >> 4'),
    (0x5E, 'uint16_t hpos;', 'the view (Hpos, Vpos)'),
    (0x60, 'uint16_t vpos;', ''),
]

# ---- the sound driver's (sound94.asm) ------------------------------------------------------------
TRACK_DECL = [
    (0x0, 'uint32_t stream;', "the next event's address (negative: the slot is free)"),
    (0x4, 'uint16_t delay;', 'frames to it'),
]
CHANNEL_DECL = [
    (0x0, 'uint8_t key;', "the voice it plays (midi channel * 8 + track; $FF none)"),
    (0x1, 'uint8_t note;', ''),
    (0x2, 'uint8_t volume;', "the note's"),
    (0x3, 'uint8_t patch;', '$60 and up: a PCM sample'),
    (0x4, 'uint8_t out;', 'its output channel (0-2, 4-6)'),
    (0x5, 'uint8_t age;', 'frames since its key on, held at $FF'),
    (0x6, 'uint8_t flags;', 'bit 0: key on'),
    (0x7, 'uint8_t _pad7;', ''),
]
VOICE_DECL = [
    (0x0, 'int16_t bend;', 'pitch bend'),
    (0x2, 'uint8_t unused2;', '(cleared, never read)'),
    (0x3, 'uint8_t patch;', ''),
    (0x4, 'uint16_t volume;', 'controller 7 (its low byte)'),
    (0x6, 'uint16_t _pad6;', ''),
]

# ---- the menus' state (menuitem) ---------------------------------------------------------------
MENU_DECL = [
    (0x0, 'uint16_t item;', 'the item chosen'),
    (0x2, 'uint16_t top;', 'the first shown (SelectGoalieMenu: the last, its goalies)'),
]

# ---- the faceoff (fofdata2) and the puck's path (puckcross) ---------------------------------------
FOMAN_DECL = [
    (0x0, 'uint16_t frame;', ''),
    (0x2, 'uint16_t attribute;', ''),
]
CROSS_DECL = [
    (0x0, 'int16_t x;', 'where the puck crosses the goal line'),
    (0x2, 'uint16_t frames;', 'frames until it does ($FFFF: never; st puckcross2 sets the high byte)'),
]

# ---- the game's structures (NATIVE-STATE.md, step 3) -------------------------------------
# Each: its size, what it is, and its fields (offset, declaration, comment) at the 68000's offsets,
# which the generator checks follow one another and the C asserts. TYPED gives a RAM region one.
STRUCTS = {
    'Team': (TMSIZE, "a team's struct (team_struct(away))", 'tmsize', TEAM_DECL),
    'Matchup': (0x10, "one of the night's games, a playoff matchup in the playoffs (gsstruct)", 'gssize', MATCHUP_DECL),
    'ReplayFrame': (0x62, "one frame of the instant replay (updatereplay)", None, REPLAY_DECL),
    'TrackSlot': (6, "one of the sound driver's 8 track slots: a sound or song's event stream", None, TRACK_DECL),
    'Channel': (8, "one of the sound driver's 6 channels (the sixth: PCM)", None, CHANNEL_DECL),
    'Voice': (8, "the sound driver's voice of a key (midi channel * 8 + track)", None, VOICE_DECL),
    'MenuPos': (4, "a menu's state", None, MENU_DECL),
    'FoMan': (4, "a faceoff man's frame and attribute (fofdata2)", None, FOMAN_DECL),
    'PuckCross': (4, "where and when the puck crosses a goal line (findpc)", None, CROSS_DECL),
}
# RAM regions of the game's structures: the symbol, the field's declaration, and its comment
TYPED = {
    'tmstruct': ('struct Team teams[2]', 'the home and away team structs, AwShots the second'),
    'gsstruct': ('struct Matchup matchups[8]', "the night's games: the playoff tree's first round, then each round's"),
    'M68K_RAM': ('struct ReplayFrame replay[458]', "the instant replay's frames, a ring to replayend; between games the save RAM's copy, and the game's RAM kept during a highlight: those by address"),
    'fm_voice_usage_table': ('struct Voice fm_voices[64]', "the voice of each key"),
    'fm_channel_structs': ('struct Channel fm_channels[6]', "the channels (fm_channel_struct6 the sixth, PCM)"),
    'fm_track_slots': ('struct TrackSlot fm_slots[8]', "the track slots (track 7 the first)"),
    'fofdata2': ('struct FoMan fomen[3]', "the faceoff men: the two players, then the ref (fofdata)"),
    'puckcross': ('struct PuckCross puckcross[2]', "the puck's crossings of the top goal line, then the bottom's"),
    'menuitem': ('struct MenuPos menu', "the menu's state"),
    'mesarea': ('uint8_t mesarea[0x64]', "the message area, a String as the cartridge's (a big-endian length word counting itself, the text: TextBuffer)"),
    'pads': ('struct RinkObj pads[7]', "the pads' markers (Joy1Struct the second; the fourth the replay camera, ReplayStarStruct), then the gloves (glovestruct)"),
    'sso': ('struct SpriteHead sso[4]', "the controlled players' off-screen arrows (x, y at Xfrac's place)"),
}
SZ = {'uint8_t': 1, 'int8_t': 1, 'uint16_t': 2, 'int16_t': 2, 'uint32_t': 4, 'int32_t': 4}


def decl_size(decl):
    """(type, name, element size, element count) of a declaration such as 'uint8_t lines[7][8]'"""
    m = re.match(r'\s*(struct \w+|u?int(?:8|16|32)_t)\s+(\w+)((?:\[(?:0x[0-9A-Fa-f]+|\d+)\])*);?', decl)
    t = m.group(1)
    size = SZ[t] if t in SZ else (SIZES.get(t[7:]) or STRUCTS[t[7:]][0])
    count = 1
    for d in re.findall(r'\[(0x[0-9A-Fa-f]+|\d+)\]', m.group(3)):
        count *= int(d, 0)
    return t, m.group(2), size, count


# the sort object's head, in two: what the sprite code draws (addframe2, to $14: an sso arrow is
# one) and what is placed on the rink (addframe, to $1C: the pads, the gloves, the replay camera)
HEAD, RINK = 0x14, 0x1C
SIZES = {'SortObj': SCSTRUCT, 'SpriteHead': HEAD, 'RinkObj': RINK}
sort_at, at = [], 0
for l in sort_fields:
    t, n, size, count = decl_size(l)
    sort_at.append((at, l, n, size, count))
    at += size * count
if not ({a for a, *_ in sort_at} >= {HEAD, RINK}):
    raise SystemExit('the sort object has no field boundary at $14 or $1C')
struct_scalars = {}
for sname, limit in SIZES.items():
    struct_scalars[sname] = [(a + e * z, z, n) for a, l, n, z, c in sort_at for e in range(c)
                             if a + (e + 1) * z <= limit and not n.startswith('_pad')]
head_lines = [l for a, l, *_ in sort_at if a < HEAD]
rink_lines = [l for a, l, *_ in sort_at if HEAD <= a < RINK]
rest_lines = [l for a, l, *_ in sort_at if a >= RINK]
def indent(lines, k):
    return [' ' * (4 * k) + l for l in lines]
anon_head = ['union {', '    struct SpriteHead head;', '    struct RAM_PACKED {'] + indent(head_lines, 1) + ['    };', '};']
sort_text = (
    "/* the head of a sort object: what the sprite code draws (addframe2); sso's arrows are one */\n"
    'struct RAM_PACKED SpriteHead {\n' + '\n'.join(head_lines) + '\n};\n'
    f'_Static_assert(sizeof(struct SpriteHead) == 0x{HEAD:X}, "SpriteHead size");\n\n'
    "/* a drawn object placed on the rink (addframe): a sort object's first $1C bytes; the pads' markers,\n"
    " * the replay camera and the gloves are one */\n"
    'struct RAM_PACKED RinkObj {\n' + '\n'.join(indent(anon_head, 1)) + '\n' + '\n'.join(rink_lines) + '\n};\n'
    f'_Static_assert(sizeof(struct RinkObj) == 0x{RINK:X}, "RinkObj size");\n\n'
    '/* one sorted graphics object (players, puck, referee ...): SortCords entry; o->head and o->rink\n'
    ' * are its parts the drawing code takes */\n'
    'struct RAM_PACKED SortObj {\n    union {\n        struct RinkObj rink;\n        struct RAM_PACKED {\n'
    + '\n'.join(indent(anon_head, 3)) + '\n' + '\n'.join(indent(rink_lines, 2)) + '\n        };\n    };\n'
    + '\n'.join(rest_lines) + '\n};\n'
    '_Static_assert(sizeof(struct SortObj) == SCSTRUCT, "SortObj size");\n'
    + '\n'.join(f'_Static_assert(offsetof(struct SortObj, {n}) == 0x{a:02X}, "SortObj {n}");' for a, l, n, *_ in sort_at if not n.startswith('_pad'))
    + '\n')

struct_text = []
for sname, (ssize, sdoc, size_name, sdecl) in STRUCTS.items():
    lines, sasserts, at, flat = [], [], 0, []
    for off, decl, com in sdecl:
        if off != at:
            raise SystemExit(f'struct {sname}: {decl} at ${off:X}, but the fields before end at ${at:X}')
        t, fname, esize, count = decl_size(decl)
        lines.append('    ' + decl + ('  /* ' + com + ' */' if com else ''))
        if not fname.startswith('_pad'):
            sasserts.append(f'_Static_assert(offsetof(struct {sname}, {fname}) == 0x{off:03X}, "{sname} {fname}");')
            for e in range(count):
                if t in SZ:
                    flat.append((off + e * esize, esize, f'{fname}'))
                else:
                    flat += [(off + e * esize + o, z, f'{fname}.{n}') for o, z, n in struct_scalars[t[7:]]]
        at = off + esize * count
    if at != ssize:
        raise SystemExit(f'struct {sname}: fields end at ${at:X}, not its size ${ssize:X}')
    struct_scalars[sname] = flat
    size_def = f'#define {size_name} 0x{ssize:X}\n' if size_name else ''
    struct_text.append(f'{size_def}/* {sdoc} */\nstruct RAM_PACKED {sname} {{\n' + '\n'.join(lines) + '\n};\n'
                       f'_Static_assert(sizeof(struct {sname}) == 0x{ssize:X}, "{sname} size");\n' + '\n'.join(sasserts) + '\n')

# ---- RAM struct ---------------------------------------------------------------------------
fields, asserts = [], []
pos = 0
inram = [r for r in rows if RAM_BASE <= r['addr'] < RAM_BASE + RAM_SIZE]
inram.sort(key=lambda r: r['addr'])
for i, r in enumerate(inram):
    off = r['addr'] - RAM_BASE
    name = cident(r['name'])
    nxt = inram[i + 1]['addr'] - RAM_BASE if i + 1 < len(inram) else RAM_SIZE
    if off < pos:
        asserts.append(f'/* {name} at 0x{off:04X} overlaps the previous field; use RAM_OFF_{name} */')
        continue
    if off > pos:
        fields.append(f'    uint8_t _pad{pos:04X}[0x{off - pos:X}];')
        pos = off
    gap = nxt - off
    com = r.get('comment', '').replace('*/', '* /')
    cm = f'  /* ${r["addr"]:08X}{": " + com if com else ""} */'
    if r['name'] in TYPED:
        decl, tcom = TYPED[r['name']]
        _, name, esize, count = decl_size(decl)
        fields.append(f'    {decl};  /* ${r["addr"]:08X}: {tcom} ({r["name"]}) */')
        pos = off + esize * count
    elif name == 'SortCords':
        n = gap // SCSTRUCT
        fields.append(f'    struct SortObj SortCords[{n}];{cm}')
        pos = off + n * SCSTRUCT
        if pos < nxt:
            fields.append(f'    uint8_t _pad{pos:04X}[0x{nxt - pos:X}];'); pos = nxt
    elif r['name'] in BY_ADDRESS:
        name = name + '_'
        fields.append(f'    uint8_t {name}[0x{gap:X}];  /* ${r["addr"]:08X}: {BY_ADDRESS[r["name"]]} (RAM({r["name"]})) */')
        pos = off + gap
    else:
        w = OVERRIDE.get(r['name']) or pick_width(r['widths'], gap)
        size = WSIZE[w]
        if gap >= 2 * size and gap % size == 0 and gap > 4:
            fields.append(f'    {CTYPE[w]} {name}[0x{gap // size:X}];{cm}')
            pos = off + gap
        else:
            fields.append(f'    {CTYPE[w]} {name};{cm}')
            pos = off + size
    asserts.append(f'_Static_assert(offsetof(struct Ram, {name}) == 0x{off:04X}, "{name}");')
if pos < RAM_SIZE:
    fields.append(f'    uint8_t _pad{pos:04X}[0x{RAM_SIZE - pos:X}];')

CFILE = '''/* Generated by tools/gen_ram.py. Do not edit by hand.
 *
 * Work RAM itself, and the map from a machine byte to the host byte holding it. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "mem.h"

struct Ram g;

/* RAM's 16- and 32-bit scalars, as runs (first byte, count, size; the run again reps times, stride
 * bytes on: an array of structs): struct Ram's fields, and struct
 * SortObj's over the sorted objects, and the game's structs (src/ram.h) wherever RAM holds them */
static const struct { uint16_t at, n; uint8_t size; uint16_t reps, stride; } runs[] = {@RUNS@
};

uint16_t ram_host_[RAM_SIZE];

/* each machine byte's host byte: the same offset, but inside a scalar the host's byte order */
__attribute__((constructor)) static void ram_map(void)
{
    for (uint32_t a = 0; a < RAM_SIZE; a++) ram_host_[a] = (uint16_t)a;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (size_t r = 0; r < sizeof runs / sizeof runs[0]; r++)
        for (uint32_t j = 0; j < runs[r].reps; j++)
            for (uint32_t k = 0; k < runs[r].n; k++) {
                uint32_t at = runs[r].at + j * runs[r].stride + k * runs[r].size;
                for (uint32_t i = 0; i < runs[r].size; i++) ram_host_[at + i] = (uint16_t)(at + runs[r].size - 1 - i);
            }
#endif
}

void ram_pack(uint8_t out[RAM_SIZE])
{
    const uint8_t *b = (const uint8_t *)&g;
    for (uint32_t a = 0; a < RAM_SIZE; a++) out[a] = b[ram_host_[a]];
}

void ram_unpack(const uint8_t in[RAM_SIZE])
{
    uint8_t *b = (uint8_t *)&g;
    for (uint32_t a = 0; a < RAM_SIZE; a++) b[ram_host_[a]] = in[a];
}

/* (src/mem.h's ram_bytes on what is not a byte buffer: a bug, the bytes would not be the machine's) */
void ram_bytes_fail_(rp p, size_t n)
{
    fprintf(stderr, "ram_bytes: $%06X, %u bytes: not a byte buffer of RAM's\\n", (unsigned)rp_addr(p) & 0xFFFFFF, (unsigned)n);
    abort();
}
'''

# ---- the map: every scalar of RAM, as the game names it ------------------------------------
# struct SortObj's fields over the 16 sorted objects (bytes too: a byte read by name must not lie
# inside a word; SortCords, the puck and its shadow past the array), where struct Ram's fields count
# only as the game names them (a field's first element, as g.puckvz[0]: the rest of such an array
# is the object's own fields), which must agree. Then every scalar of struct Ram's fields, and of
# the game's structs in the regions typed by them (TYPED). No two views may disagree.
SORT_VIEWS = [('SortCords', 16, SCSTRUCT)]
def layout(lines):
    """(name, offset, size, count) of each declared field, in order"""
    out, at = [], 0
    for l in lines:
        m = re.match(r'\s+((?:struct \w+|u?int(?:8|16|32)_t)\s+\w+(?:\[(?:0x[0-9A-Fa-f]+|\d+)\])*;)', l)
        if m:
            t, n, size, count = decl_size(m.group(1))
            out.append((n, at, size, count, t))
            at += size * count
    return out


unit_of, conflicts = {}, []
def unit(start, size, what):
    for b in range(start, start + size):
        old = unit_of.get(b)
        if old and old[:2] != (start, size):
            conflicts.append(f'RAM ${b:04X}: {what} ({size} bytes at ${start:04X}) against {old[2]} ({old[1]} at ${old[0]:04X})')
            return
    for b in range(start, start + size):
        unit_of[b] = (start, size, what)


sortobj = layout(sort_fields)
base_of = {r['name']: r['addr'] - RAM_BASE for r in inram}
in_view = set()
for region, count, stride in SORT_VIEWS:
    in_view.update(range(base_of[region], base_of[region] + count * stride))
    for k in range(count):
        for n, at, size, cnt, _ in sortobj:
            for e in range(cnt):
                if at + (e + 1) * size <= stride:
                    unit(base_of[region] + k * stride + at + e * size, size, f'{region} {k} {n}')
for n, at, size, count, t in layout(fields):
    if t.startswith('struct ') and t != 'struct SortObj':  # (a region of the game's structures: every scalar)
        for k in range(count):
            for o, z, sn in struct_scalars[t[7:]]:
                unit(at + k * size + o, z, f'{n}[{k}].{sn}')
        continue
    if t == 'struct SortObj' or n.startswith('_pad'):
        continue                                          # (the sort objects: their view; padding: nothing names it)
    if at in in_view and n not in NAMED:
        continue                                          # (reached by address only: the view's layout)
    for k in range(1 if at in in_view else count):
        unit(at + k * size, size, n)
if conflicts:
    raise SystemExit('views of RAM that disagree:\n  ' + '\n  '.join(conflicts))
starts = sorted({(s, z) for s, z, _ in unit_of.values() if z > 1})
runs = []
for s, z in starts:
    if runs and runs[-1][2] == z and runs[-1][0] + runs[-1][1] * z == s:
        runs[-1][1] += 1
    else:
        runs.append([s, 1, z])
# a block of runs repeated at a stride (an array of structs): each run once, with its repeats
packed, i = [], 0
while i < len(runs):
    best = (1, 1, 0)                                      # (runs covered, block length, stride)
    for k in range(1, 9):
        if i + k >= len(runs):
            break
        stride = runs[i + k][0] - runs[i][0]
        reps = 1
        while all(i + reps * k + t < len(runs) and runs[i + reps * k + t] == [runs[i + t][0] + reps * stride, *runs[i + t][1:]]
                  for t in range(k)):
            reps += 1
        if reps >= 3 and reps * k > best[0]:
            best = (reps * k, k, stride)
    covered, k, stride = best
    packed += [[*runs[i + t], covered // k, stride] for t in range(k)]
    i += covered

offs = '\n'.join(f'#define RAM_OFF_{cident(r["name"])} 0x{r["addr"] - RAM_BASE:04X}' for r in inram)
sort_offs = '\n'.join(f'#define SO_{cident(n)} 0x{off:02X}' for n, off, _ in members)

hdr = f'''/* Generated by tools/gen_ram.py from the NHL94Genesis RAM map. Do not edit by hand.
 *
 * The Genesis work RAM ($FFFF0000-$FFFFFFFF) as one struct at the 68000's offsets (packed, no
 * alignment padding), its scalars in the host's byte order: native C, for any compiler. The game
 * reads it by its fields; src/mem.h reads and writes it by 68000 address through ram_host_, the
 * host byte of each machine byte (inside a 16- or 32-bit scalar the host's order: src/ram.c).
 * ram_pack makes the machine's image of it (the 68000's big-endian bytes) and ram_unpack takes
 * one back, for what compares or keeps RAM as the console has it: the oracle, the capture test,
 * lockstep, golden runs, save states.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define RAM_BASE 0x{RAM_BASE:X}u
#define RAM_SIZE 0x{RAM_SIZE:X}u
#define RAM_PACKED __attribute__((packed))

#define SCSTRUCT 0x{SCSTRUCT:X}
#define Sortobjs 16
#define puckscnum 14
{sort_text}
{chr(10).join(struct_text)}
struct RAM_PACKED Ram {{
{chr(10).join(fields)}
}};
_Static_assert(sizeof(struct Ram) == RAM_SIZE, "Ram size");
{chr(10).join(asserts)}

extern struct Ram g;

/* each machine byte's host byte (src/ram.c), and the machine's image of RAM made and taken back */
extern uint16_t ram_host_[RAM_SIZE];
void ram_pack(uint8_t out[RAM_SIZE]);
void ram_unpack(const uint8_t in[RAM_SIZE]);

/* sorted object n (the puck and its shadow lie past SortCords' 14) */
static inline struct SortObj *sortobj(int n) {{ return (struct SortObj *)((uint8_t *)&g + RAM_OFF_SortCords_PLACEHOLDER + n * SCSTRUCT); }}

/* byte offsets of every RAM symbol */
{offs}

/* sort object member offsets */
{sort_offs}
'''
hdr = hdr.replace('RAM_OFF_SortCords_PLACEHOLDER', f'0x{[r for r in inram if r["name"]=="SortCords"][0]["addr"] - RAM_BASE:04X}')
(P / 'src').mkdir(exist_ok=True)
(P / 'src/ram.h').write_text(hdr)
print(f'wrote src/ram.h: {len(fields)} fields, {len(members)} SortObj members, {len(asserts)} asserts')
rows_c = ''.join(('\n    ' if i % 4 == 0 else ' ') + '{ 0x%04X, %d, %d, %d, 0x%X },' % tuple(r) for i, r in enumerate(packed))
cfile = CFILE.replace('@RUNS@', rows_c)
(P / 'src/ram.c').write_text(cfile)
print(f'wrote src/ram.c: {sum(z for _, z in starts)} bytes in {len(starts)} scalars of 2 or 4, {len(packed)} runs')
