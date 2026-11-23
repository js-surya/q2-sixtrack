#!/usr/bin/env python3
"""Six Track's stands, as one SVG panorama across the top of the screen (375 x 97, the field from y 97):
a night sky, floodlight towers, the roof canopy and its lights, three curved tiers of seats with aisles
and a crowd of little supporters (mostly the batting side's blue, some green, flags and arms up), LED
boards on the tier fronts and at the boundary, and the black sightscreen behind the bowler. Original
artwork; the crowd is placed by a fixed seed, so it is the same each build. Writes art/stands.svg."""
import math
import os
import random

W, H = 375, 97
CX = 187.5
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'stands.svg')
rnd = random.Random(1976)

SHIRTS = ['#2E5BDA'] * 7 + ['#3D6FF0'] * 3 + ['#21A560'] * 3 + ['#F5C030', '#FFFFFF', '#E04848', '#8A4FD8']
SKINS = ['#E8B892', '#D49A72', '#B57A55', '#8E5A3A', '#F2CDB0']
SEATS = [('#1F8C8E', '#176A6C'), ('#D8742E', '#A85820'), ('#B83C84', '#8A2C63')]

# tiers: (top y, bottom y at the middle, how much the ends rise, seat colours)
TIERS = [(36, 52, 3, SEATS[0]), (55, 71, 3, SEATS[1]), (74, 89, 3, SEATS[2])]
SIGHT = (CX - 27, 47, CX + 27, 92)   # the sightscreen's box


def curve(y, rise):
    """A tier's line: lower in the middle, rising toward the ends (the bowl seen from the pitch)."""
    return f'M0 {y - rise} Q{CX} {y + rise} {W} {y - rise}'


def y_at(x, y, rise):
    t = x / W
    return (1 - t) ** 2 * (y - rise) + 2 * (1 - t) * t * (y + rise) + t * t * (y - rise)


def supporter(x, y, s):
    """A little supporter: shoulders in a shirt, a head, now and then arms up or a flag."""
    shirt, skin = rnd.choice(SHIRTS), rnd.choice(SKINS)
    parts = [f'<path d="M{x - 2.1 * s:.2f} {y + 2.6 * s:.2f} Q{x - 2.1 * s:.2f} {y + 0.6 * s:.2f} {x:.2f} {y + 0.5 * s:.2f} '
             f'Q{x + 2.1 * s:.2f} {y + 0.6 * s:.2f} {x + 2.1 * s:.2f} {y + 2.6 * s:.2f} Z" fill="{shirt}"/>',
             f'<circle cx="{x:.2f}" cy="{y - 0.6 * s:.2f}" r="{1.25 * s:.2f}" fill="{skin}"/>']
    r = rnd.random()
    if r < 0.09:   # arms up
        parts.append(f'<path d="M{x - 1.6 * s:.2f} {y + 1 * s:.2f} L{x - 2.4 * s:.2f} {y - 2.6 * s:.2f} M{x + 1.6 * s:.2f} {y + 1 * s:.2f} '
                     f'L{x + 2.4 * s:.2f} {y - 2.6 * s:.2f}" stroke="{skin}" stroke-width="{0.8 * s:.2f}" stroke-linecap="round"/>')
    elif r < 0.13:   # a flag
        flag = rnd.choice(['#2E5BDA', '#F5C030', '#21A560', '#FFFFFF'])
        parts.append(f'<path d="M{x + 1.4 * s:.2f} {y + 1 * s:.2f} L{x + 1.4 * s:.2f} {y - 4.5 * s:.2f}" stroke="#DDD" stroke-width="{0.4 * s:.2f}"/>'
                     f'<path d="M{x + 1.4 * s:.2f} {y - 4.5 * s:.2f} l{3.4 * s:.2f} {0.8 * s:.2f} l{-3.4 * s:.2f} {1.6 * s:.2f} Z" fill="{flag}"/>')
    elif r < 0.2 and shirt == '#2E5BDA':   # a cap
        parts.append(f'<path d="M{x - 1.3 * s:.2f} {y - 1 * s:.2f} Q{x:.2f} {y - 2.6 * s:.2f} {x + 1.3 * s:.2f} {y - 1 * s:.2f} Z" fill="#F5C030"/>')
    return ''.join(parts)


