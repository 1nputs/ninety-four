#!/usr/bin/env python3
"""Leagues (native/league.c), from the outside: make leaguetest.

  1. nhl94 --export-league writes the cartridge's league (out/leagues/cartridge), and
     build/leaguetest builds it again: it must be the cartridge's, byte for byte.
  2. A 32-team league made from it (out/leagues/thirty-two: four teams of the 1970s added before
     the All-Stars, two of them with no lines or colours, so they are made and borrowed) must
     build, its records apart in save RAM, every playoff team in a draw.
  3. Broken leagues must be refused, each saying what is wrong and where.

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
    ('a long name', edit('players.csv', lambda t: cell(t, 1, 'name', 'Steven King Of The North')), 'longer than 19 characters'),
    ('not ASCII', edit('players.csv', lambda t: cell(t, 1, 'name', 'Teemu Selänne')), 'only plain letters'),
    ('a rating', edit('players.csv', lambda t: cell(t, 1, 'speed', '16')), 'players.csv, line 2: speed: 16 is not from 0 to 15'),
    ('a number twice', edit('players.csv', lambda t: cell(t, 2, 'number', t[1][t[0].index('number')])), 'ANH: number 27 twice'),
    ('art', edit('teams.csv', lambda t: cell(t, 1, 'art', 'SEA')), 'art: SEA is not a cartridge team'),
    ('a colour', edit('colours.csv', lambda t: cell(t, 1, 'colours', '0EEE')), 'colours.csv, line 2: colours: 16 Genesis colour words'),
    ('a picture', edit('pictures.csv', lambda t: cell(t, 1, 'picture', 'CardNobody')), 'picture: "CardNobody" is not one'),
    ('a draw', edit('playoffs.csv', lambda t: cell(t, 1, '2', 'BUF')), 'playoffs.csv, line 2: BUF twice in a draw'),
    ('too few teams', edit('teams.csv', lambda t: t.__delitem__(slice(18, None))), 'teams.csv: 17 teams'),
    ('no teams file', lambda d: os.remove(os.path.join(d, 'teams.csv')), 'teams.csv: not in'),
    ('a wide banner', edit('teams.csv', lambda t: cell(t, 1, 'banner', 'WWWWWWWWWW')), 'ANH: banner: "WWWWWWWWWW" is 110 pixels wide, a banner holds 82'),
    ('a banner letter', edit('teams.csv', lambda t: cell(t, 1, 'banner', 'A#B')), "ANH: banner: the banners have no letter '#'"),
]


def main():
    if not os.path.exists(ROM):
        sys.exit('no %s here (link the ROM into the repository)' % os.path.basename(ROM))
    export()
    thirty_two()
    r = subprocess.run([os.path.join(ROOT, 'build', 'leaguetest'), ROM] + [os.path.relpath(d, ROOT) for d in (CARTRIDGE, THIRTY_TWO)], cwd=ROOT)
    bad = r.returncode != 0
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
