#!/bin/sh
# The release's downloads, in out/release/vVERSION (make release VERSION=0.3.1):
#   ninety-four-VERSION-linux-x86_64.tar.gz   nhl94 for x86-64 Linux (GCC: glibc 2.34 or newer and
#                                             the system's SDL2), its README, LEAGUES.md, licenses/
#   ninety-four-VERSION-windows-x86_64.zip    nhl94.exe for 64-bit Windows (MinGW-w64 GCC), SDL2.dll
#                                             beside it, the same with SDL2's licence
#   SHA256SUMS                                the two files' checksums
#   RELEASE-NOTES.md                          a copy of NOTES=FILE, when given (the release's text)
# Inside each the folder is ninety-four-linux-x86_64 or ninety-four-windows-x86_64, whatever the
# version, so a new one unpacks over the old and keeps its settings and saves.
#
# Both games are built from clean at the commit checked out, which must have no changes: a build's
# identity is its commit, which online play compares, so the two downloads play each other. The
# READMEs are tools/release/README-*.txt, @VERSION@ the version.
#
# The Windows build needs MinGW-w64 GCC and SDL2's MinGW development package, in MINGW=DIR
# (out/mingw by default): bin/x-gcc, bin/x-g++ and bin/x-ar (the compilers), bin/pkg-config
# (giving SDL2's MinGW flags for sdl2), root/usr/bin/x86_64-w64-mingw32-strip, and sdl/SDL2-*/
# (SDL2's package: x86_64-w64-mingw32/bin/SDL2.dll, LICENSE.txt).
set -eu
cd "$(dirname "$0")/.."
VERSION=${1:-}
VERSION=${VERSION#v}
case "$VERSION" in
    [0-9]*.[0-9]*) ;;
    *) echo "usage: tools/release.sh VERSION (e.g. 0.3.1), or make release VERSION=0.3.1"; exit 1 ;;
esac
OUT=out/release/v$VERSION
NAME=ninety-four-$VERSION
[ -d "${MINGW:-out/mingw}" ] || { echo "no ${MINGW:-out/mingw}: MINGW=DIR (see the top of tools/release.sh)"; exit 1; }
MINGW=$(cd "${MINGW:-out/mingw}" && pwd)
SDL=$(ls -d "$MINGW"/sdl/SDL2-* 2> /dev/null | tail -n 1)
for f in "$MINGW/bin/x-gcc" "$MINGW/bin/pkg-config" "$MINGW/root/usr/bin/x86_64-w64-mingw32-strip" \
         "$SDL/x86_64-w64-mingw32/bin/SDL2.dll" "$SDL/LICENSE.txt"; do
    [ -e "$f" ] || { echo "no $f (MINGW: see the top of tools/release.sh)"; exit 1; }
done
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "the tree has changes: commit them first (a build's identity is its commit)"; exit 1; }
[ ! -e "$OUT" ] || { echo "$OUT is there already: move it away first"; exit 1; }
if [ -n "${NOTES:-}" ] && [ ! -f "$NOTES" ]; then echo "no $NOTES (NOTES)"; exit 1; fi

# ---- the games, from clean ------------------------------------------------------------------
JOBS=$(nproc 2> /dev/null || echo 4)
rm -rf build/obj build/win64 ext/pulseman/build/release ext/pulseman/build/win64
rm -f build/nhl94 build/nhl94.exe
echo "building nhl94 for Linux"
make -j"$JOBS" nhl94 > /dev/null
echo "building nhl94.exe for Windows"
PATH="$MINGW/bin:$MINGW/root/usr/bin:$PATH" make -j"$JOBS" OS=Windows_NT CC=x-gcc CXX=x-g++ AR=x-ar \
    MDRTB=build/win64 OBJ=build/win64/obj nhl94 > /dev/null
ID=$(sed -n 's/^#define NHL94_BUILD "\(.*\)"/\1/p' build/build_id.h)
case "$ID" in *-dirty|unknown|"") echo "the build's identity is \"$ID\", not a commit"; exit 1 ;; esac
for b in build/nhl94 build/nhl94.exe; do
    grep -a -q "$ID" "$b" || { echo "$b is not build $ID"; exit 1; }
done

# ---- the folders ----------------------------------------------------------------------------
folder() {   # $1: linux or windows
    d=$OUT/ninety-four-$1-x86_64
    mkdir -p "$d/licenses"
    sed "s/@VERSION@/$VERSION/g" "tools/release/README-$1.txt" > "$d/README.txt"
    cp docs/LEAGUES.md "$d/LEAGUES.md"
    cp LICENSE "$d/licenses/ninety-four-MIT.txt"
    cp ext/pulseman/LICENSE "$d/licenses/pulseman-MIT.txt"
    cp ext/pulseman/third_party/nuked-opn2/LICENSE "$d/licenses/Nuked-OPN2-LGPL-2.1.txt"
    cp ext/pulseman/third_party/ymfm/LICENSE "$d/licenses/ymfm-BSD-3.txt"
    cp tools/release/licenses/*.txt "$d/licenses/"
}
folder linux
strip --strip-all -o "$OUT/ninety-four-linux-x86_64/nhl94" build/nhl94
folder windows
"$MINGW/root/usr/bin/x86_64-w64-mingw32-strip" --strip-all -o "$OUT/ninety-four-windows-x86_64/nhl94.exe" build/nhl94.exe
cp "$SDL/x86_64-w64-mingw32/bin/SDL2.dll" "$OUT/ninety-four-windows-x86_64/"
cp "$SDL/LICENSE.txt" "$OUT/ninety-four-windows-x86_64/licenses/SDL2-zlib.txt"
if [ -n "${NOTES:-}" ]; then cp "$NOTES" "$OUT/RELEASE-NOTES.md"; fi

# ---- the downloads, named by the version --------------------------------------------------------
DAY=$(TZ=UTC git log -1 --format=%cd --date=format-local:%Y-%m-%d)   # (the commit's day: the tar the same each time)
(
    cd "$OUT"
    tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="$DAY 00:00Z" -czf "$NAME-linux-x86_64.tar.gz" ninety-four-linux-x86_64
    zip -qrX "$NAME-windows-x86_64.zip" ninety-four-windows-x86_64
    sha256sum "$NAME-linux-x86_64.tar.gz" "$NAME-windows-x86_64.zip" > SHA256SUMS
)
echo "ninety-four $VERSION (build $ID) in $OUT:"
ls -l "$OUT"
