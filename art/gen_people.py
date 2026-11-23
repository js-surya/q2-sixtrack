#!/usr/bin/env python3
"""Six Track's other players, facing us, as SVG: fielders, the bowler, the umpire, the non-striker.

One rig after the approved fielder drawing (art/fielder_ready.svg): its shirt, face and cap, with the
legs and arms drawn for each frame from hip to knee to foot and shoulder to elbow to hand. The kit
follows the Laws: fielders and the bowler in cap, shirt, trousers and spikes, no pads; the umpire in
his coat and hat; the non-striker in helmet, pads and gloves with his bat. Coordinates are 200 x 300,
the feet on the ground at y 286, the middle at x 100; his right is our left."""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'people')
VIEW = '-20 -20 240 320'
FEET = (100, 286)

KITS = {
    'field': dict(shirt=('#4FD08A', '#21A560', '#146C3E'), trouser=('#2E9C5C', '#1D7C46', '#125530'), ink='#0F2018',
                  collar='#F4F4F4', head='cap', capc=('#3BAE6E', '#18804A', '#0E5530'), hands='skin', pads=False),
    'umpire': dict(shirt=('#FFFFFF', '#ECEEF2', '#B8BEC9'), trouser=('#3A3D46', '#26282F', '#16171C'), ink='#14161C',
                   collar='#2A2D36', head='hat', capc=('#3A3D46', '#202228', '#101114'), hands='skin', pads=False),
    'bat': dict(shirt=('#5A86F4', '#2E5BDA', '#1B3A9C'), trouser=('#3D63CC', '#2546AE', '#172F7C'), ink='#101833',
                collar='#F5C030', head='helmet', capc=('#4A63B8', '#1B2A6A', '#0E1640'), hands='glove', pads=True),
}


def defs(k):
    s, t, c = k['shirt'], k['trouser'], k['capc']
    return f'''<defs>
    <linearGradient id="shirt" x1="0" y1="0" x2="1" y2="0.3"><stop offset="0" stop-color="{s[0]}"/><stop offset="0.55" stop-color="{s[1]}"/><stop offset="1" stop-color="{s[2]}"/></linearGradient>
    <linearGradient id="trouser" x1="0" y1="0" x2="1" y2="0.2"><stop offset="0" stop-color="{t[0]}"/><stop offset="0.6" stop-color="{t[1]}"/><stop offset="1" stop-color="{t[2]}"/></linearGradient>
    <linearGradient id="skin" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#D9A07A"/><stop offset="1" stop-color="#9C6844"/></linearGradient>
    <radialGradient id="face" cx="0.38" cy="0.35" r="0.75"><stop offset="0" stop-color="#E6B08A"/><stop offset="1" stop-color="#A26E4A"/></radialGradient>
    <radialGradient id="cap" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="{c[0]}"/><stop offset="0.5" stop-color="{c[1]}"/><stop offset="1" stop-color="{c[2]}"/></radialGradient>
    <linearGradient id="pad" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#FFFFFF"/><stop offset="0.6" stop-color="#EEEEE8"/><stop offset="1" stop-color="#BFC3CF"/></linearGradient>
    <linearGradient id="glove" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#C8CCD8"/></linearGradient>
    <linearGradient id="wood" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#F0C890"/><stop offset="0.5" stop-color="#D9A464"/><stop offset="1" stop-color="#A87438"/></linearGradient>
  </defs>'''


def limb(a, b, r0, r1, fill):
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy) or 1
    nx, ny = -dy / n, dx / n
    p = lambda q, r, s: f'{q[0] + nx * r * s:.1f} {q[1] + ny * r * s:.1f}'
    return (f'<path d="M{p(a, r0, 1)} L{p(b, r1, 1)} A{r1} {r1} 0 0 0 {p(b, r1, -1)} '
            f'L{p(a, r0, -1)} A{r0} {r0} 0 0 0 {p(a, r0, 1)} Z" fill="{fill}"/>')


