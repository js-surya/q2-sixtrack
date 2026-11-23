#!/usr/bin/env python3
"""Pack the menu's wordmark (art/logo.svg, from art/gen_logo.py) into ../logo.h: 300 x 74 RGBA,
rendered with rsvg-convert, read back with ImageMagick."""
import os, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 300, 74
with tempfile.TemporaryDirectory() as d:
    png = os.path.join(d, 'logo.png')
    subprocess.run(['rsvg-convert', '-w', str(W), '-h', str(H), os.path.join(HERE, 'logo.svg'), '-o', png], check=True)
    raw = subprocess.run(['magick', png, '-depth', '8', 'rgba:-'], check=True, capture_output=True).stdout
assert len(raw) == W * H * 4
out = ['/* art/pack_logo.py: the menu\'s wordmark, RGBA, from art/gen_logo.py (original artwork). Do not edit. */',
       f'#define LOGO_W {W}', f'#define LOGO_H {H}', f'static const unsigned char logo_rgba[{len(raw)}] = {{']
out += ['    ' + ','.join(str(b) for b in raw[i:i + 48]) + ',' for i in range(0, len(raw), 48)]
out.append('};')
open(os.path.join(HERE, '..', 'logo.h'), 'w').write('\n'.join(out) + '\n')
print(len(raw), 'bytes -> logo.h')
