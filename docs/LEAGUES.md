# Leagues

A league is who plays: the teams, their players and ratings, lines, colours, logos, banners, centre
ice, the players' photos, and if you like the art every team shares (title screens, rinks, sprites,
fonts and the rest). The game plays the cartridge's own league unless you choose another on the
menu's **League** page. A league is a folder of text files and pictures, so a spreadsheet and a
paint program are all you need to make one, or it can be taken from a community hack ROM. Either
way it can travel as one zip file.

- [Quick start](#quick-start)
- [Where leagues live](#where-leagues-live)
- [The folder](#the-folder)
- [The files](#the-files): [league.ini](#leagueini), [teams.csv](#teamscsv),
  [goalies.csv and players.csv](#goaliescsv-and-playerscsv), [lines.csv](#linescsv),
  [colours.csv](#colourscsv), [pictures.csv](#picturescsv), [playoffs.csv](#playoffscsv)
- [The art](#the-art)
- [Leagues from hack ROMs](#leagues-from-hack-roms)
- [Packages](#packages)
- [Records, saved states and online play](#records-saved-states-and-online-play)
- [The League page](#the-league-page)
- [When something is wrong](#when-something-is-wrong)

## Quick start

Run these from the folder you play from (the game looks for `leagues/` there). Each needs the
cartridge: `--rom FILE`, or your dump in that folder as `NHL 94 (UE) [!].gen`.

```
# start from the cartridge's own league, to change as you like
./build/nhl94 --rom "NHL 94.gen" --export-league leagues/my-league

# or take a hack's league (read as data, never run)
./build/nhl94 --rom "NHL 94.gen" --extract-rom "NHL 2026 - 94 Edition v5.0S.bin" leagues/nhl2026

# check it: what holds, or what is wrong and on which line
./build/nhl94 --rom "NHL 94.gen" --check-league leagues/my-league

# make it one file to keep or send
./build/nhl94 --rom "NHL 94.gen" --pack-league leagues/my-league my-league.zip
```

Then start the game, press **Esc**, open **League**, pick it with Left / Right and choose **Reset**.
A league takes effect at the next power on, so a game in progress is never changed under you.

## Where leagues live

In `leagues/`, beside the game (the folder you run it from). Each league is a folder,
`leagues/NAME/`, or a package, `leagues/NAME.zip`, which plays as it is with no unpacking. The
name is how the game, the settings file (`rules.league = NAME`) and its records know it; the name
the menu shows is the one in its `league.ini`. `cartridge` is the cartridge's own and cannot be a
folder's name. The menu lists up to 15 leagues besides the cartridge's.

A league in `leagues/` that will not read is left off the list. The League page's note says how
many were left out, and the terminal the game was started from says why ("... (not in the menu)").

## The folder

```
leagues/my-league/
  league.ini          its name, how many teams play for the cup (optional)
  teams.csv           the teams (needed)
  goalies.csv         the goalies (needed)
  players.csv         the skaters (needed)
  lines.csv           the lines (optional: made from the roster's order)
  colours.csv         the teams' colours (optional: their art teams')
  pictures.csv        which player cards have photos (optional)
  playoffs.csv        the playoff draws (optional: made from the conferences)
  art/                pictures of the league's own (all optional)
    logos/ABBR.png      a team's logo
    banners/ABBR.png    a team's name banners
    ice/ABBR.png        a team's centre ice
    photos/NAME.png     a player's photo
    game/NAME.png       art every team shares (NAME.bin beside it: the hack's own bytes)
```

`ABBR` is a team's abbreviation from `teams.csv`; `NAME` in `photos/` is the name
`pictures.csv` gives it; `NAME` in `game/` is the game's own name for the asset (below).

## The files

The CSV files open in any spreadsheet. The rules for all of them:

- The first row names the columns. Columns may come in any order; columns the game does not know
  are ignored, so you can keep notes in your own.
- Empty rows, and rows whose first cell starts with `#`, are ignored.
- Text is plain ASCII; `\xNN` writes any other byte. A cell with a comma in it goes in double quotes.
- Files name a team by its abbreviation (the `team` column of `teams.csv`), and a player by his
  number, or by his name where two of a team's players wear one number.

### league.ini

```
name = My League
playoff_teams = 26
hiscore_screen = 14,3 16,16
```

| Key | |
|---|---|
| `name` | the name the menu and the League page show (at most 63 letters). Missing: the folder's name |
| `playoff_teams` | how many teams play for the cup: the first that many in `teams.csv` (18 up to all of them). The rest, like the cartridge's All-Star teams, play single games only. Missing: all of them |
| `hiscore_screen` | where the screen after the EA logo puts its two pictures: column,row (0-39, 0-27) of its logo, then of `HiScoreImg`. Missing: the cartridge's 14,3 16,16. The hacks draw a whole screen there, at 0,0 0,0 |

Lines starting with `#` or `;` are comments.

### teams.csv

A row a team, in the order the game lists them (the first `playoff_teams` play for the cup).
A league has 18 to 32 teams.

| Column | | Range |
|---|---|---|
| `team` | its abbreviation: how the other files name it (unique) | 1-3 letters |
| `city` | the city, on the setup screen and the banners | up to 17 letters |
| `nickname` | the team's name: Penguins, Oilers | up to 14 |
| `arena` | the arena the announcer names | up to 26 |
| `conference` | the side of the playoff draws it plays on, when `playoffs.csv` gives none | up to 15 |
| `art` | the cartridge team whose logo, banners and colours it shows where it has none of its own: `ANH BOS BUF CGY CHI DAL DET EDM FLA HFD LA MTL NJ NYI NYR OTW PHI PIT QUE SJ STL TB TOR VAN WSH WPG ASE ASW` | (its own abbreviation when empty) |
| `rating` | the team rating its cards and the matchups show | 0-99 |
| `offense`, `defense` | how its other playoff games are simulated | 0-7 |
| `pp`, `pk` | its players' bonus on the power play, and killing a penalty | 0-15 |
| `home`, `away` | its players' bonus at home, and away | 0-15 |
| `songs` | the organ's 6 songs, numbers with spaces between | 0-255 each |
| `banner` | the name on its banners when not its city (letters, digits, `.`, `-`, spaces) | up to 16 |

An empty number takes the art team's. A team whose city is not its art team's gets banners of its
own, its city in the cartridge's own lettering on its art team's colours, unless it has a picture
of its own for them (`art/banners`) or a `banner` name.

### goalies.csv and players.csv

A row a player. `goalies.csv`: `team`, `number`, `name`, the 14 ratings, `start`.
`players.csv`: `team`, `pos`, `number`, `name`, the 14 ratings.

| Column | | Range |
|---|---|---|
| `team` | his team's abbreviation | |
| `pos` | (skaters) `F` or `D`; `C`, `LW`, `RW`, `LD` and `RD` too | |
| `number` | his number | 0-99 |
| `name` | his name (the roster screen has room for about 19 letters) | up to 24 letters |
| `start` | (goalies) how often he starts | 1-15 |

The 14 ratings, 0 to 15 each, named as the game's roster screen labels them:

| Skaters | Goalies |
|---|---|
| `weight` | `weight` |
| `agility` | `agility` |
| `speed` | `speed` |
| `off_awareness` | `off_awareness` (shown by no screen) |
| `def_awareness` | `def_awareness` |
| `shot_power` | `puck_control` |
| `checking` | `unused1` (used by nothing) |
| `handed_fighting` (bit 0: right handed; bits 1-3: fighting) | `glove_hand` |
| `stick_handling` | `unused2` (used by nothing) |
| `shot_accuracy` | `unused3` (used by nothing) |
| `endurance` | `stick_right` |
| `shoot_or_pass` (how often the computer shoots rather than passes; the community calls it roughness) | `stick_left` |
| `pass_accuracy` | `glove_right` |
| `aggressiveness` | `glove_left` |

A team has 2 to 4 goalies, 9 to 15 forwards, 6 to 15 defensemen, and 26 players at most. The
game's overall rating for a player (on his card, and the League page's) is worked out from these
the way the cartridge does.

### lines.csv

A row a team's line: `team`, `line`, then the roster place of each position: `G`, `LD`, `RD`,
`LW`, `C`, `RW`, `X` (the extra attacker). The lines are `SC1`, `SC2`, `CHK`, `PP1`, `PP2`, `PK1`,
`PK2` and `L8`. Each cell is a player's number, or his name where two share a number; empty is
nobody. A team with no lines gets them made from its roster's order.

### colours.csv

A row a palette: `team`, `palette`, `colours`. The palettes are `home` and `away` (the jerseys,
and the players' sprites in them), `logo` (the logo's own), `matchup` and `matchup2` (the matchup
screens). `colours` is 16 Genesis colour words with spaces between, the way the console stores
colours (`0BGR`, each of B, G and R an even digit 0-E): `0EEE 0666 0000 024E ...`. A palette not
given is the art team's. A logo picture of the team's own brings its own `logo` colours.

### pictures.csv

Which players' cards have photos: `team`, `number` (or the player's name), `picture`. `picture`
names a photo of the league's own (`art/photos/NAME.png`), or one of the cartridge's by its label
(`CardMarioLemieux`, `NoPicSkater1` ...). A team has up to 32. A player with none gets the
cartridge's blank card.

### playoffs.csv

The draws a playoff starts from, a row of 16 team abbreviations each: the first 8 one side of the
bracket, the last 8 the other. Give a power of two rows (1, 2, 4 ... 32); the game picks one.
Without the file, draws are made from the playoff teams' conferences (two of them, 8 or more
teams each; else from all of them).

## The art

Everything under `art/` is optional: a team with no picture shows its art team's. The pictures are
PNGs, written by `--export-league` and `--extract-rom` in the colours they show in, so you can draw
over them in any editor.

| Picture | Size | Colours |
|---|---|---|
| `logos/ABBR.png` | 48 x 48 | the team's own 15, and 0 for clear |
| `banners/ABBR.png` | 88 x 32 | the top half: the playoff bracket's and the game's top bar's banner, colours 0-63 (four lines of 16, one line to each 8 x 8 cell); the bottom half: the setup screen's banner, colours 64-79 (65-68 are the team's own) |
| `ice/ABBR.png` | 48 x 32 | the rink's colours; only the centre ice is read back, the rest shows where it goes |
| `photos/NAME.png` | 48 x 48 | 8 shades of grey (a picture in colour is turned into them by its brightness) |
| `game/NAME.png` | the asset's own | the asset's own |

How a picture is read back: an indexed PNG keeps each pixel's palette place, so an editor that
leaves the palette as it is (most do) changes nothing you did not draw. A PNG that is not indexed,
or whose palette changed, is matched colour by colour to the colours that screen has; when a pixel
is none of them, `--check-league` says which colour and where.

`game/` holds the art every team shares, by the game's own names:

| | |
|---|---|
| Title and intro | `TitleScreenImg`, `TitleImg`, `NHLShieldImg`, `PAlogoImg`, `EASportsMap`, `HiScoreImg` |
| Setup and menus | `GameSetUpMap`, `GameSetUpMap2`, `SetupMenuMap`, `SetupFramerMap`, `LogoBoxMap`, `ScoutMap`, `RonBarrMap`, `PlayerCardsScreenPic`, `PicturePalette`, `ReplayMap` |
| The rink and arena | `Rinktilelist`, `RevRinkTilelist`, `HorRinkMap`, `ArenaGfxBank`, `CornerLogoMap` |
| In game | `ScoresMap`, `EASNmap`, `EnergyBarMap`, `FaceOffMap`, `FramerMap`, `Arrowsmap`, `RefsMap`, `RefMap2`, `HotIconMap`, `ColdIconMap` |
| Fonts | `BigFontMap`, `SmallFontMap`, `PrintFont2Map` |
| Sprites | `Sprites` (a sheet of the players' tiles), `CrowdFrameList`, `ZamFrameList`, `FaceOffSprites`, `PlayoffSprite` |
| Blank cards | `NoPicSkater1`, `NoPicSkater2`, `NoPicGoalie1`, `NoPicGoalie2` |

A shared picture is taken back through its own map: the screen is made of 8 x 8 tiles, and a tile
it shows in two places must look the same in both (`--check-league` says where it does not). Its
tile order, and the sprites' frames, stay as they were. A `NAME.bin` beside a picture is the asset
as the game reads it (a hack's own, gathered from wherever the hack put it); the picture is drawn
from it and taken back into it.

## Leagues from hack ROMs

`--extract-rom` reads a community hack made on the 32-team template (NHL 94 2025, NHL 2026 94
Edition, the 30th Anniversary Championship Teams and the like) and writes its league as a folder:
teams, players, ratings, lines, playoff draws, colours (the jerseys too, with its player sprites)
and all its art, logos to photos to the shared art it has of its own. The hack is only read, never
run, and stays yours: nothing from it goes anywhere but that folder.

Its code is not taken, so what a hack changed there is not either: its own gameplay, its photo
carousel's order, a new menu line (the 2026 edition's Fighting), its own period lengths, its
settings' defaults (line changes on, say). Nor are its menu and announcer texts: hacks reword them
to suit their own code (the 30th Anniversary's "Leave Off!" for user records, which work fine
here), so they could not be taken safely. Some of what a hack's code shows is taken as data:

- where the screen after the EA logo puts its pictures (`hiscore_screen` in `league.ini`);
- the setup screen's banners: the template has one HOME label for every team (its code makes an
  AWAY beside it), so each team gets its own name there instead, in the cartridge's lettering.

The 2025 hack also moves its in-game boxes (the goal, injury, three stars, penalty shot and
shootout boxes, the scoreboard) about 7 columns right, as for a screen 320 wide. Those places are in
its code too and are not taken; the game's own wide rink view is a game rule instead.

## Packages

```
./build/nhl94 --rom "NHL 94.gen" --pack-league leagues/my-league my-league.zip
```

puts a league's files in a zip that any unzip tool opens. A package in `leagues/` plays as it is.
The same folder always makes the same package, byte for byte, and the package must build the
folder's league exactly: `--pack-league` checks, and prints the package's checksum and the
league's. A package from someone else is only read: every name, size and checksum in it is
checked before anything is taken from it, and its pictures and files are checked as a folder's
are. The NHL 2026 edition's whole league, every photo included, packs to about 1.7 MB.

## Records, saved states and online play

Each league keeps its own battery save (its user records and record holders, team and crowd
records, and edited lines), in a file named after it beside the cartridge's:
`out/nhl94-my-league.srm`.

A saved state remembers its league's checksum and plays on only with that league: change the
league's files (or extract it again) and its old states will not load. Quick resume is a state
too. The checksum is also how the session tells that every player has the host's league, for the
online play to come.

## The League page

Esc, **League**: the line to choose a league, and under it what the chosen one holds:

- **Files**: the cartridge's own, or its folder or package in `leagues/` (and the package's size);
- **Teams**: how many, and how many play for the cup;
- **Best skaters**, **Best goalies**: the top five of each from the teams that play for the cup,
  by the overall rating their cards show (worked out the way the cartridge does, hot and cold
  streaks aside; a player the All-Stars repeat is listed once);
- **Best teams**: the top eleven by team rating.

The cartridge's own figures appear once its ROM is chosen.

## When something is wrong

`--check-league DIR` builds the league and says what holds, or what is wrong and where, with the
file and line: `players.csv, line 14: shot_power: 16 is not from 0 to 15`, `goalies.csv: ANA has 1
goalies: 2 to 4`, `.../art/logos/BOS.png: the colour #E02000 at 12, 30 is none of its palette's (the
PNG as written has them)`. A league that will not
read is left off the League page's list, and the terminal says why. `--league DIR` with
`--headless` plays one without the window, which is how the tests play them.