def mid(a, b, t=0.5):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def leg(hip, knee, foot, k, side):
    out = [limb(hip, knee, 12, 10, 'url(#trouser)')]
    ankle = (foot[0], foot[1] - 8)
    if k['pads']:
        top = mid(hip, knee, 0.7)
        out.append(limb(top, ankle, 11, 9, 'url(#pad)'))
        for t in (0.45, 0.75):
            m = mid(knee, ankle, t)
            out.append(f'<path d="M{m[0] - 10:.1f} {m[1]:.1f} L{m[0] + 10:.1f} {m[1]:.1f}" stroke="#3A3E4C" stroke-width="2.4" fill="none"/>')
        out.append(f'<ellipse cx="{knee[0]:.1f}" cy="{knee[1]:.1f}" rx="11" ry="8" fill="url(#pad)"/>')
    else:
        out.append(limb(knee, ankle, 10, 8, 'url(#trouser)'))
    toe = 10 * side
    out.append(f'<path d="M{foot[0] - 8 - max(toe, 0) * 0.3:.1f} {foot[1] - 10:.1f} Q{foot[0] - 14 + min(toe, 0):.1f} {foot[1]:.1f} '
               f'{foot[0] - 6:.1f} {foot[1]:.1f} L{foot[0] + 10 + max(toe, 0):.1f} {foot[1]:.1f} Q{foot[0] + 14:.1f} {foot[1] - 6:.1f} '
               f'{foot[0] + 6:.1f} {foot[1] - 11:.1f} Z" fill="#F7F7F7"/>')
    out.append(f'<path d="M{foot[0] - 13:.1f} {foot[1] - 0.5:.1f} L{foot[0] + 14:.1f} {foot[1] - 0.5:.1f}" stroke="#24282E" stroke-width="2.6"/>')
    return out


def arm(shoulder, hand, k, out):
    mx, my = mid(shoulder, hand)
    dx, dy = hand[0] - shoulder[0], hand[1] - shoulder[1]
    n = math.hypot(dx, dy) or 1
    bend = max(0.0, 26 - n * 0.3) + 4
    elbow = (mx - dy / n * bend * out, my + dx / n * bend * out)
    cuff = mid(shoulder, elbow, 0.6)
    skin = 'url(#shirt)' if k['head'] == 'hat' else 'url(#skin)'   # the umpire's coat to the wrist
    parts = [limb(cuff, elbow, 7.4, 6.8, skin), limb(elbow, hand, 7, 5.6, skin), limb(shoulder, cuff, 11, 9.2, 'url(#shirt)')]
    if k['hands'] == 'glove':
        parts.append(f'<ellipse cx="{hand[0]:.1f}" cy="{hand[1]:.1f}" rx="9" ry="8" fill="url(#glove)"/>')
    else:
        parts.append(f'<ellipse cx="{hand[0]:.1f}" cy="{hand[1]:.1f}" rx="7" ry="6.5" fill="url(#skin)"/>')
    return parts


def torso(k, dy):
    collar = k['collar']
    body = (f'<path d="M70 {176 + dy} Q62 {140 + dy} 70 {112 + dy} Q84 {96 + dy} 100 {96 + dy} Q116 {96 + dy} 130 {112 + dy} '
            f'Q138 {140 + dy} 130 {176 + dy} Q100 {186 + dy} 70 {176 + dy} Z" fill="url(#shirt)"/>')
    if k['head'] == 'hat':   # the umpire's coat: open down the front, buttons
        return [body, f'<path d="M100 {104 + dy} L100 {180 + dy}" stroke="#9AA0AC" stroke-width="1.6" fill="none"/>',
                f'<path d="M88 {102 + dy} L100 {116 + dy} L112 {102 + dy}" stroke="{collar}" stroke-width="3" fill="none"/>',
                *[f'<circle cx="104" cy="{y + dy}" r="1.8" fill="#9AA0AC" stroke-width="0.8"/>' for y in (128, 146, 164)]]
    return [body, f'<path d="M86 {104 + dy} L100 {118 + dy} L114 {104 + dy}" stroke="{collar}" stroke-width="3" fill="none"/>',
            f'<path d="M100 {118 + dy} L100 {140 + dy}" stroke="#000" stroke-opacity="0.25" stroke-width="1.4" fill="none"/>',
            f'<circle cx="114" cy="{132 + dy}" r="5" fill="{collar}" stroke-width="1.4"/>',
            f'<path d="M80 {160 + dy} Q100 {166 + dy} 120 {160 + dy}" stroke="#000" stroke-opacity="0.25" stroke-width="1.4" fill="none"/>']