def tier(top, bottom, rise, seat, back):
    a, b = seat
    out = [f'<path d="{curve(top, rise)} L{W} {bottom - rise} Q{CX} {bottom + rise} 0 {bottom - rise} Z" fill="{b}"/>']
    rows = 4
    for i in range(rows):   # the seat rows, a shade each
        y0 = top + (bottom - top) * i / rows
        y1 = top + (bottom - top) * (i + 0.55) / rows
        out.append(f'<path d="{curve(y0, rise)} L{W} {y1 - rise} Q{CX} {y1 + rise} 0 {y1 - rise} Z" fill="{a}" fill-opacity="0.85"/>')
    for x in range(14, W, 58):   # aisles and their steps
        yt, yb = y_at(x, top, rise), y_at(x, bottom, rise)
        out.append(f'<path d="M{x} {yt:.1f} L{x + 2} {yb:.1f}" stroke="#2A2C36" stroke-width="3"/>')
        for k in range(1, 5):
            yy = yt + (yb - yt) * k / 5
            out.append(f'<path d="M{x - 1.2} {yy:.1f} L{x + 3} {yy:.1f}" stroke="#4A4E5C" stroke-width="0.7"/>')
    for i in range(rows):   # the crowd, row by row, back to front
        yrow = top + (bottom - top) * (i + 0.62) / rows
        s = 0.9 + 0.1 * i + (0.1 if back else 0.2)
        x = rnd.uniform(1, 4)
        while x < W:
            yy = y_at(x, yrow, rise)
            inside_sight = SIGHT[0] - 3 < x < SIGHT[2] + 3 and yy > SIGHT[1] - 4
            on_aisle = any(abs(x - (ax + 1)) < 2.6 for ax in range(14, W, 58))
            if not inside_sight and not on_aisle and rnd.random() < 0.86:
                out.append(supporter(x, yy, s))
            x += rnd.uniform(4.2, 5.4) * s
    # the tier's front: a fascia with an LED board
    out.append(f'<path d="{curve(bottom, rise)} L{W} {bottom - rise + 3} Q{CX} {bottom + rise + 3} 0 {bottom - rise + 3} Z" fill="#14161E"/>')
    return out


def led(y0, y1, k):
    """An LED board: blocks of colour with abstract patterns and the game's name, no brands."""
    out = []
    cols = ['#173A8C', '#0E7C82', '#B8501E', '#5A2C9C']
    for i, x in enumerate(range(0, W, 47)):
        c = cols[(i + k) % 4]
        out.append(f'<rect x="{x}" y="{y0}" width="47" height="{y1 - y0}" fill="{c}"/>')
        if (i + k) % 3 == 0:
            out.append(f'<text x="{x + 23.5}" y="{y1 - 1.3}" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" '
                       f'font-weight="bold" font-size="{(y1 - y0) * 0.85:.1f}" fill="#FFFFFF" fill-opacity="0.9">SIX TRACK</text>')
        else:
            for j in range(4):
                out.append(f'<circle cx="{x + 8 + j * 10}" cy="{(y0 + y1) / 2:.1f}" r="{(y1 - y0) * 0.25:.1f}" fill="#FFFFFF" fill-opacity="0.35"/>')
    out.append(f'<rect x="0" y="{y0}" width="{W}" height="0.8" fill="#AFC4FF" fill-opacity="0.7"/>')
    return out


def tower(x, lean):
    lamps = ''.join(f'<circle cx="{x - 11 + 7.3 * i:.1f}" cy="8" r="2.6" fill="#FFF8DC"/>' for i in range(4))
    return [f'<path d="M{x} 10 L{x + lean} 92" stroke="#3C4050" stroke-width="3.2"/>',
            f'<path d="M{x - 1} 30 L{x + lean * 0.4 + 2} 50 M{x + 1} 50 L{x + lean * 0.7 - 2} 70" stroke="#3C4050" stroke-width="1"/>',
            f'<rect x="{x - 17}" y="2" width="34" height="12" rx="3" fill="#30343F" stroke="#1A1C24" stroke-width="1"/>', lamps,
            f'<circle cx="{x}" cy="8" r="40" fill="url(#lampglow)"/>']


def main():
    p = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 375 97" width="375" height="97">',
         '<defs><linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0A1030"/>'
         '<stop offset="1" stop-color="#223466"/></linearGradient>'
         '<radialGradient id="lampglow"><stop offset="0" stop-color="#FFF0C0" stop-opacity="0.55"/>'
         '<stop offset="1" stop-color="#FFF0C0" stop-opacity="0"/></radialGradient>'
         '<linearGradient id="roof" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#262A38"/>'
         '<stop offset="1" stop-color="#14161E"/></linearGradient></defs>',
         f'<rect width="{W}" height="{H}" fill="url(#sky)"/>']
    for _ in range(40):   # stars
        p.append(f'<circle cx="{rnd.uniform(0, W):.1f}" cy="{rnd.uniform(0, 24):.1f}" r="{rnd.uniform(0.3, 0.75):.2f}" fill="#FFFFFF" '
                 f'fill-opacity="{rnd.uniform(0.4, 0.9):.2f}"/>')
    # the roof canopy, curving with the bowl, its lights underneath
    p.append(f'<path d="M0 22 Q{CX} 30 {W} 22 L{W} 33 Q{CX} 41 0 33 Z" fill="url(#roof)"/>')
    p.append(f'<path d="M0 33 Q{CX} 41 {W} 33" stroke="#5A6078" stroke-width="1" fill="none"/>')
    for x in range(8, W, 16):
        p.append(f'<circle cx="{x}" cy="{y_at(x, 33, -4) - 1.5:.1f}" r="1" fill="#FFF0C8"/>')
    for k, (top, bottom, rise, seat) in enumerate(TIERS):
        p += tier(top, bottom, rise, seat, k == 0)
        if k < 2:
            p += led(bottom + 0.5, bottom + 3.5, k)
    p += led(91, 97, 2)   # the boundary boards
    x0, y0, x1, y1 = SIGHT
    p.append(f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" fill="#101014" stroke="#5A5E6E" stroke-width="1"/>')
    p += tower(31, -6) + tower(344, 6)
    p.append('</svg>')
    open(OUT, 'w').write('\n'.join(p) + '\n')
    print('->', OUT)


main()
