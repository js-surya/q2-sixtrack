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

def pack(frames, scale, view, anchor, number=None):
    """frames: name -> (svg text, torso turn); view: the view box (x, y, w, h); anchor: where the frame
    stands. Returns the table rows, the names and the pixel words."""
    vx, vy, vw, vh = view
    W, H = round(vw * scale), round(vh * scale)
    names, table, words = [], [], []
    for name, (svg, turn) in frames.items():
        raw = render(svg, W, H)
        px = [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]
        drawn = [(x, y) for y in range(H) for x in range(W) if px[y * W + x][3] > 6]
        x0, x1 = min(p[0] for p in drawn), max(p[0] for p in drawn) + 1
        y0, y1 = min(p[1] for p in drawn), max(p[1] for p in drawn) + 1
        cw, ch, off = x1 - x0, y1 - y0, len(words)
        lit, run = [], 0
        for y in range(y0, y1):
            for x in range(x0, x1):
                r, g, b, a = px[y * W + x]
                if a <= 6:
                    if lit:
                        words.append(len(lit))
                        words.extend(lit)
                        lit = []
                    run += 1
                else:
                    if run:
                        words.append(0x80000000 | run)
                        run = 0
                    lit.append((a << 24) | (r << 16) | (g << 8) | b)
        if lit:
            words.append(len(lit))
            words.extend(lit)
        if run:
            words.append(0x80000000 | run)
        ax, ay = (anchor[0] - vx) * scale - x0, (anchor[1] - vy) * scale - y0
        nx = ny = 0
        if number:
            num = gb.rot(number, turn)
            nx, ny = (num[0] - vx) * scale - x0 - ax, (num[1] - vy) * scale - y0 - ay
        names.append(name)
        table.append(f'    {{ {cw}, {ch}, {round(ax)}, {round(ay)}, {round(nx)}, {round(ny)}, %d }}, /* {name} */' % off)
    return names, table, words


def main():
    import gen_people as gp
    bnames, btable, bwords = pack({n: (gb.frame(**kw), kw.get('torso', 0)) for n, kw in gb.FRAMES.items()},
                                  128 / 300, (-50, -40, 300, 340), gb.BACK_FOOT, gb.NUMBER)
    pnames, ptable, pwords = pack({n: (gp.figure(kit, **kw), 0) for n, (kit, kw) in gp.FRAMES.items()},
                                  PEOPLE_SCALE, (-20, -20, 240, 320), gp.FEET)
    out = ['/* art/pack.py: the players\' frames, from art/gen_batter.py and art/gen_people.py (original artwork).',
           ' * Do not edit. Each: its size, where it stands (the anchor), the shirt number\'s middle from the',
           ' * anchor (the batter\'s), and where its run-length coded pixels start. */',
           'typedef struct { short w, h, ax, ay, nx, ny; int off; } sprite;',
           'enum { ' + ', '.join('BF_' + n.upper() for n in bnames) + ', BF_N };',
           'enum { ' + ', '.join('PF_' + n.upper() for n in pnames) + ', PF_N };',
           f'#define PEOPLE_H {round(250 * PEOPLE_SCALE)} /* a standing figure\'s height in its frames, px */',
           'static const sprite batter_frames[BF_N] = {', *btable, '};',
           'static const sprite people_frames[PF_N] = {', *ptable, '};']
    for name, words in (('batter_px', bwords), ('people_px', pwords)):
        out.append(f'static const unsigned {name}[{len(words)}] = {{')
        for i in range(0, len(words), 8):
            out.append('    ' + ', '.join(f'0x{w:08x}u' for w in words[i:i + 8]) + ',')
        out.append('};')
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sprites.h')
    open(path, 'w').write('\n'.join(out) + '\n')
    print(len(bnames), 'batter frames,', len(pnames), 'others,', (len(bwords) + len(pwords)) * 4, 'bytes ->', os.path.normpath(path))


PEOPLE_SCALE = 0.3                     # a standing figure 75 px tall, scaled down in the game
main()
