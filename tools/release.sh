#!/bin/sh
# The release's downloads, in out/release/vVERSION (make release VERSION=0.3.2):
#   ninety-four-VERSION-linux-x86_64.tar.gz   nhl94 for x86-64 Linux (GCC: glibc 2.34 or newer and
#                                             the system's SDL2), its README, ROSTERS.md, licenses/
#   ninety-four-VERSION-windows-x86_64.zip    nhl94.exe for 64-bit Windows (MinGW-w64 GCC), SDL2.dll
#                                             beside it, the same with SDL2's licence
#   ninety-four-VERSION-macos-universal.zip   ninety-four.app for macOS 11 or newer, Apple Silicon and
#                                             Intel in one (Zig's Clang), SDL2.framework inside it,
#                                             the same beside it
#   SHA256SUMS                                the three files' checksums
#   RELEASE-NOTES.md                          a copy of NOTES=FILE, when given (the release's text)
# Inside each the folder is ninety-four-linux-x86_64, ninety-four-windows-x86_64 or
# ninety-four-macos, whatever the version, so a new one unpacks over the old and keeps its
# settings and saves.
#
# The games are built from clean at the commit checked out, which must have no changes: a build's
# identity is its commit, which online play compares, so the downloads play each other. The READMEs
# and the app's Info.plist are in tools/release (@VERSION@ the version). No download may hold a
# path of the machine that built it.
#
# What the builds need besides GCC (all of it is found under out/ by default):
#   MINGW=DIR   MinGW-w64 GCC and SDL2's MinGW development package (out/mingw): bin/x-gcc,
#               bin/x-g++ and bin/x-ar (the compilers), bin/pkg-config (giving SDL2's MinGW flags
#               for sdl2), root/usr/bin/x86_64-w64-mingw32-strip, and sdl/SDL2-*/ (SDL2's package:
#               x86_64-w64-mingw32/bin/SDL2.dll, LICENSE.txt)
#   ZIG=FILE    Zig, whose Clang builds the macOS games (out/zig/zig: pip's ziglang package)
#   MACSDL=DIR  SDL2's macOS release, the SDL2-*.dmg's contents (out/macos): SDL2-*/SDL2.framework
#               and SDL2-*/License.txt
#   LIPO=FILE   LLVM's llvm-lipo, which makes the two macOS games one (found on the PATH or in
#               /usr/lib/llvm-*/bin)
# The app is signed as Zig's linker signs a program for Apple Silicon (ad hoc), not by a
# registered developer: the macOS README says how to open it the first time.
set -eu
cd "$(dirname "$0")/.."
VERSION=${1:-}
VERSION=${VERSION#v}
case "$VERSION" in
    [0-9]*.[0-9]*) ;;
    *) echo "usage: tools/release.sh VERSION (e.g. 0.3.2), or make release VERSION=0.3.2"; exit 1 ;;
esac
OUT=out/release/v$VERSION
NAME=ninety-four-$VERSION
need() { [ -e "$1" ] || { echo "no $1 ($2: see the top of tools/release.sh)"; exit 1; }; }
abs() { (cd "$(dirname "$1")" && echo "$(pwd)/$(basename "$1")"); }

need "${MINGW:-out/mingw}" MINGW
MINGW=$(cd "${MINGW:-out/mingw}" && pwd)
SDL=$(ls -d "$MINGW"/sdl/SDL2-* 2> /dev/null | tail -n 1)
for f in "$MINGW/bin/x-gcc" "$MINGW/bin/pkg-config" "$MINGW/root/usr/bin/x86_64-w64-mingw32-strip" \
         "$SDL/x86_64-w64-mingw32/bin/SDL2.dll" "$SDL/LICENSE.txt"; do need "$f" MINGW; done
need "${ZIG:-out/zig/zig}" ZIG
ZIG=$(abs "${ZIG:-out/zig/zig}")
need "${MACSDL:-out/macos}" MACSDL
MACSDL=$(ls -d "$(cd "${MACSDL:-out/macos}" && pwd)"/SDL2-* 2> /dev/null | tail -n 1)
for f in "$MACSDL/SDL2.framework/Versions/A/SDL2" "$MACSDL/License.txt"; do need "$f" MACSDL; done
LIPO=${LIPO:-$(command -v llvm-lipo || ls /usr/lib/llvm-*/bin/llvm-lipo 2> /dev/null | tail -n 1)}
need "${LIPO:-llvm-lipo}" LIPO
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "the tree has changes: commit them first (a build's identity is its commit)"; exit 1; }
[ ! -e "$OUT" ] || { echo "$OUT is there already: move it away first"; exit 1; }
if [ -n "${NOTES:-}" ] && [ ! -f "$NOTES" ]; then echo "no $NOTES (NOTES)"; exit 1; fi

# ---- the games, from clean ------------------------------------------------------------------
JOBS=$(nproc 2> /dev/null || echo 4)
rm -rf build/obj build/win64 build/mac-arm64 build/mac-x64 build/mac build/macos-pkg
rm -rf ext/pulseman/build/release ext/pulseman/build/win64 ext/pulseman/build/mac-arm64 ext/pulseman/build/mac-x64
rm -f build/nhl94 build/nhl94.exe
echo "building nhl94 for Linux"
make -j"$JOBS" nhl94 > /dev/null
echo "building nhl94.exe for Windows"
PATH="$MINGW/bin:$MINGW/root/usr/bin:$PATH" make -j"$JOBS" OS=Windows_NT CC=x-gcc CXX=x-g++ AR=x-ar \
    MDRTB=build/win64 OBJ=build/win64/obj nhl94 > /dev/null
