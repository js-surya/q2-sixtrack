#!/usr/bin/env python3
"""The batter's frames for Six Track, as SVG: the approved stance drawing (art/batter_stance.svg) rigged.

Legs, seat, back and helmet are the stance drawing's own paths. The torso and helmet turn together
about the waist; the arms are drawn for each frame from the shoulders to the gloves; the bat is one
drawing, turned about the gloves. Each frame is written to art/batter/<name>.svg.

Coordinates are the stance drawing's (200 x 300, the batter's back foot at about 125, 285); the frame
is 300 wide with the figure shifted 50 right, so a bat held out either side fits."""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'batter')
INK = '#101833'

DEFS = '''<defs>
    <linearGradient id="shirt" x1="0" y1="0" x2="1" y2="0.3">
      <stop offset="0" stop-color="#5A86F4"/><stop offset="0.55" stop-color="#2E5BDA"/><stop offset="1" stop-color="#1B3A9C"/>
    </linearGradient>
    <linearGradient id="trouser" x1="0" y1="0" x2="1" y2="0.2">
      <stop offset="0" stop-color="#3D63CC"/><stop offset="0.6" stop-color="#2546AE"/><stop offset="1" stop-color="#172F7C"/>
    </linearGradient>
    <linearGradient id="pad" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#FFFFFF"/><stop offset="0.6" stop-color="#EEEEE8"/><stop offset="1" stop-color="#BFC3CF"/>
    </linearGradient>
    <linearGradient id="skin" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#E0AC84"/><stop offset="1" stop-color="#A9744F"/>
    </linearGradient>
    <radialGradient id="helmet" cx="0.35" cy="0.3" r="0.8">
      <stop offset="0" stop-color="#4A63B8"/><stop offset="0.45" stop-color="#1B2A6A"/><stop offset="1" stop-color="#0E1640"/>
    </radialGradient>
    <linearGradient id="wood" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#F0C890"/><stop offset="0.5" stop-color="#D9A464"/><stop offset="1" stop-color="#A87438"/>
    </linearGradient>
    <linearGradient id="glove" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#C8CCD8"/>
    </linearGradient>
  </defs>'''

SHADOW = '<ellipse cx="104" cy="284" rx="58" ry="9" fill="#000" fill-opacity="0.22" stroke="none"/>'

FRONT_LEG = '''
    <path d="M86 160 Q76 186 84 208 L98 206 Q102 184 104 166 Z" fill="url(#trouser)"/>
    <path d="M78 204 Q74 232 77 252 L91 254 Q96 230 96 204 Q88 199 78 204 Z" fill="url(#pad)"/>
    <path d="M81 210 L80 248 M87 208 L87 250" stroke="#B8BCC8" stroke-width="1.4" fill="none"/>
    <path d="M78 222 L96 222 M77 238 L94 238" stroke="#3A3E4C" stroke-width="2.4" fill="none"/>
    <ellipse cx="87" cy="205" rx="10" ry="7" fill="url(#pad)"/>
    <path d="M70 252 Q68 262 76 264 L96 264 Q99 258 93 251 Z" fill="#F7F7F7"/>
    <path d="M70 263 L97 263" stroke="#2A2C36" stroke-width="2.6"/>'''

SEAT = '''
    <path d="M88 150 Q80 166 86 180 Q100 192 122 186 Q138 180 136 160 Q120 146 88 150 Z" fill="url(#trouser)"/>
    <path d="M104 156 Q108 172 104 186" stroke="#162C70" stroke-width="1.4" fill="none"/>'''

BACK_LEG = '''
    <path d="M116 176 Q130 194 132 216 L118 220 Q108 200 104 182 Z" fill="url(#trouser)"/>
    <path d="M114 214 Q112 246 116 272 L132 272 Q136 244 134 214 Q124 209 114 214 Z" fill="url(#pad)"/>
    <path d="M119 220 L120 266 M125 219 L127 267" stroke="#B8BCC8" stroke-width="1.4" fill="none"/>
    <path d="M114 232 L134 232 M114 252 L134 252" stroke="#3A3E4C" stroke-width="2.4" fill="none"/>
    <ellipse cx="124" cy="216" rx="11" ry="8" fill="url(#pad)"/>
    <path d="M110 272 Q107 284 116 286 L140 286 Q144 279 136 271 Z" fill="#F7F7F7"/>
    <path d="M109 285 L141 285" stroke="#2A2C36" stroke-width="2.8"/>'''

