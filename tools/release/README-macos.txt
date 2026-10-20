ninety-four v@VERSION@: NHL '94, rebuilt in C
https://github.com/1nputs/ninety-four

You need your own NHL '94 ROM (the 1 MB US/Europe retail cartridge dump). Open ninety-four.app:
the first time it asks for the ROM in a file browser and remembers it.

The first time, macOS will not open it with a double-click: it is not signed by a registered
Apple developer. Control-click (or right-click) ninety-four.app, choose Open, then Open again. On
macOS 15 and newer, try to open it once, then in System Settings, Privacy & Security, choose Open
Anyway. (Or in Terminal: xattr -dr com.apple.quarantine ninety-four.app)

Online play on your own network: the first time you join a game, macOS asks whether ninety-four may
find and connect to devices on your local network; allow it. A new version may count as a new app
and be asked about again. If the question was missed or refused, joining gets no answer (the
connecting page says so): turn ninety-four on in System Settings, Privacy & Security, Local
Network, or off and on again.

Keys        Player 1     Player 2
D-pad       arrows       I J K L
A / B / C   Z / X / C    Q / W / E
Start       Enter        Tab

Game controllers work too (the first two are players 1 and 2).

F1                 mute
F2 / F3            volume down / up
F4 (held)          rewind
F5                 pause
F6 (held)          fast-forward
F7                 slow motion on / off
F8                 a frame at a time, while paused
F10                save the replay: the last 20 seconds, as a GIF (in a "videos" folder)
F11                start / stop recording a GIF
F12                screenshot (a PNG in a "screenshots" folder)
Esc                the menu (save and load states there)
Option+Enter       full screen

Hold Fn for the function keys (or set them to act as standard function keys in System Settings,
Keyboard).

The menu (Esc, or a controller's Back button) has the ROM, a reset, save slots, the video (a
pixel-art upscaler: Scale2x, Scale3x, MMPX or xBR; sharp, integer or fit scaling; square pixels or
4:3; the colours' levels; dithering blended as a television blended it; a picture tube's scanlines
and mask; full screen; vsync), captures, the sound (volume, the FM core and chip), the
players, the rosters, the game rules, online play, options and every key: pick one and press the
new key.

Game rules (the menu's Game rules page, for every player and the whole game; Classic, the default,
is the original exactly): the weight bug fixed, the second assist kept on a give-and-go, plus/minus
and body checks in Player Stats, more period lengths, the season's overtime length, skating speed,
the speed burst, shot power, how far the puck slides, tiring, bench rest, rest in the penalty box,
how many penalties are called, hot and cold streaks, a wide rink view, and presets (Classic,
Simulation, Arcade). The amounts are numbers underneath: settings.ini takes any in a rule's range
(rules.skating = 115), and so does --rule skating=115.

Rosters (the menu's Rosters page): the cartridge's teams, or rosters of your own of up to 32 teams,
with their players, ratings, lines, colours, logos, banners, centre ice, photos and even the title
screens and rinks, as CSV files and PNGs in a "rosters" folder in the game's folder (below), or
anywhere: the page's Files line finds one. To start one, take one from a community hack made on
the 32-team template, or pack one into a single zip to send, use Terminal, in the folder that
holds ninety-four.app (the paths are from the folder you run it in):

    ninety-four.app/Contents/MacOS/nhl94 --rom FILE --export-rosters rosters/my-rosters
    ninety-four.app/Contents/MacOS/nhl94 --rom FILE --extract-rom "HACK.bin" rosters/2026
    ninety-four.app/Contents/MacOS/nhl94 --rom FILE --check-rosters rosters/my-rosters
    ninety-four.app/Contents/MacOS/nhl94 --rom FILE --pack-rosters rosters/my-rosters my-rosters.zip

The page says what the chosen rosters hold: their files, their teams and their best players and teams
by the game's own ratings. Everything about the files is in ROSTERS.md.

Four players: in the menu, Players, set Controllers to EA 4 Way Play, Reset, then choose Players:
Three or Four in the game's setup screen. The Players page also gives each player a controller,
moves a controller's buttons, adds turbo and sets the stick's dead zone.

Online play (the menu's Online play page): two to four machines, over the internet or a LAN, with
rollback, so no one waits. Set Name and Players here (how many play at this machine). One
machine hosts (Role: host, and Total players) and the others join its address and port (7000
by default; the host's port must be reachable, forwarded on its router for UDP); two machines can
instead both connect at once, which most home routers let through without a forwarded port. When
everyone is in, the game powers on everywhere: the host's players on pads 1 onward, then each
machine's as it joined; with pads 3 or 4 played, EA's 4 Way Play is in the ports (Players: Three
or Four). The host's rosters and game rules go to everyone, and every game online starts from a
new cartridge (nothing is saved to anyone's save file). Seasons and records go online in a state:
anyone's Save state saves one everyone played, and the host's Load state loads one for everyone
(a state of other rosters than the session's is refused). The host's Reset (in the menu) starts
the game again for everyone. If anyone leaves, the game is over for
everyone. Every machine needs this same build and the US/Europe
cartridge. When you host, allow incoming connections if macOS asks.

Save slots: eight, each with a picture, on the menu's Save and Load pages (states are taken during
play). Quitting saves the Autosave, first on the Load page: the game always starts fresh.

Captures: the last 20 seconds are always kept; F10 (or the menu's Capture page) saves them and
F11 records until pressed again, as GIFs (exact colours, 30 frames a second, no sound). The
Capture page sets the replay's length and a GIF's size.

Opened from the Finder, the game keeps everything in ~/Library/Application Support/ninety-four (in
the Finder: Go, Go to Folder, and paste that): settings.ini, the saves (an "out" folder), save
slots ("states"), screenshots, videos and "rosters". Run from Terminal, in the folder you run it
from.

The licences of the code inside are in "licenses": ninety-four (MIT), pulseman (MIT), Nuked-OPN2
(LGPL-2.1), ymfm (BSD-3), MMPX (MIT), xBR (MIT), and SDL2 (zlib). The source of everything is at
the address above and at https://github.com/1nputs/pulseman.

macOS: 11 (Big Sur) or newer, on Apple Silicon or Intel (one universal app, SDL2 inside it). It is
built with Zig's Clang on Linux and has not been run on a Mac yet: please report what you find.
