ninety-four v@VERSION@: NHL '94, rebuilt in C
https://github.com/1nputs/ninety-four

You need your own NHL '94 ROM (the 1 MB US/Europe retail cartridge dump). Run ./nhl94: the first
time it asks for the ROM in a file browser and remembers it (or give it: ./nhl94 --rom FILE).

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
Alt+Enter          full screen

The menu (Esc, or a controller's Back button) has the ROM, a reset, save slots, the video (a
pixel-art upscaler: Scale2x, Scale3x, MMPX or xBR; sharp, integer or fit scaling; square pixels or
4:3; the colours' levels; dithering blended as a television blended it; a picture tube's scanlines
and mask; full screen; vsync), captures, the sound (volume, the FM sound core and chip), the
players, the league, the game rules, online play, options and every key: pick one and press the
new key.

Game rules (the menu's Game rules page, for every player and the whole game; Classic, the default,
is the original exactly): the weight bug fixed, the second assist kept on a give-and-go, plus/minus
and body checks in Player Stats, more period lengths, the season's overtime length, skating speed,
the speed burst, shot power, how far the puck slides, tiring, bench rest, rest in the penalty box,
how many penalties are called, hot and cold streaks, a wide rink view, and presets (Classic,
Simulation, Arcade). The amounts are numbers underneath: settings.ini takes any in a rule's range
(rules.skating = 115), and so does --rule skating=115.

Leagues (the menu's League page): the cartridge's teams, or a league of your own of up to 32 teams,
with its players, ratings, lines, colours, logos, banners, centre ice, photos and even the title
screens and rinks, as CSV files and PNGs in a "leagues" folder beside the program (or anywhere:
the page's Files line finds one). To start one, or take one from a community hack made on the
32-team template, or pack one into a single zip to send:

    ./nhl94 --export-league leagues/my-league
    ./nhl94 --extract-rom "HACK.bin" leagues/2026
    ./nhl94 --check-league leagues/my-league
    ./nhl94 --pack-league leagues/my-league my-league.zip

(each with --rom FILE, unless the ROM is beside the program as "NHL 94 (UE) [!].gen"). The page
says what the chosen league holds: its files, its teams and its best players and teams by the
game's own ratings. Everything about the files is in LEAGUES.md.

Four players: in the menu, Players, set Controllers to EA 4 Way Play, Reset, then choose Players:
Three or Four in the game's setup screen. The Players page also gives each player a controller,
moves a controller's buttons, adds turbo and sets the stick's dead zone.

Online play (the menu's Online play page): two to four machines, over the internet or a LAN, with
rollback, so no one waits. Set Your name and Players here (how many play at this machine). One
machine hosts (Play as: host, and Players in all) and the others join its address and port (7000
by default; the host's port must be reachable, forwarded on its router for UDP); two machines can
instead both connect at once, which most home routers let through without a forwarded port. When
everyone is in, the game powers on everywhere: the host's players on pads 1 onward, then each
machine's as it joined; with pads 3 or 4 played, EA's 4 Way Play is in the ports (Players: Three
or Four). The host's league, game rules and save RAM (its own, or a new cartridge's: the host's
Save RAM on the same page) go to everyone, and nothing is saved from an online game. The host's
Reset (in the menu) starts the game again for everyone. If anyone leaves, the game is over for
everyone. Every machine needs this same build and the US/Europe
cartridge.

Save slots: eight, each with a picture, on the menu's Save and Load pages (states are taken during
play). Quit in the middle of a game and the next start puts you back where you were.

Captures: the last 20 seconds are always kept; F10 (or the menu's Capture page) saves them and
F11 records until pressed again, as GIFs (exact colours, 30 frames a second, no sound). The
Capture page sets the replay's length and a GIF's size.

Everything is kept in settings.ini, beside the program in the folder you run it from, with the
saves (an "out" folder), save slots ("states"), screenshots and videos. On a Mac, hold Fn for the
function keys.

The licences of the code inside are in "licenses": ninety-four (MIT), pulseman (MIT), Nuked-OPN2
(LGPL-2.1), ymfm (BSD-3), MMPX (MIT), xBR (MIT). The source of everything is at the address above
and at https://github.com/1nputs/pulseman.

Linux: x86-64, glibc 2.34 or newer, with SDL2 installed (Debian/Ubuntu: sudo apt install libsdl2-2.0-0).