TORSO = '''
    <path d="M86 152 Q78 124 86 104 Q100 88 126 92 Q148 98 150 116 Q148 140 138 158 Q114 164 86 152 Z" fill="url(#shirt)"/>
    <path d="M98 110 Q112 120 128 112" stroke="#1C3A9A" stroke-width="1.4" fill="none"/>
    <path d="M100 140 Q116 146 132 140" stroke="#1C3A9A" stroke-width="1.4" fill="none"/>
    <path d="M88 104 Q104 96 120 98" stroke="#F5C030" stroke-width="3.2" fill="none"/>
    <path d="M128 96 Q142 100 148 110" stroke="#F5C030" stroke-width="3.2" fill="none"/>
    <text x="113" y="138" text-anchor="middle" font-family="Helvetica, Arial, sans-serif" font-weight="bold" font-size="22"
          fill="#F5C030" stroke="#101833" stroke-width="1.2">{num}</text>'''

HEAD = '''
    <path d="M104 92 Q108 82 118 82 Q124 86 122 94 Z" fill="url(#skin)"/>
    <path d="M90 72 Q90 48 112 46 Q134 46 136 68 Q136 84 120 88 Q98 90 90 72 Z" fill="url(#helmet)"/>
    <path d="M94 76 Q110 92 130 82 L132 88 Q112 100 92 84 Z" fill="#14205A"/>
    <path d="M92 66 Q112 72 134 64" stroke="#F5C030" stroke-width="3" fill="none"/>
    <path d="M134 56 Q146 58 148 70 L140 72" fill="#1B2A6A"/>
    <path d="M140 64 L146 78 M136 70 L142 82" stroke="#A8ACB8" stroke-width="1.6" fill="none"/>
    <ellipse cx="104" cy="56" rx="7" ry="4" fill="#FFFFFF" fill-opacity="0.25" stroke="none"/>'''

VIEW = '-50 -40 300 340'            # room for a bat held out or up
BACK_FOOT = (125, 285)               # where the frame stands on the ground
NUMBER = (113, 130)                  # the shirt number's middle, on the torso (the game prints it)
WAIST = (112, 158)                    # the torso turns about here
NEAR_SHOULDER = (142, 106)            # the right shoulder, on the torso
FAR_SHOULDER = (92, 104)              # the left, mostly behind the back


def rot(p, deg, about=WAIST):
    a = math.radians(deg)
    x, y = p[0] - about[0], p[1] - about[1]
    return (about[0] + x * math.cos(a) - y * math.sin(a), about[1] + x * math.sin(a) + y * math.cos(a))