# macOS: SDL2's framework found through a pkg-config of the build's own, the app's Frameworks on the
# program's rpath, and the programs stripped as they are linked (stripped after, Apple Silicon's
# signature would no longer hold)
mkdir -p build/macos-pkg
cat > build/macos-pkg/pkg-config <<EOF
#!/bin/sh
# (the macOS build's pkg-config, made by tools/release.sh: SDL2's framework)
case "\$*" in
  *--cflags*) echo "-I$MACSDL/SDL2.framework/Headers -F$MACSDL" ;;
  *--libs*) echo "-F$MACSDL -framework SDL2 -Wl,-rpath,@executable_path/../Frameworks" ;;
esac
EOF
chmod +x build/macos-pkg/pkg-config
mac() {   # $1: the build's name, $2: Zig's target
    PATH="$PWD/build/macos-pkg:$PATH" make -j"$JOBS" CC="$ZIG cc -target $2" CXX="$ZIG c++ -target $2" AR="$ZIG ar" \
        CXXLIB=-lc++ LDFLAGS=-s BIN="build/$1/nhl94" OBJ="build/$1/obj" MDRTB="build/$1" "build/$1/nhl94" > /dev/null
}
echo "building nhl94 for macOS (Apple Silicon)"
mac mac-arm64 aarch64-macos.11.0
echo "building nhl94 for macOS (Intel)"
mac mac-x64 x86_64-macos.11.0
mkdir -p build/mac
"$LIPO" -create build/mac-arm64/nhl94 build/mac-x64/nhl94 -output build/mac/nhl94
ID=$(sed -n 's/^#define NHL94_BUILD "\(.*\)"/\1/p' build/build_id.h)
case "$ID" in *-dirty|unknown|"") echo "the build's identity is \"$ID\", not a commit"; exit 1 ;; esac
for b in build/nhl94 build/nhl94.exe build/mac-arm64/nhl94 build/mac-x64/nhl94; do
    grep -a -q "$ID" "$b" || { echo "$b is not build $ID"; exit 1; }
done

# ---- the folders ----------------------------------------------------------------------------
folder() {   # $1: the README's system (linux, windows, macos), $2: the folder
    d=$OUT/$2
    mkdir -p "$d/licenses"
    sed "s/@VERSION@/$VERSION/g" "tools/release/README-$1.txt" > "$d/README.txt"
    cp docs/ROSTERS.md "$d/ROSTERS.md"
    cp LICENSE "$d/licenses/ninety-four-MIT.txt"
    cp ext/pulseman/LICENSE "$d/licenses/pulseman-MIT.txt"
    cp ext/pulseman/third_party/nuked-opn2/LICENSE "$d/licenses/Nuked-OPN2-LGPL-2.1.txt"
    cp ext/pulseman/third_party/ymfm/LICENSE "$d/licenses/ymfm-BSD-3.txt"
    cp tools/release/licenses/*.txt "$d/licenses/"
}
folder linux ninety-four-linux-x86_64
strip --strip-all -o "$OUT/ninety-four-linux-x86_64/nhl94" build/nhl94
folder windows ninety-four-windows-x86_64
"$MINGW/root/usr/bin/x86_64-w64-mingw32-strip" --strip-all -o "$OUT/ninety-four-windows-x86_64/nhl94.exe" build/nhl94.exe
cp "$SDL/x86_64-w64-mingw32/bin/SDL2.dll" "$OUT/ninety-four-windows-x86_64/"
cp "$SDL/LICENSE.txt" "$OUT/ninety-four-windows-x86_64/licenses/SDL2-zlib.txt"
folder macos ninety-four-macos
APP=$OUT/ninety-four-macos/ninety-four.app/Contents
mkdir -p "$APP/MacOS" "$APP/Frameworks"
cp build/mac/nhl94 "$APP/MacOS/nhl94"
sed "s/@VERSION@/$VERSION/g" tools/release/Info.plist > "$APP/Info.plist"
cp -a "$MACSDL/SDL2.framework" "$APP/Frameworks/"
cp "$MACSDL/License.txt" "$OUT/ninety-four-macos/licenses/SDL2-zlib.txt"
for b in "$OUT/ninety-four-linux-x86_64/nhl94" "$OUT/ninety-four-windows-x86_64/nhl94.exe" "$APP/MacOS/nhl94"; do
    if grep -a -q -e "$PWD" -e "${HOME:-/nonexistent}/" "$b"; then echo "$b holds a path of this machine"; exit 1; fi
done
if [ -n "${NOTES:-}" ]; then cp "$NOTES" "$OUT/RELEASE-NOTES.md"; fi

# ---- the downloads, named by the version --------------------------------------------------------
DAY=$(TZ=UTC git log -1 --format=%cd --date=format-local:%Y-%m-%d)   # (the commit's day: the tar the same each time)
(
    cd "$OUT"
    tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="$DAY 00:00Z" -czf "$NAME-linux-x86_64.tar.gz" ninety-four-linux-x86_64
    zip -qrX "$NAME-windows-x86_64.zip" ninety-four-windows-x86_64
    zip -qrXy "$NAME-macos-universal.zip" ninety-four-macos      # (-y: the framework's links kept as links)
    sha256sum "$NAME-linux-x86_64.tar.gz" "$NAME-windows-x86_64.zip" "$NAME-macos-universal.zip" > SHA256SUMS
)
echo "ninety-four $VERSION (build $ID) in $OUT:"
ls -l "$OUT"
