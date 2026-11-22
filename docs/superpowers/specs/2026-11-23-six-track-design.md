# Six Track: design

An original one-button cricket batting game for the Shanling Q2, as a Q2 Pod card game.
Named *Six Track*: a six-track EP, and sixes.

## Intent

A quick, colourful pick-up-and-play game: you bat in a stadium under evening floodlights, seen from
behind the batter, time one button to the ball, and hit it to every part of the ground. Inspired by
cricket games and TV cricket in general; it copies no particular game's name, art, characters,
score panels or look (no stick figures).

## Goals

- **One input.** Centre, or a tap anywhere on the screen, swings the bat. Nothing else in play.
- **Timing decides everything:** where the ball goes (all around the ground) and how far.
- **One fixed screen.** No camera moves: the stadium and field are still; only the ball, bowler,
  umpire, batter, fielders and stumps animate.
- **Full colour**, in our own cartoon style.
- **Two modes:** Endless (best score, three wickets) and Chase (a target in a set number of balls,
  harder level by level).
- **Music keeps playing** while you bat; Play/Pause and the side buttons stay the music's.
- Installs from the App Store's Games tab like the other card games.

## Non-goals (v1)

- Sound effects (they would mean taking the Q2's audio from the music).
- Bowling, choosing shots, running between wickets by hand, multiplayer, online scores.
- Wheel or other buttons during play (the wheel works only in menus).
- Camera movement, replays.

## Anti-goals

- Looking or playing like a specific existing game: our own name, art, panels and rules.
- Timing that feels unfair: a dropped frame or input lag that turns a perfect press into a miss.
- A busy screen that hides the ball or the timing strip.

## Constraints (Q2 Pod's card games)

- `q2app` with `name=` and `game=1`; the program `.sixtrack/q2-sixtrack` on the card. Q2 Pod starts it
  from Library → Games and lists it under the App Store's Games tab.
- It draws on `/dev/fb0` itself: 320 × 375, 32 bpp, two pages; the 375 × 320 landscape view is stored
  turned clockwise.
- Input arrives as text datagrams on `/tmp/q2-sixtrack.input.sock`: `k d CODE` / `k u CODE` (Centre 218,
  Return 170), `t d|m|u X Y` in landscape pixels, `w N` wheel travel.
- Holding Return quits (Q2 Pod TERMs the game); 2 minutes without input closes it.
- No libraries on the Q2: raw MIPS o32 syscalls, software rendering, built with `clang` + `ld.lld`
  as Buddy (`DiamondBond/q2-fidget`, MIT) is.

## Approaches considered

- **A. C, on Buddy's Q2 layer (chosen).** Reuse Buddy's syscalls, framebuffer, input socket, theme
  pop-up colours and `-DHOST` preview build, with credit (MIT). Small binary, steady frames: right
  for a game decided by milliseconds.
- **B. Go, on the Tailscale app's build.** Quicker to write, but a ~2 MB binary and GC pauses that
  could make a perfect swing feel off.
- Porting an existing cricket game was looked at: the close matches on GitHub have no licence, and
  the licensed ones are Unity 3D or text games. Commercial games are not an option.

## The screen (landscape 375 × 320, fixed)

Seen from behind the batter, looking down the pitch:

- **Still (drawn once, kept as the background):** evening sky, floodlight towers, the stands with a
  coloured crowd, boundary rope and advertising-free boards, the green outfield with mowing stripes,
  the tan pitch in perspective narrowing toward the bowler's end, crease lines.
- **Animated:** the batter (bottom centre, from behind: stance, backlift, swing, follow-through),
  the stumps behind the batter (still, shattered and bails flying when bowled), the bowler (run-up,
  delivery), the umpire at the bowler's end (still, signals: four, six, out), the fielders in the
  deep (idle, run toward a ball, catch, throw), and the ball (from the bowler's hand toward the
  batter, growing as it nears, then off the bat across the field).
- **Overlays:** a score panel top left (runs/wickets, overs), the target or best top right, the
  timing strip on the pitch, and a big result number on a boundary, all in our own design.

## Timing: one press, every direction

A strip on the pitch in front of the batter, along the ball's path, in seven parts:

`[ red | fine leg | midwicket | straight | cover | third man | red ]`, early to late.

- **Which band** the ball is in when you press sets the direction: very early fine leg, early
  midwicket, on time straight, late cover, very late third man (for a right-hander).
- **Within a band**, distance from its bright centre line sets the distance: dead centre **6** over
  the boundary, near it **4**, further **2**, at the band's edge **1**.
- **Red ends:** far too early or late, an edge: caught behind (out) at 50%, else 1.
- **No press, or the ball past the strip:** bowled.
- **Caught in the deep:** a lofted shot that falls short of six (a 4 or 2 off a band's outer half)
  has a chance of going to a fielder, who catches it: out. Rarer near a band's centre.
- One press per ball; a press during the swing does nothing.

**Getting harder.** Each over (6 balls): the ball gets faster and the centre lines narrower. From
over 3, slower balls and higher bounces mix in, so the ball reaches the bands at different moments.

## Modes

- *Endless:* 3 wickets, play on until all are gone; best score kept.
- *Chase:* level 1 = 12 runs off 6 balls, each level more runs per ball; one wicket; best level kept.

**Menus.** Start screen: Endless, Chase, Help, Quit, picked with the wheel and Centre (or a tap).
Return during play opens Pause: Continue, Restart, Quit. Game over: score, best, Play again, Menu.
Menu colours from Q2 Pod's theme pop-up colours, as Buddy does.

**Saving.** Best Endless score and best Chase level in a small file in the game's card folder
(`.sixtrack/save`), written whole (temp file, then rename).

## Look (full colour, our own cartoon style)

- Sky: evening blue to deep navy, floodlights glowing white-gold with soft halos.
- Stands: coloured tiers (teal, orange, magenta, yellow) with a speckled crowd.
- Field: rich green with lighter mowing stripes, white boundary rope.
- Pitch: tan with worn darker patches, white crease lines.
- Players: rounded cartoon figures with proper heads and bodies (not stick figures); our team in
  royal blue with a gold trim, the bowling side in green and white; umpire in white coat and black
  hat; wooden-brown bat; red ball with a short trail.
- Effects: a big result number with a colour burst on a 4 or 6, bails flying when bowled, a quick
  flash when a fielder catches.
- Panels: white text on dark rounded panels, readable over the field.

## Feel and performance

- 60 frames a second during play. The still background is rendered once into a buffer; each frame
  copies it and draws only the animated parts.
- The ball's position comes from time, not frames, so a slow frame never moves it wrongly; a press
  is judged by when its datagram arrived.

## Testing

- Host build (`-DHOST`) renders scripted sessions to image frames on the Mac: the look, the strip,
  each result and direction, catches, menus, game over.
- Checks of the timing table (press time → band, distance, result) and of difficulty per over.
- Then on the Q2 (dev build, q2remote): smoothness, input lag, Return-hold quit, music keeps
  playing.

## Left to the builder

- Exact speeds, band widths and catch chances (tuned by play).
- Figure drawing details and animation frames.
- The font (Buddy's generated font header can be reused).

## Release

- Repo `js-surya/q2-sixtrack`, MIT, credits Buddy (MIT) for the Q2 layer.
- Release zip `q2-sixtrack-vX.Y-<sha>.zip` holding `.sixtrack/q2app` and `.sixtrack/q2-sixtrack`; then
  ask Bond to add it to `sources.list` under `[Games]`.