def head(k, dy, tilt=0):
    y = 72 + dy
    g = [f'<g transform="rotate({tilt} 100 {y + 20})">',
         f'<path d="M92 {98 + dy} L92 {88 + dy} L108 {88 + dy} L108 {98 + dy} Z" fill="url(#skin)"/>',
         f'<ellipse cx="100" cy="{y}" rx="19" ry="21" fill="url(#face)"/>',
         f'<ellipse cx="81" cy="{y + 2}" rx="4" ry="6" fill="url(#skin)"/><ellipse cx="119" cy="{y + 2}" rx="4" ry="6" fill="url(#skin)"/>',
         f'<ellipse cx="93" cy="{y + 2}" rx="2.6" ry="3.2" fill="#1E1612" stroke="none"/><ellipse cx="107" cy="{y + 2}" rx="2.6" ry="3.2" fill="#1E1612" stroke="none"/>',
         f'<path d="M88 {y - 5} Q93 {y - 8} 97 {y - 5} M103 {y - 5} Q107 {y - 8} 112 {y - 5}" stroke="#3A2618" stroke-width="1.6" fill="none"/>',
         f'<path d="M95 {y + 12} Q100 {y + 15} 105 {y + 12}" stroke="#6E3E26" stroke-width="1.6" fill="none"/>']
    if k['head'] == 'cap':
        g += [f'<path d="M80 {y - 10} Q80 {y - 32} 100 {y - 34} Q120 {y - 32} 120 {y - 10} Z" fill="url(#cap)"/>',
              f'<path d="M78 {y - 10} Q100 {y + 2} 122 {y - 10} Q118 {y - 18} 100 {y - 16} Q82 {y - 18} 78 {y - 10} Z" fill="#0E5530"/>',
              f'<circle cx="100" cy="{y - 33}" r="2.4" fill="#F4F4F4" stroke-width="1"/>']
    elif k['head'] == 'hat':
        g += [f'<ellipse cx="100" cy="{y - 12}" rx="34" ry="8" fill="url(#cap)"/>',
              f'<path d="M82 {y - 12} Q82 {y - 34} 100 {y - 35} Q118 {y - 34} 118 {y - 12} Z" fill="url(#cap)"/>',
              f'<path d="M82 {y - 17} Q100 {y - 13} 118 {y - 17}" stroke="#55585F" stroke-width="2.4" fill="none"/>']
    else:   # the batting helmet from the front: dome, peak, the grille over the face
        g += [f'<path d="M78 {y + 2} Q78 {y - 34} 100 {y - 36} Q122 {y - 34} 122 {y + 2} Z" fill="url(#cap)"/>',
              f'<path d="M76 {y - 6} Q100 {y + 2} 124 {y - 6} L122 {y - 12} Q100 {y - 6} 78 {y - 12} Z" fill="#14205A"/>',
              f'<path d="M84 {y - 2} L84 {y + 18} Q100 {y + 26} 116 {y + 18} L116 {y - 2}" fill="none" stroke="#A8ACB8" stroke-width="2"/>',
              f'<path d="M84 {y + 8} L116 {y + 8} M100 {y - 2} L100 {y + 22}" stroke="#A8ACB8" stroke-width="1.6" fill="none"/>',
              f'<path d="M80 {y - 18} Q100 {y - 12} 120 {y - 18}" stroke="#F5C030" stroke-width="3" fill="none"/>']
    g.append('</g>')
    return g


