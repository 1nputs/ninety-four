#!/usr/bin/env python3
"""Tile screenshots from a directory into one labelled grid image (needs Pillow)."""
import sys, os, glob
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 5
files = sorted(glob.glob(os.path.join(d, '*.png')))
if not files: sys.exit('no images')
ims = [Image.open(f).convert('RGB') for f in files]
w, h = max(i.width for i in ims), max(i.height for i in ims)
rows = (len(ims) + cols - 1) // cols
sheet = Image.new('RGB', (cols * w, rows * (h + 12)), (40, 40, 40))
dr = ImageDraw.Draw(sheet)
for k, (f, im) in enumerate(zip(files, ims)):
    x, y = (k % cols) * w, (k // cols) * (h + 12)
    sheet.paste(im, (x, y + 12)); dr.text((x + 2, y), os.path.basename(f)[:-4], fill=(255, 255, 0))
sheet.save(out); print(out, sheet.size, len(ims), 'images')
