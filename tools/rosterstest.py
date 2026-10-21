#!/usr/bin/env python3
"""Rosters (native/rosters.c), from the outside: make rosterstest.

  1. nhl94 --export-rosters writes the cartridge's roster set (out/rosters/cartridge), and
     build/rosterstest builds it again: its tables must be the cartridge's as the game reads them
     off the cartridge, field for field, and its art the cartridge's.
  2. A 32-team roster set made from it (out/rosters/thirty-two: four teams of the 1970s added before
     the All-Stars, two of them with no lines or colours, so they are made and borrowed, and two
     players of one wearing the same number) must build, its records apart in save RAM, every
     playoff team in a draw.
  3. Broken roster sets must be refused, each saying what is wrong and where.
  4. Each hack ROM on the community's 32-team template beside the repository (../roms, or HACKS=)
     is extracted (nhl94 --extract-rom: read, never run) to out/rosters/hacks, must build to its
     teams and art, and plays a game.
  5. The 32-team roster set and the hacks' as packages (--pack-rosters: zips building the same roster set;
     the 32-team one's checked again from its zip), and damaged packages refused.
  6. The ratings' columns named as the game's roster screen names them, and a hack's ratings as its
     authors' own sheets beside it say (when it has them: the 2025 edition does).

tools/golden.py plays games with the first two (make golden-check). The files come from the
user's ROM, so they stay in out/ (never committed).
"""
import csv
import os
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NHL94 = os.path.join(ROOT, 'build', 'nhl94')
ROM = os.path.join(ROOT, 'NHL 94 (UE) [!].gen')
CARTRIDGE = os.path.join(ROOT, 'out', 'rosters', 'cartridge')
THIRTY_TWO = os.path.join(ROOT, 'out', 'rosters', 'thirty-two')
HACKS = os.path.join(ROOT, 'out', 'rosters', 'hacks')

# the teams added: (abbreviation, city, nickname, arena, conference, art, the team whose players
# they take, whether they keep its lines and colours)
ADDED = [
    ('SEA', 'Seattle', 'Totems', 'Seattle Center Coliseum', 'west', 'VAN', 'VAN', True),
    ('KC', 'Kansas City', 'Scouts', 'Kemper Arena', 'west', 'STL', 'STL', False),
    ('CLE', 'Cleveland', 'Barons', 'Richfield Coliseum', 'east', 'PIT', 'PIT', True),
    ('HOU', 'Houston', 'Aeros', 'The Summit', 'west', 'DAL', 'DAL', False),
]


def read(path):
    with open(path, newline='') as f:
        return list(csv.reader(f))


def write(path, rows):
    with open(path, 'w', newline='') as f:
        csv.writer(f, lineterminator='\n').writerows(rows)