def figure(kit, legs, arms, crouch=0, tilt=0, bat=None, finger=False):
    """legs: ((knee, foot) for his right, for his left); arms: (hand his right, hand his left);
    bat: (hand, toe) for the non-striker."""
    k = KITS[kit]
    dy = crouch
    hipR, hipL = (88, 172 + dy), (112, 172 + dy)
    shR, shL = (74, 114 + dy), (126, 114 + dy)
    parts = ['<ellipse cx="100" cy="287" rx="46" ry="7" fill="#000" fill-opacity="0.22" stroke="none"/>']
    if bat:
        h, t = bat
        parts.append(limb(h, t, 6.5, 7.5, 'url(#wood)'))
    parts += leg(hipR, legs[0][0], legs[0][1], k, -1)
    parts += leg(hipL, legs[1][0], legs[1][1], k, 1)
    if kit == 'bat':
        parts.append(limb((80, 176 + dy), (120, 176 + dy), 12, 12, 'url(#trouser)'))
    parts += torso(k, dy)
    parts += arm(shR, arms[0], k, -1)
    parts += arm(shL, arms[1], k, 1)
    parts += head(k, dy, tilt)
    if finger:   # the umpire's finger raised: out
        x, y = arms[1]
        parts.append(f'<path d="M{x:.1f} {y - 4:.1f} L{x:.1f} {y - 16:.1f}" stroke-width="5" stroke="{k["ink"]}"/>'
                     f'<path d="M{x:.1f} {y - 4:.1f} L{x:.1f} {y - 15:.1f}" stroke-width="3" stroke="#D9A07A"/>')
    body = '\n    '.join(parts)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{VIEW}" width="240" height="320">\n  {defs(k)}\n'
            f'  <g stroke="{k["ink"]}" stroke-width="2.4" stroke-linejoin="round" stroke-linecap="round">\n    {body}\n  </g>\n</svg>\n')


STAND_LEGS = (((84, 230), (80, 286)), ((116, 230), (120, 286)))


def ready(c):
    return dict(legs=(((78, 228 + c * 0.3), (72, 286)), ((122, 228 + c * 0.3), (128, 286))), arms=((80, 206 + c), (120, 206 + c)), crouch=c)


def run(ph):
    s = math.sin(ph)
    up = lambda v: 220 + 14 * v
    return dict(legs=(((86, up(-s)), (86 - 4 * s, 286 - max(0, 22 * s))), ((114, up(s)), (114 + 4 * s, 286 - max(0, -22 * s)))),
                arms=((70 - 6 * s, 160 + 22 * s), (130 + 6 * s, 160 - 22 * s)), crouch=6)


FRAMES = {
    'f_ready0': ('field', ready(10)),
    'f_ready1': ('field', ready(6)),
    **{f'f_run{i}': ('field', run(i * math.pi / 2)) for i in range(4)},
    'f_catch': ('field', dict(legs=STAND_LEGS, arms=((94, 46), (106, 46)), crouch=2)),
    'b_bowl0': ('field', dict(legs=STAND_LEGS, arms=((58, 196), (132, 64)), crouch=4, tilt=-4)),
    'b_bowl1': ('field', dict(legs=STAND_LEGS, arms=((86, 22), (134, 178)), crouch=0, tilt=4)),
    'b_bowl2': ('field', dict(legs=(((84, 226), (82, 286)), ((122, 238), (132, 280))), arms=((132, 206), (150, 150)), crouch=10, tilt=8)),
    'u_stand': ('umpire', dict(legs=STAND_LEGS, arms=((94, 196), (106, 196)))),
    'u_six': ('umpire', dict(legs=STAND_LEGS, arms=((66, 26), (134, 26)))),
    'u_four0': ('umpire', dict(legs=STAND_LEGS, arms=((92, 196), (176, 132)))),
    'u_four1': ('umpire', dict(legs=STAND_LEGS, arms=((92, 196), (174, 104)))),
    'u_out': ('umpire', dict(legs=STAND_LEGS, arms=((92, 196), (128, 40)), finger=True)),
    'n_stand': ('bat', dict(legs=STAND_LEGS, arms=((104, 188), (110, 182)), bat=((110, 186), (126, 282)))),
    'n_jog0': ('bat', dict(legs=(((84, 222), (84, 274)), ((116, 236), (118, 286))), arms=((100, 176), (108, 170)), bat=((106, 174), (150, 252)), crouch=4)),
    'n_jog1': ('bat', dict(legs=(((84, 236), (82, 286)), ((116, 222), (116, 274))), arms=((100, 176), (108, 170)), bat=((106, 174), (150, 252)), crouch=4)),
}

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, (kit, kw) in FRAMES.items():
        with open(os.path.join(OUT, name + '.svg'), 'w') as f:
            f.write(figure(kit, **kw))
    print(len(FRAMES), 'frames in', OUT)
