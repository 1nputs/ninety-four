# Rosters

Rosters are who plays: the teams, their players and ratings, lines, colours, logos, banners, centre
ice, the players' photos, and if you like the art every team shares (title screens, rinks, sprites,
fonts and the rest). The game plays the cartridge's own rosters unless you choose others on the
menu's **Rosters** page. Rosters are a folder of text files and pictures, so a spreadsheet and a
paint program are all you need to make them, or they can be taken from a community hack ROM.
Either way they can travel as one zip file.

- [Quick start](#quick-start)
- [Where rosters live](#where-rosters-live)
- [The folder](#the-folder)
- [The files](#the-files): [rosters.ini](#rostersini), [teams.csv](#teamscsv),
  [goalies.csv and players.csv](#goaliescsv-and-playerscsv), [lines.csv](#linescsv),
  [colours.csv](#colourscsv), [pictures.csv](#picturescsv), [playoffs.csv](#playoffscsv)
- [The art](#the-art)
- [Rosters from hack ROMs](#rosters-from-hack-roms)
- [Packages](#packages)
- [Records, saved states and online play](#records-saved-states-and-online-play)
- [The Rosters page](#the-rosters-page)
- [When something is wrong](#when-something-is-wrong)

## Quick start

Run these from the folder you play from (the game looks for `rosters/` there). Each needs the
cartridge: `--rom FILE`, or your dump in that folder as `NHL 94 (UE) [!].gen`.

```
# start from the cartridge's own rosters, to change as you like
./build/nhl94 --rom "NHL 94.gen" --export-rosters rosters/my-rosters

# or take a hack's rosters (read as data, never run)
./build/nhl94 --rom "NHL 94.gen" --extract-rom "NHL 2026 - 94 Edition v5.0S.bin" rosters/nhl2026

# check it: what holds, or what is wrong and on which line
./build/nhl94 --rom "NHL 94.gen" --check-rosters rosters/my-rosters

# make it one file to keep or send
./build/nhl94 --rom "NHL 94.gen" --pack-rosters rosters/my-rosters my-rosters.zip
```

Then start the game, press **Esc**, open **Rosters**, pick it with Left / Right and choose **Reset**.
A change of rosters takes effect at the next power on, so a game in progress is never changed under you.

## Where rosters live

In `rosters/`, beside the game (the folder you run it from), or anywhere you like. Each set of rosters is
a folder, `rosters/NAME/`, or a package, `rosters/NAME.zip`, which plays as it is with no
unpacking. The game lists what is in `rosters/` when it starts; rosters anywhere else are found
from the Rosters page: choose **Files**, and in the browser that opens (in `rosters/`), go to the
rosters' folder and choose **Use this folder**, or choose their `.zip`. They join the list, and the
settings file keeps their path, so they are there the next time too. The folder's (or package's) name is
how the game, the settings file (`rules.rosters = NAME`, or the path) and their records know them;
the name the menu shows is the one in their `rosters.ini`. `cartridge` is the cartridge's own and cannot
be a folder's name. The list holds 31 sets of rosters besides the cartridge's.

Rosters in `rosters/` that will not read are left off the list. The Rosters page's note says how
many were left out, and the terminal the game was started from says why ("... (not in the menu)").

## The folder

```
rosters/my-rosters/
  rosters.ini          its name, how many teams play for the cup (optional)
  teams.csv           the teams (needed)
  goalies.csv         the goalies (needed)
  players.csv         the skaters (needed)
  lines.csv           the lines (optional: made from the roster's order)
  colours.csv         the teams' colours (optional: their art teams')
  pictures.csv        which player cards have photos (optional)
  playoffs.csv        the playoff draws (optional: made from the conferences)
  art/                pictures of the rosters' own (all optional)
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

### rosters.ini

```
name = My Rosters
playoff_teams = 26
hiscore_screen = 14,3 16,16
```

| Key | |
|---|---|
| `name` | the name the menu and the Rosters page show (at most 63 letters). Missing: the folder's name |
| `playoff_teams` | how many teams play for the cup: the first that many in `teams.csv` (18 up to all of them). The rest, like the cartridge's All-Star teams, play single games only. Missing: all of them |
| `hiscore_screen` | where the screen after the EA logo puts its two pictures: column,row (0-39, 0-27) of its logo, then of `HiScoreImg`. Missing: the cartridge's 14,3 16,16. The hacks draw a whole screen there, at 0,0 0,0 |

Lines starting with `#` or `;` are comments.

### teams.csv

A row a team, in the order the game lists them (the first `playoff_teams` play for the cup).
Rosters have 18 to 32 teams.

| Column | | Range |
|---|---|---|
| `team` | its abbreviation: how the other files name it (unique) | 1-3 letters |
| `city` | the city, on the setup screen and the banners | up to 17 letters |
| `nickname` | the team's name: Penguins, Oilers | up to 14 |
| `arena` | the arena the announcer names | up to 26 |
| `conference` | the side of the playoff draws it plays on, when `playoffs.csv` gives none | up to 15 |
| `art` | the cartridge team whose logo, banners and colours it shows where it has none of its own: `ANH BOS BUF CGY CHI DAL DET EDM FLA HFD LA MTL NJ NYI NYR OTW PHI PIT QUE SJ STL TB TOR VAN WSH WPG ASE ASW` | (its own abbreviation when empty) |
| `rating` | the team rating its cards and the matchups show | 0-99 |
| `offense`, `defense` | how its other playoff games are simulated: the higher its offense, the more it scores; the higher its defense, the more it lets in | 0-7 |
| `pp`, `pk` | what its skaters' ratings gain on the power play, and lose killing a penalty | 0-15 |
| `home`, `away` | what its skaters' ratings gain at home, and lose away | 0-15 |
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
game's overall rating for a player (on his card, and the Rosters page's) is worked out from these
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
names a photo of the rosters' own (`art/photos/NAME.png`), or one of the cartridge's by its label
(`CardMarioLemieux`, `NoPicSkater1` ...). A team has up to 32. A player with none gets the
cartridge's blank card.

### playoffs.csv

The draws a playoff starts from, a row of 16 team abbreviations each: the first 8 one side of the
bracket, the last 8 the other. Give a power of two rows (1, 2, 4 ... 32); the game picks one.
Without the file, draws are made from the playoff teams' conferences (two of them, 8 or more
teams each; else from all of them).

## The art

Everything under `art/` is optional: a team with no picture shows its art team's. The pictures are
PNGs, written by `--export-rosters` and `--extract-rom` in the colours they show in, so you can draw
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
is none of them, `--check-rosters` says which colour and where.

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
it shows in two places must look the same in both (`--check-rosters` says where it does not). Its
tile order, and the sprites' frames, stay as they were. A `NAME.bin` beside a picture is the asset
as the game reads it (a hack's own, gathered from wherever the hack put it); the picture is drawn
from it and taken back into it.

## Rosters from hack ROMs

`--extract-rom` reads a community hack made on the 32-team template (NHL 94 2025, NHL 2026 94
Edition, the 30th Anniversary Championship Teams and the like) and writes its rosters as a folder:
teams, players, ratings, lines, playoff draws, colours (the jerseys too, with its player sprites)
and all its art, logos to photos to the shared art it has of its own. The hack is only read, never
run, and stays yours: nothing from it goes anywhere but that folder.

Its code is not taken, so what a hack changed there is not either: its own gameplay, its photo
carousel's order, a new menu line (the 2026 edition's Fighting), its own period lengths, its
settings' defaults (line changes on, say). Nor are its menu and announcer texts: hacks reword them
to suit their own code (the 30th Anniversary's "Leave Off!" for user records, which work fine
here), so they could not be taken safely. Some of what a hack's code shows is taken as data:

- where the screen after the EA logo puts its pictures (`hiscore_screen` in `rosters.ini`);
- the setup screen's banners: the template has one HOME label for every team (its code makes an
  AWAY beside it), so each team gets its own name there instead, in the cartridge's lettering.

The 2025 hack also moves its in-game boxes (the goal, injury, three stars, penalty shot and
shootout boxes, the scoreboard) about 7 columns right, as for a screen 320 wide. Those places are in
its code too and are not taken; the game's own wide rink view is a game rule instead.

### Known issue: NHL 94 2025's ratings play as the cartridge's 6 at most

The cartridge rates a player's skills 0 to 6, and the game plays a rating as five times it, at most
30, a hot or cold streak added (`AttributeCalc`): so a 7 or more plays as a 6, but for a streak,
which it is further from. NHL 94 2025 (`[h] 2025 AC v3.9`) rates on a wider scale of its own, for
its own code: all of its 704 skaters past 6 in agility, shot power and shot accuracy (speed up to
10, shot power up to 14), most in their awareness, stick handling and endurance, and all of its 96
goalies past 6 in every rating a goalie has. Its code is not taken, so here those ratings play as
the cartridge reads them: every rating past 6 the same as a 6, and its players far more alike than
its authors meant. Its cards' overall ratings come out 99 for many of them (the game's reckoning
stops there), and the roster screen's numbers run over (8 to 14 show as 25). The Rosters page orders
such ratings by the rosters' own numbers and shows those (see below). Its teams, lines, colours and
art are as the hack has them; only how far its ratings reach is lost. A game rule that reads a
wider scale would be the way to play it as meant; there is none yet. The other hacks here rate on
the cartridge's scale (the 30th Anniversary's has three aggressiveness ratings past 6).

## Packages

```
./build/nhl94 --rom "NHL 94.gen" --pack-rosters rosters/my-rosters my-rosters.zip
```

puts the rosters' files in a zip that any unzip tool opens. A package in `rosters/` plays as it is.
The same folder always makes the same package, byte for byte, and the package must build the
folder's rosters exactly: `--pack-rosters` checks, and prints the package's checksum and the
rosters'. A package from someone else is only read: every name, size and checksum in it is
checked before anything is taken from it, and its pictures and files are checked as a folder's
are. The NHL 2026 edition's whole rosters, every photo included, pack to about 1.7 MB.

## Records, saved states and online play

Each set of rosters keeps its own battery save (its user records and record holders, team and crowd
records, and edited lines), in a file named after its folder (or package) beside the cartridge's:
`out/nhl94-my-rosters.srm`. Two sets of rosters in folders of one name, in different places, share it.

A saved state remembers its rosters' checksum and plays on only with those rosters: change the
rosters' files (or extract them again) and their old states will not load. A new version of the game
can reckon the rosters' checksum another way, and then their states from before will not load either
(the cartridge's own rosters keep their states). The Autosave is a state too.

Online (README.md, "Online play") the rosters are the host's: their package (a folder's files packed
as `--pack-rosters` packs them, or the package as it is) goes to every other machine before the
game starts, and each builds them from those bytes, so no one else needs a copy and all play the
same rosters to the byte. The save RAM is a new cartridge's on every machine, and no one's save file
is written, during the session or after it: records go online in a state, which the host loads for
everyone (one of the session's rosters: a state of others is refused, as a guest has only the
host's).

## The Rosters page

Esc, **Rosters**: the line to choose rosters, and under it what the chosen one holds:

- **Files**: the cartridge's own, or its folder or package (and the package's size). Choose it to
  find rosters anywhere on disk;
- **Teams**: how many, and how many play for the cup;
- then a line to step through sixteen pages of them with left and right (the page stays as you
  choose other rosters, to compare them). Each is of the teams that play for the cup, a player
  the All-Stars repeat counted once:
  - **Best skaters**, **Best goalies**: the top ten by the overall rating their cards show
    (worked out the way the cartridge does, hot and cold streaks aside);
  - **Best teams**: the top ten by team rating;
  - **Fastest skaters**, **Hardest shots**, **Sharpest shooters**, **Best passers**, **Best
    stickhandlers**, **Biggest hitters**: the top ten by that rating (speed, shot power, shot
    accuracy, pass accuracy, stick handling, checking), shown as the roster screen shows it. The
    game rates each of these 0 to 6 and plays a rating as five times it, at most 30, a hot or cold
    streak added (so a 7 or more plays as a 6, but for a streak); the roster screen shows 0 to 6 as
    25, 34, 43, 54, 72, 90 and 99, with nothing between, and its reckoning runs over past 7 (8 to
    14 come out as 25). So rosters that rate past 7 (as NHL 94 2025 does, for its own code)
    show their own numbers there instead, and say so;
  - **Heaviest skaters**, **Lightest skaters**: ten of each, in pounds as the roster screen has them;
  - **Power play**, **Penalty killing**, **Home ice**, **On the road**: the teams by the bonus
    their skaters' ratings gain a man up or at home (`pp`, `home`), or lose a man down or away
    (`pk`, `away`), those as good on one line;
  - **The rosters in numbers**: their players, forwards and defensemen, how many shoot left and
    right, the average skater's weight, the number worn most, the rosters' sizes (the biggest and
    smallest, or how many teams share them), and the teams that score most and allow least when
    the playoffs simulate their other games (`offense`, `defense`).

The cartridge's own figures appear once its ROM is chosen.

## When something is wrong

`--check-rosters DIR` builds the rosters and says what holds, or what is wrong and where, with the
file and line: `players.csv, line 14: shot_power: 16 is not from 0 to 15`, `goalies.csv: ANA has 1
goalies: 2 to 4`, `.../art/logos/BOS.png: the colour #E02000 at 12, 30 is none of its palette's (the
PNG as written has them)`. Rosters that will not
read are left off the Rosters page's list, and the terminal says why. `--rosters DIR` with
`--headless` plays one without the window, which is how the tests play them.