def export(dst=CARTRIDGE):
    r = subprocess.run([NHL94, '--rom', ROM, '--export-rosters', dst], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        sys.exit('--export-rosters failed: ' + r.stderr.strip())


def thirty_two(src=CARTRIDGE, dst=THIRTY_TWO):
    """the cartridge's roster set with ADDED before the All-Stars, no playoff draws given (made)"""
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    os.remove(os.path.join(dst, 'playoffs.csv'))
    with open(os.path.join(dst, 'rosters.ini'), 'w') as f:
        f.write('name = Thirty-two teams\nplayoff_teams = 30\n')
    teams = read(os.path.join(dst, 'teams.csv'))
    head, rows = teams[0], teams[1:]
    stars = [r for r in rows if r[0] in ('ASE', 'ASW')]
    rows = [r for r in rows if r[0] not in ('ASE', 'ASW')]
    for abbr, city, nick, arena, conf, art, _, _ in ADDED:
        rows.append([abbr, city, nick, arena, conf, art] + [''] * (len(head) - 6))   # (the rest: the art team's)
    write(os.path.join(dst, 'teams.csv'), [head] + rows + stars)
    for name in ('goalies.csv', 'players.csv', 'lines.csv', 'colours.csv'):
        table = read(os.path.join(dst, name))
        added = []
        for abbr, _, _, _, _, _, source, keep in ADDED:
            if name in ('lines.csv', 'colours.csv') and not keep:
                continue
            added += [[abbr] + r[1:] for r in table[1:] if r[0] == source]
        write(os.path.join(dst, name), table + added)
    share_number(dst, 'CLE', 1, 2)                         # (two wear one number: named in its lines)
    own_art(dst)


def share_number(d, team, first, second, name_first=True):
    """team's skaters first and second (their rows of its players in players.csv) wear one number:
    lines.csv names the second by his name, and the first too when name_first"""
    players = read(os.path.join(d, 'players.csv'))
    rows = [r for r in players[1:] if r[0] == team]
    num, name = players[0].index('number'), players[0].index('name')
    a, b = rows[first - 1], rows[second - 1]
    renamed = {b[num]: b[name]}
    if name_first:
        renamed[a[num]] = a[name]
    b[num] = a[num]
    write(os.path.join(d, 'players.csv'), players)
    lines = read(os.path.join(d, 'lines.csv'))
    for r in lines[1:]:
        if r[0] == team:
            r[2:] = [renamed.get(c, c) for c in r[2:]]
    write(os.path.join(d, 'lines.csv'), lines)


def png_rgb(path, w, h, pixel):
    """a truecolour PNG with alpha (as any editor writes): pixel(x, y) -> (r, g, b, a)"""
    raw = b''.join(b'\0' + b''.join(bytes(pixel(x, y)) for x in range(w)) for y in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def png_poke(path, x, y, index):
    """one pixel of an indexed PNG as nhl94 writes them (8 bits, filter 0) set to index"""
    d = bytearray(open(path, 'rb').read())
    o, w, out = 8, 0, b''
    while o < len(d):
        n = struct.unpack('>I', d[o:o + 4])[0]
        t, c = bytes(d[o + 4:o + 8]), bytes(d[o + 8:o + 8 + n])
        if t == b'IHDR':
            w = struct.unpack('>I', c[:4])[0]
        if t == b'IDAT':
            raw = bytearray(zlib.decompress(c))
            raw[y * (w + 1) + 1 + x] = index
            c = zlib.compress(bytes(raw))
            out += struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xFFFFFFFF)
        else:
            out += bytes(d[o:o + 12 + n])
        o += 12 + n
    open(path, 'wb').write(bytes(d[:8]) + out)


# the art the 32-team roster set draws its own: HOU a logo in plain RGB (a corner transparent, red,
# white), KC a photo in colours (a ramp of greys: the shades by brightness), BOS its logo with one
# pixel changed, the players' sprites with one pixel changed
def own_art(dst):
    png_rgb(os.path.join(dst, 'art', 'logos', 'HOU.png'), 48, 48,
            lambda x, y: (0, 0, 0, 0) if x < 8 and y < 8 else (255, 0, 0, 255) if x < 24 else (255, 255, 255, 255))
    png_rgb(os.path.join(dst, 'art', 'photos', 'KCScout.png'), 48, 48, lambda x, y: (x * 255 // 47, x * 255 // 47, x * 200 // 47, 255))
    with open(os.path.join(dst, 'pictures.csv'), 'a') as f:
        f.write('KC,%s,KCScout\n' % read(os.path.join(dst, 'players.csv'))[1:][[r[0] for r in read(os.path.join(dst, 'players.csv'))[1:]].index('KC')][2])
    png_poke(os.path.join(dst, 'art', 'logos', 'BOS.png'), 0, 0, 1)
    png_poke(os.path.join(dst, 'art', 'game', 'Sprites.png'), 0, 0, 3)


def rink_twice(d):
    """the rink's picture changed at one place a tile it shows twice is (the cartridge's rink map, read
    off the ROM): the first pixel of the tile's second place"""
    rom = open(ROM, 'rb').read()
    w16 = lambda a: struct.unpack('>H', rom[a:a + 2])[0]
    w32 = lambda a: struct.unpack('>I', rom[a:a + 4])[0]
    base = 0x5605A                                          # (Rinktilelist)
    m = base + w32(base + 4)
    w, h = w16(m), w16(m + 2)
    seen = {}
    for c in range(w * h):
        word = w16(m + 4 + 2 * c)
        k = word & 0x7FF
        if k in seen and not word & 0x1800:                # (a second place, not flipped)
            x, y = c % w * 8, c // w * 8
            path = os.path.join(d, 'art', 'game', 'Rinktilelist.png')
            png_poke(path, x, y, (word >> 13 & 3) * 16 + (5 if png_peek(path, x, y) % 16 != 5 else 6))
            return
        seen[k] = c


def png_peek(path, x, y):
    d = open(path, 'rb').read()
    o, w = 8, 0
    while o < len(d):
        n = struct.unpack('>I', d[o:o + 4])[0]
        if d[o + 4:o + 8] == b'IHDR':
            w = struct.unpack('>I', d[o + 8:o + 12])[0]
        if d[o + 4:o + 8] == b'IDAT':
            return zlib.decompress(d[o + 8:o + 8 + n])[y * (w + 1) + 1 + x]
        o += 12 + n


# broken roster sets: (name, the change to the cartridge's files, what the refusal must say)
def edit(name, fn):
    def apply(d):
        p = os.path.join(d, name)
        rows = read(p)
        fn(rows)
        write(p, rows)
    return apply


def cell(rows, r, col, value):
    rows[r][rows[0].index(col)] = value


BROKEN = [
    ('team twice', edit('teams.csv', lambda t: t.insert(3, list(t[1]))), 'teams.csv, line 4: team ANH twice'),
    ('no such team', edit('players.csv', lambda t: cell(t, 5, 'team', 'XYZ')), 'players.csv, line 6: no team XYZ'),
    ('a line names nobody', edit('lines.csv', lambda t: cell(t, 1, 'C', '98')), 'lines.csv, line 2: ANH C: no number 98'),
    ('one goalie', edit('goalies.csv', lambda t: t.pop(2)), 'goalies.csv: ANH has 1 goalies: 2 to 4'),
    ('a long name', edit('players.csv', lambda t: cell(t, 1, 'name', 'Steven King Of The North Pole')), 'longer than 24 characters'),
    ('not ASCII', edit('players.csv', lambda t: cell(t, 1, 'name', 'Teemu Selänne')), 'only plain letters'),
    ('a rating', edit('players.csv', lambda t: cell(t, 1, 'speed', '16')), 'players.csv, line 2: speed: 16 is not from 0 to 15'),
    ('a shared number', lambda d: share_number(d, 'ANH', 1, 2, name_first=False), 'lines.csv, line 2: ANH X: 2 players wear 27: name him instead'),
    ('art', edit('teams.csv', lambda t: cell(t, 1, 'art', 'SEA')), 'art: SEA is not a cartridge team'),
    ('a colour', edit('colours.csv', lambda t: cell(t, 1, 'colours', '0EEE')), 'colours.csv, line 2: colours: 16 Genesis colour words'),
    ('a draw', edit('playoffs.csv', lambda t: cell(t, 1, '2', 'BUF')), 'playoffs.csv, line 2: BUF twice in a draw'),
    ('too few teams', edit('teams.csv', lambda t: t.__delitem__(slice(18, None))), 'teams.csv: 17 teams'),
    ('no teams file', lambda d: os.remove(os.path.join(d, 'teams.csv')), 'teams.csv: not in'),
    ('a wide banner', edit('teams.csv', lambda t: cell(t, 1, 'banner', 'WWWWWWWWWW')), 'ANH: banner: "WWWWWWWWWW" is 110 pixels wide, a banner holds 82'),
    ('a banner letter', edit('teams.csv', lambda t: cell(t, 1, 'banner', 'A#B')), "ANH: banner: the banners have no letter '#'"),
    ('a logo\'s size', lambda d: png_rgb(os.path.join(d, 'art', 'logos', 'BUF.png'), 50, 48, lambda x, y: (0, 0, 0, 255)),
     'art/logos/BUF.png: 50 x 48 pixels; it must be 48 x 48'),
    ('a logo\'s colours', lambda d: png_rgb(os.path.join(d, 'art', 'logos', 'BUF.png'), 48, 48, lambda x, y: (x * 5, y * 5, 0, 255)),
     'art/logos/BUF.png: more than 15 colours'),
    ('a banner\'s colour', lambda d: png_rgb(os.path.join(d, 'art', 'banners', 'CGY.png'), 88, 32, lambda x, y: (1, 200, 3, 255)),
     'art/banners/CGY.png: the colour #01C803 at 0, 0 is none of its palette\'s'),
    ('not a PNG', lambda d: open(os.path.join(d, 'art', 'ice', 'CHI.png'), 'wb').write(b'GIF89a not a PNG'), 'art/ice/CHI.png: not a PNG'),
    ('a shared picture\'s size', lambda d: png_rgb(os.path.join(d, 'art', 'game', 'TitleScreenImg.png'), 10, 10, lambda x, y: (0, 0, 0, 255)),
     'art/game/TitleScreenImg.png: 10 x 10 pixels; it must be 256 x 224'),
    ('a tile two ways', rink_twice, 'Rinktilelist: the picture differs at'),
    ('a photo missing', edit('pictures.csv', lambda t: cell(t, 1, 'picture', 'CardNoSuchPlayer')),
     'pictures.csv, line 2: picture: "CardNoSuchPlayer" is neither a photo of the folder\'s'),
]


def hack_roms():
    """the hack ROMs to try: HACKS (paths), else those beside the repository in ../roms"""
    if os.environ.get('HACKS'):
        return [p for p in os.environ['HACKS'].split(os.pathsep) if p]
    found = []
    for top, _, names in os.walk(os.path.join(os.path.dirname(ROOT), 'roms')):
        for n in sorted(names):
            p = os.path.join(top, n)
            if n.lower().endswith(('.bin', '.gen', '.md')) and 0x100000 < os.path.getsize(p) <= 0x400000:
                found.append(p)
    return sorted(found)


def extract_hacks():
    """each hack on the 32-team template extracted to out/rosters/hacks/NAME (those that are not, said
    and left out): [(path, folder)]"""
    out = []
    for path in hack_roms():
        name = '-'.join(''.join(c if c.isalnum() else ' ' for c in os.path.splitext(os.path.basename(path))[0].lower()).split())
        d = os.path.join(HACKS, name)
        if os.path.exists(d):
            shutil.rmtree(d)
        r = subprocess.run([NHL94, '--rom', ROM, '--extract-rom', path, d], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if r.returncode:
            print('  %s: not extracted: %s' % (os.path.basename(path), r.stderr.strip()))
            continue
        out.append((path, d))
    if not out:
        print('  no hack ROMs on the 32-team template (../roms, or HACKS=path%s...): their roster sets not tried' % os.pathsep)
    return out


def pack(hacks):
    """the 32-team roster set and each hack's as packages (nhl94 --pack-rosters: the package must build the
    folder's roster set, checked by nhl94), and damaged packages refused. True when all is well"""
    ok = True
    for d in [THIRTY_TWO] + [h[1] for h in hacks]:
        r = subprocess.run([NHL94, '--rom', ROM, '--pack-rosters', d, d + '.zip'], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        print('  ' + (r.stdout.strip().replace(ROOT + '/', '') if r.returncode == 0 else 'FAIL --pack-rosters %s: %s' % (d, r.stderr.strip())))
        ok &= r.returncode == 0
    z = open(THIRTY_TWO + '.zip', 'rb').read()
    broken = os.path.join(ROOT, 'out', 'rosters', 'broken.zip')
    for what, data, says in [('cut short', z[:len(z) // 2], 'not a zip archive'),
                             ('a file damaged', z[:200] + bytes([z[200] ^ 0x55]) + z[201:], 'damaged')]:
        open(broken, 'wb').write(data)
        r = subprocess.run([NHL94, '--rom', ROM, '--check-rosters', broken], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        refused = r.returncode != 0 and says in r.stderr
        print('  a package %s: %s' % (what, 'refused' if refused else 'FAIL: ' + (r.stderr or r.stdout).strip()))
        ok &= refused
    os.remove(broken)
    return ok


# the ratings' columns as the game's roster screen names them (native/rosters.c)
SKATER_COLS = ['weight', 'agility', 'speed', 'off_awareness', 'def_awareness', 'shot_power', 'checking', 'handed_fighting',
               'stick_handling', 'shot_accuracy', 'endurance', 'shoot_or_pass', 'pass_accuracy', 'aggressiveness']
GOALIE_COLS = ['weight', 'agility', 'speed', 'off_awareness', 'def_awareness', 'puck_control', 'unused1', 'glove_hand',
               'unused2', 'unused3', 'stick_right', 'stick_left', 'glove_right', 'glove_left']


def headers():
    """the exported players' and goalies' columns as the game names them. True when they are"""
    ok = read(os.path.join(CARTRIDGE, 'players.csv'))[0] == ['team', 'pos', 'number', 'name'] + SKATER_COLS \
        and read(os.path.join(CARTRIDGE, 'goalies.csv'))[0] == ['team', 'number', 'name'] + GOALIE_COLS + ['start']
    print('  the ratings\' columns %s' % ('named as the game names them' if ok else 'FAIL: not as the game names them'))
    return ok


def authors_sheets(hacks):
    """a hack's ratings against its authors' own sheets beside it (*Ratings - Players.csv: Goalie or Player,
    First, Last, Team, Pos, JNo, the 14 ratings in the game's order; *Ratings - Teams.csv: Team, Overall, Home
    Bonus, Away Penalty, Offence, Defence, PP Bonus, PK Penalty), when it has them. True when all agree"""
    ok = True
    for path, d in hacks:
        top = os.path.dirname(path)
        sheets = [os.path.join(r, n) for r, _, ns in os.walk(top) for n in ns if n.endswith('.csv')] if top != os.path.dirname(ROOT) + '/roms' else []
        players = [p for p in sheets if p.endswith('Ratings - Players.csv')]
        teams = [p for p in sheets if p.endswith('Ratings - Teams.csv')]
        if not players and not teams:
            continue
        ours = {}
        for name, cols in (('players.csv', SKATER_COLS), ('goalies.csv', GOALIE_COLS)):
            rows = read(os.path.join(d, name))
            for r in rows[1:]:
                row = dict(zip(rows[0], r))
                ours.setdefault((row['team'], int(row['number'])), []).append((row['name'], [int(row[c]) for c in cols]))
        same = differ = 0
        for p in players:
            for r in read(p):
                if len(r) < 20 or r[0] in ('Goalie', 'Player'):
                    continue
                found = [v for n, v in ours.get((r[3], int(r[5])), []) if n == (r[1] + ' ' + r[2]).strip()] or [v for n, v in ours.get((r[3], int(r[5])), [])]
                good = len(found) >= 1 and found[0] == [int(x) for x in r[6:20]]
                same += good
                differ += not good
        tsame = tdiffer = 0
        ot = {r[0]: r for r in read(os.path.join(d, 'teams.csv'))[1:]}
        for p in teams:
            for r in read(p)[1:]:
                o = ot.get(r[0])
                good = o is not None and [o[6], o[11], o[12], o[7], o[8], o[9], o[10]] == r[1:8]
                tsame += good
                tdiffer += not good
        print('  %s: as its authors\' sheets: %d of %d players\' ratings, %d of %d teams\'' %
              (os.path.relpath(d, ROOT), same, same + differ, tsame, tsame + tdiffer))
        ok &= differ == 0 and tdiffer == 0
    return ok


def play_hacks(hacks):
    """a game with each hack's roster set (golden's full_game script, 16000 frames): it must play to the
    end. True when one does not"""
    bad = False
    for path, d in hacks:
        sram = os.path.join(ROOT, 'out', 'rosterstest-game.srm')
        if os.path.exists(sram):
            os.remove(sram)
        r = subprocess.run([NHL94, '--rom', ROM, '--headless', '--frames', '16000', '--rosters', d, '--sram', sram,
                            '--input', os.path.join(ROOT, 'tools', 'oracle', 'scripts', 'full_game_30s.txt')],
                           cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        print('  %s: a game of 16000 frames %s' % (os.path.relpath(d, ROOT), 'played' if r.returncode == 0 else 'FAILED: ' + r.stderr.strip()[-300:]))
        bad |= r.returncode != 0
    return bad


def main():
    if not os.path.exists(ROM):
        sys.exit('no %s here (link the ROM into the repository)' % os.path.basename(ROM))
    export()
    thirty_two()
    hacks = extract_hacks()
    packages = pack(hacks)
    args = [os.path.relpath(d, ROOT) for d in (CARTRIDGE, THIRTY_TWO, THIRTY_TWO + '.zip')]
    listing = os.path.join(ROOT, 'out', 'rosters', 'listing')        # (a folder with rosters/ as the window's)
    if os.path.exists(listing):
        shutil.rmtree(listing)
    shutil.copytree(THIRTY_TWO, os.path.join(listing, 'rosters', 'thirty-two'))
    for z in [THIRTY_TWO + '.zip'] + [h[1] + '.zip' for h in hacks]:     # (packages, as the window lists them)
        if os.path.exists(z):
            shutil.copy(z, os.path.join(listing, 'rosters', 'packed-' + os.path.basename(z)))
    if os.path.exists(THIRTY_TWO + '.zip'):                              # (one beside the game, not in rosters/: found by Files)
        shutil.copy(THIRTY_TWO + '.zip', os.path.join(listing, 'beside.zip'))
    os.makedirs(os.path.join(listing, 'rosters', 'broken'))              # (one that will not read: left out, and said)
    open(os.path.join(listing, 'rosters', 'broken', 'teams.csv'), 'w').write('team\n')
    args += ['--listed', os.path.relpath(listing, ROOT), 'thirty-two']
    for path, d in hacks:
        args += ['--hack', path, os.path.relpath(d, ROOT)]
    r = subprocess.run([os.path.join(ROOT, 'build', 'rosterstest'), ROM] + args, cwd=ROOT)
    bad = play_hacks(hacks) | (r.returncode != 0) | (not packages) | (not headers()) | (not authors_sheets(hacks))
    base = os.path.join(ROOT, 'out', 'rosters', 'broken')
    refused = 0
    for name, change, says in BROKEN:
        d = os.path.join(base, name.replace(' ', '-'))
        if os.path.exists(d):
            shutil.rmtree(d)
        shutil.copytree(CARTRIDGE, d)
        change(d)
        r = subprocess.run([NHL94, '--rom', ROM, '--check-rosters', d], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        ok = r.returncode != 0 and says in r.stderr
        refused += ok
        if not ok:
            print('FAIL %s: %s' % (name, (r.stderr or r.stdout).strip() or 'built'))
    print('rosterstest: %d of %d broken roster sets refused, saying what is wrong' % (refused, len(BROKEN)))
    shutil.rmtree(base)
    sys.exit(1 if bad or refused != len(BROKEN) else 0)


if __name__ == '__main__':
    main()
