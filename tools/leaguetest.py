#!/usr/bin/env python3
"""Leagues (native/league.c), from the outside: make leaguetest.

  1. nhl94 --export-league writes the cartridge's league (out/leagues/cartridge), and
     build/leaguetest builds it again: it must be the cartridge's, byte for byte.
  2. A 32-team league made from it (out/leagues/thirty-two: four teams of the 1970s added before
     the All-Stars, two of them with no lines or colours, so they are made and borrowed, and two
     players of one wearing the same number) must build, its records apart in save RAM, every
     playoff team in a draw.
  3. Broken leagues must be refused, each saying what is wrong and where.
  4. Each hack ROM on the community's 32-team template beside the repository (../roms, or HACKS=)
     is extracted (nhl94 --extract-rom: read, never run) to out/leagues/hacks, must build to its
     teams, and plays a game.

tools/golden.py plays games with the first two (make golden-check). The files come from the
user's ROM, so they stay in out/ (never committed).
"""
import csv
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NHL94 = os.path.join(ROOT, 'build', 'nhl94')
ROM = os.path.join(ROOT, 'NHL 94 (UE) [!].gen')
CARTRIDGE = os.path.join(ROOT, 'out', 'leagues', 'cartridge')
THIRTY_TWO = os.path.join(ROOT, 'out', 'leagues', 'thirty-two')
HACKS = os.path.join(ROOT, 'out', 'leagues', 'hacks')

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
    r = subprocess.run([NHL94, '--rom', ROM, '--export-league', dst], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        sys.exit('--export-league failed: ' + r.stderr.strip())


def thirty_two(src=CARTRIDGE, dst=THIRTY_TWO):
    """the cartridge's league with ADDED before the All-Stars, no playoff draws given (made)"""
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    os.remove(os.path.join(dst, 'playoffs.csv'))
    with open(os.path.join(dst, 'league.ini'), 'w') as f:
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


# broken leagues: (name, the change to the cartridge's files, what the refusal must say)
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
    ('a picture', edit('pictures.csv', lambda t: cell(t, 1, 'picture', 'CardNobody')), 'picture: "CardNobody" is not one'),
    ('a draw', edit('playoffs.csv', lambda t: cell(t, 1, '2', 'BUF')), 'playoffs.csv, line 2: BUF twice in a draw'),
    ('too few teams', edit('teams.csv', lambda t: t.__delitem__(slice(18, None))), 'teams.csv: 17 teams'),
    ('no teams file', lambda d: os.remove(os.path.join(d, 'teams.csv')), 'teams.csv: not in'),
    ('a wide banner', edit('teams.csv', lambda t: cell(t, 1, 'banner', 'WWWWWWWWWW')), 'ANH: banner: "WWWWWWWWWW" is 110 pixels wide, a banner holds 82'),
    ('a banner letter', edit('teams.csv', lambda t: cell(t, 1, 'banner', 'A#B')), "ANH: banner: the banners have no letter '#'"),
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
    """each hack on the 32-team template extracted to out/leagues/hacks/NAME (those that are not, said
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
        print('  no hack ROMs on the 32-team template (../roms, or HACKS=path%s...): their leagues not tried' % os.pathsep)
    return out


def play_hacks(hacks):
    """a game with each hack's league (golden's full_game script, 16000 frames): it must play to the
    end. True when one does not"""
    bad = False
    for path, d in hacks:
        sram = os.path.join(HACKS, 'game.srm')
        if os.path.exists(sram):
            os.remove(sram)
        r = subprocess.run([NHL94, '--rom', ROM, '--headless', '--frames', '16000', '--league', d, '--sram', sram,
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
    args = [os.path.relpath(d, ROOT) for d in (CARTRIDGE, THIRTY_TWO)]
    for path, d in hacks:
        args += ['--hack', path, os.path.relpath(d, ROOT)]
    r = subprocess.run([os.path.join(ROOT, 'build', 'leaguetest'), ROM] + args, cwd=ROOT)
    bad = play_hacks(hacks) | (r.returncode != 0)
    base = os.path.join(ROOT, 'out', 'leagues', 'broken')
    refused = 0
    for name, change, says in BROKEN:
        d = os.path.join(base, name.replace(' ', '-'))
        if os.path.exists(d):
            shutil.rmtree(d)
        shutil.copytree(CARTRIDGE, d)
        change(d)
        r = subprocess.run([NHL94, '--rom', ROM, '--check-league', d], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        ok = r.returncode != 0 and says in r.stderr
        refused += ok
        if not ok:
            print('FAIL %s: %s' % (name, (r.stderr or r.stdout).strip() or 'built'))
    print('leaguetest: %d of %d broken leagues refused, saying what is wrong' % (refused, len(BROKEN)))
    shutil.rmtree(base)
    sys.exit(1 if bad or refused != len(BROKEN) else 0)


if __name__ == '__main__':
    main()
