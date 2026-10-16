#!/usr/bin/env python3
"""The README's GIF: a goal, as the native game plays it. From faceoff.snap (make vectors) with
the seed 94 random input, nhl94 --watch on the scoring summary's length finds the goals; the
state saved before the one chosen, loaded again, gives every frame around it; Pillow makes a GIF
of every second frame at twice the size, with the frames' own colours (the console shows at
most 64 at a time). The picture comes from your ROM, so the GIF is not committed: this makes it.

    tools/readme_gif.py [--goal N] [--out docs/media/nhl94-scoring.gif]

--goal: which goal of the run (2: Blake from Gretzky at 7:30 of the first, with this build).
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NHL94 = os.path.join(ROOT, 'build', 'nhl94')
SNAP = os.path.join(ROOT, 'out', 'states', 'faceoff.snap')
SCRIPT = os.path.join(ROOT, 'tools', 'oracle', 'scripts', 'random_seed94.txt')
BEFORE, AFTER = 180, 180            # frames around the goal (the rush, the banner)


def nhl94(*args):
    r = subprocess.run([NHL94, '--headless', '--sram', '/dev/null', '--input', SCRIPT] + list(args),
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        sys.exit('nhl94 failed: ' + r.stderr.strip()[-300:])
    return r.stdout


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--goal', type=int, default=2)
    ap.add_argument('--out', default=os.path.join(ROOT, 'docs', 'media', 'nhl94-scoring.gif'))
    a = ap.parse_args()
    from PIL import Image
    if not os.path.exists(NHL94) or not os.path.exists(SNAP):
        sys.exit('needs build/nhl94 (make nhl94) and out/states/faceoff.snap (make vectors)')
    # the goals: the scoring summary's length ($FFC472) grows by an entry each
    out = nhl94('--snap', SNAP, '--frames', '18000', '--watch', 'FFC472')
    goals = [int(f) for f, v in re.findall(r'frame (\d+): \$FFC472 = \$([0-9A-F]+)', out) if int(v, 16)]
    if len(goals) < a.goal:
        sys.exit('the run has %d goals' % len(goals))
    g = goals[a.goal - 1]
    print('goal %d at frame %d' % (a.goal, g))
    work = os.path.join(ROOT, 'out', 'readme_gif')
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    state = os.path.join(work, 'before.state')
    nhl94('--snap', SNAP, '--frames', str(g - BEFORE - 100), '--save-state', '%d:%s' % (g - BEFORE - 120, state))
    nhl94('--load-state', state, '--frames', str(g + AFTER), '--shots', work, '--every', '1')
    shots = sorted(glob.glob(os.path.join(work, 'f*.ppm')))
    first = os.path.join(work, 'f%06d.ppm' % (g - BEFORE))
    frames = [Image.open(f).convert('RGB') for f in shots[shots.index(first)::2]]
    colours = sorted({c for im in frames for _, c in im.getcolors(1 << 16)})
    pal = Image.new('P', (1, 1))
    flat = [v for c in colours for v in c]
    pal.putpalette(flat + [0] * (768 - len(flat)))
    gif = [im.resize((im.width * 2, im.height * 2), Image.NEAREST).quantize(palette=pal, dither=Image.Dither.NONE) for im in frames]
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    gif[0].save(a.out, save_all=True, append_images=gif[1:], duration=33, loop=0, disposal=1)
    shutil.rmtree(work)
    print('%s: %d frames, %d colours, %d KB' % (os.path.relpath(a.out, ROOT), len(gif), len(colours), os.path.getsize(a.out) // 1024))


if __name__ == '__main__':
    main()