def limb(a, b, r0, r1, fill):
    """A tapered limb from a (r0 thick) to b (r1), rounded at both ends, outlined."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy) or 1
    nx, ny = -dy / n, dx / n
    p = lambda q, r, s: f'{q[0] + nx * r * s:.1f} {q[1] + ny * r * s:.1f}'
    return (f'<path d="M{p(a, r0, 1)} L{p(b, r1, 1)} A{r1} {r1} 0 0 0 {p(b, r1, -1)} '
            f'L{p(a, r0, -1)} A{r0} {r0} 0 0 0 {p(a, r0, 1)} Z" fill="{fill}"/>')


def bat(hand, deg, lift=0):
    """The bat, its grip at the gloves, pointing deg off straight down (+ to the right)."""
    return f'''
    <g transform="translate({hand[0]:.1f} {hand[1] - lift:.1f}) rotate({-deg:.1f})">
      <path d="M-3.2 -10 L3.2 -10 L3.2 24 L-3.2 24 Z" fill="#26262C"/>
      <path d="M-6.5 22 L6.5 22 L7.8 96 Q7.8 104 1.5 104 L-1.5 104 Q-7.8 104 -7.8 96 Z" fill="url(#wood)"/>
      <path d="M-4 30 L-5 98" stroke="#F6DDB0" stroke-width="1.6" fill="none"/>
      <path d="M-6.8 28 L6.8 28 L7 37 L-7 37 Z" fill="#2E5BDA" stroke-width="1.4"/>
      <path d="M-7.6 97 Q-7.6 104 -1.5 104 L1.5 104 Q7.6 104 7.6 97" fill="none" stroke="#8A5C2A" stroke-width="2"/>
    </g>'''


def glove(at, deg):
    return f'''
    <g transform="translate({at[0]:.1f} {at[1]:.1f}) rotate({-deg:.1f})">
      <path d="M-8 -6 Q-9 6 -2 9 Q8 10 9 2 Q10 -7 1 -8 Q-6 -9 -8 -6 Z" fill="url(#glove)"/>
      <path d="M-6 -2 L7 -2 M-6 3 L7 3" stroke="#AEB3C2" stroke-width="1.2" fill="none"/>
    </g>'''


def arm(shoulder, hand, out, kit='url(#shirt)'):
    """Shoulder to hand through an elbow bent out to one side (out: +1 right, -1 left)."""
    mx, my = (shoulder[0] + hand[0]) / 2, (shoulder[1] + hand[1]) / 2
    dx, dy = hand[0] - shoulder[0], hand[1] - shoulder[1]
    n = math.hypot(dx, dy) or 1
    bend = max(0.0, 30 - n * 0.35) + 6   # the shorter the reach, the more the elbow bends
    elbow = (mx - dy / n * bend * out, my + dx / n * bend * out)
    cuff = (shoulder[0] + (elbow[0] - shoulder[0]) * 0.62, shoulder[1] + (elbow[1] - shoulder[1]) * 0.62)
    return (limb(cuff, elbow, 6.4, 5.8, 'url(#skin)') + limb(elbow, hand, 6.0, 4.8, 'url(#skin)')
            + limb(shoulder, cuff, 9.5, 8.0, kit))


def frame(hand, bat_deg, torso=0, bat_behind=False, lift=0, head_down=0, num=''):
    """One frame: the near glove at hand, the bat at bat_deg, the torso turned torso degrees."""
    ns, fs = rot(NEAR_SHOULDER, torso), rot(FAR_SHOULDER, torso)
    a = math.radians(bat_deg)
    top = (hand[0] - math.sin(a) * 9, hand[1] - math.cos(a) * 9 - lift)   # the top (left) hand, just above
    low = (hand[0], hand[1] - lift)
    parts = [SHADOW]
    if bat_behind:
        parts.append(bat(hand, bat_deg, lift))
    parts.append(arm(fs, top, -1))                       # the far arm, behind the back
    parts += [FRONT_LEG, SEAT, BACK_LEG]
    parts.append(f'<g transform="rotate({torso} {WAIST[0]} {WAIST[1]})">{TORSO.format(num=num)}'
                 f'<g transform="rotate({head_down} 112 92)">{HEAD}</g></g>')
    if not bat_behind:
        parts.append(bat(hand, bat_deg, lift))
    parts.append(glove(top, bat_deg))
    parts.append(arm(ns, low, 1))
    parts.append(glove(low, bat_deg))
    body = '\n'.join(parts)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{VIEW}" width="300" height="340">\n  {DEFS}\n'
            f'  <g stroke="{INK}" stroke-width="2.2" stroke-linejoin="round" stroke-linecap="round">\n{body}\n  </g>\n</svg>\n')


# The frames: the near glove, the bat's angle (0 down, + toward the right, 180 straight up), the torso's turn.
FRAMES = {
    'stance': dict(hand=(129, 170), bat_deg=8, bat_behind=True),
    'tap': dict(hand=(129, 170), bat_deg=8, bat_behind=True, lift=5),
    'lift1': dict(hand=(138, 146), bat_deg=120, torso=-2),
    'lift2': dict(hand=(144, 128), bat_deg=160, torso=-4),
    'down1': dict(hand=(142, 138), bat_deg=110, torso=-2),
    'down2': dict(hand=(134, 156), bat_deg=55, torso=0),
    # each shot: contact, follow-through, finish
    'glance0': dict(hand=(118, 168), bat_deg=-30, torso=-3),
    'glance1': dict(hand=(110, 160), bat_deg=-60, torso=-5),
    'glance2': dict(hand=(104, 150), bat_deg=-85, torso=-6),
    'pull0': dict(hand=(114, 142), bat_deg=-90, torso=-8),
    'pull1': dict(hand=(100, 126), bat_deg=-130, torso=-12),
    'pull2': dict(hand=(90, 114), bat_deg=-165, torso=-14),
    'drive0': dict(hand=(120, 172), bat_deg=0, torso=2),
    'drive1': dict(hand=(104, 140), bat_deg=-115, torso=-4),
    'drive2': dict(hand=(88, 110), bat_deg=-160, torso=-8),
    'cover0': dict(hand=(140, 156), bat_deg=70, torso=4),
    'cover1': dict(hand=(146, 132), bat_deg=125, torso=6),
    'cover2': dict(hand=(148, 116), bat_deg=150, torso=6),
    'dab0': dict(hand=(136, 168), bat_deg=38, torso=2),
    'dab1': dict(hand=(140, 164), bat_deg=55, torso=3),
    'dab2': dict(hand=(142, 162), bat_deg=62, torso=3),
    'leave': dict(hand=(120, 86), bat_deg=178, torso=-2),
    'out': dict(hand=(128, 172), bat_deg=4, bat_behind=True, head_down=10, torso=4),
}

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, kw in FRAMES.items():
        with open(os.path.join(OUT, name + '.svg'), 'w') as f:
            f.write(frame(**kw))
    print(len(FRAMES), 'frames in', OUT)
