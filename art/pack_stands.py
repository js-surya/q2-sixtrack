#!/usr/bin/env python3
"""Pack the stands (art/stands.svg, from art/gen_stands.py) into ../stands.h: rendered at 375 x 97 with
rsvg-convert, read back as RGB with ImageMagick."""
import os, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
with tempfile.TemporaryDirectory() as d:
    png = os.path.join(d, 'stands.png')
    subprocess.run(['rsvg-convert', '-w', '375', '-h', '97', os.path.join(HERE, 'stands.svg'), '-o', png], check=True)
    raw = subprocess.run(['magick', png, '-depth', '8', 'rgb:-'], check=True, capture_output=True).stdout
assert len(raw) == 375 * 97 * 3
out = ['/* art/pack_stands.py: the stands across the top of the screen, 375 x 97, RGB, from art/gen_stands.py',
       ' * (original artwork). Do not edit. */', '#define STANDS_H 97', f'static const unsigned char stands_rgb[{len(raw)}] = {{']
out += ['    ' + ','.join(str(b) for b in raw[i:i + 48]) + ',' for i in range(0, len(raw), 48)]
out.append('};')
open(os.path.join(HERE, '..', 'stands.h'), 'w').write('\n'.join(out) + '\n')
print(len(raw), 'bytes -> stands.h')
