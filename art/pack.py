#!/usr/bin/env python3
"""Pack the batter's frames into the game: ../sprites.h.

Each frame of gen_batter.py is rendered at game size with rsvg-convert, read back as RGBA with
ImageMagick, cropped to what is drawn, and run-length coded (runs of clear pixels skipped, the rest
kept as 0xAARRGGBB). With each frame go where its back foot stands (the anchor) and where the shirt
number's middle is, both from the crop's corner, in pixels."""
import math, os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_batter as gb

SCALE = 128 / 300                      # the stance 128 px tall, as in the approved mock-up
VX, VY = -50, -40                      # the frames' view box corner

def render(svg_text, w, h):
    with tempfile.TemporaryDirectory() as d:
        src, png = os.path.join(d, 'f.svg'), os.path.join(d, 'f.png')
        open(src, 'w').write(svg_text)
        subprocess.run(['rsvg-convert', '-w', str(w), '-h', str(h), src, '-o', png], check=True)
        raw = subprocess.run(['magick', png, '-depth', '8', 'rgba:-'], check=True, capture_output=True).stdout
    return raw

def main():
    W, H = round(300 * SCALE), round(340 * SCALE)
    names, table, words = [], [], []
    for name, kw in gb.FRAMES.items():
        raw = render(gb.frame(**kw), W, H)
        px = [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]
        alpha = lambda x, y: px[y * W + x][3]
        xs = [x for y in range(H) for x in range(W) if alpha(x, y) > 6]
        ys = [y for y in range(H) for x in range(W) if alpha(x, y) > 6]
        x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
        cw, ch = x1 - x0, y1 - y0
        off = len(words)
        lit = []
        run = 0
        def flush_lit():
            if lit:
                words.append(len(lit))
                words.extend(lit)
                lit.clear()
        for y in range(y0, y1):
            for x in range(x0, x1):
                r, g, b, a = px[y * W + x]
                if a <= 6:
                    flush_lit()
                    run += 1
                else:
                    if run:
                        words.append(0x80000000 | run)
                        run = 0
                    lit.append((a << 24) | (r << 16) | (g << 8) | b)
        flush_lit()
        if run:
            words.append(0x80000000 | run)
        ax = (gb.BACK_FOOT[0] - VX) * SCALE - x0
        ay = (gb.BACK_FOOT[1] - VY) * SCALE - y0
        num = gb.rot(gb.NUMBER, kw.get('torso', 0))
        nx, ny = (num[0] - VX) * SCALE - x0 - ax, (num[1] - VY) * SCALE - y0 - ay
        names.append(name)
        table.append(f'    {{ {cw}, {ch}, {round(ax)}, {round(ay)}, {round(nx)}, {round(ny)}, {off} }}, /* {name} */')
    out = ['/* art/pack.py: the batter\'s frames, from art/gen_batter.py (original artwork). Do not edit. */',
           'typedef struct { short w, h, ax, ay, nx, ny; int off; } sprite;',
           'enum { ' + ', '.join('BF_' + n.upper() for n in names) + ', BF_N };',
           'static const sprite batter_frames[BF_N] = {', *table, '};',
           f'static const unsigned batter_px[{len(words)}] = {{']
    for i in range(0, len(words), 8):
        out.append('    ' + ', '.join(f'0x{w:08x}u' for w in words[i:i + 8]) + ',')
    out.append('};')
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sprites.h')
    open(path, 'w').write('\n'.join(out) + '\n')
    print(len(names), 'frames,', len(words) * 4, 'bytes of pixels ->', os.path.normpath(path))

main()
