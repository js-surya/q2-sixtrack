# Rooftop Sixes: design

An original one-button street-cricket batting game for the Shanling Q2, as a Q2 Pod card game.
Working name *Rooftop Sixes*; the owner may rename it before release.

## Intent

A quick, colourful pick-up-and-play game: you bat in a narrow street at dusk, time one button to
the ball, and chase sixes over the rooftops. Inspired by the genre of one-button timing cricket
games; it copies no particular game's name, art, characters, levels or look.

## Goals

- **One input.** Centre, or a tap anywhere on the screen, swings the bat. Nothing else in play.
- **Timing is everything**, made visible by coloured zones on the road in front of the batter.
- **Full colour**, not the theme's monochrome: a warm, busy street scene.
- **Two modes:** Endless (best score, three wickets) and Chase (a target in a set number of balls,
  harder level by level).
- **Music keeps playing** while you bat; Play/Pause and the side buttons stay the music's.
- Installs from the App Store's Games tab like the other card games.

## Non-goals (v1)

- Sound effects (they would mean taking the Q2's audio from the music).
- Bowling, fielding, multiplayer, online scores.
- Wheel or other buttons during play (the wheel works only in menus).

## Anti-goals

- Looking or playing like a specific existing game. Our own name, art and rules.
- Timing that feels unfair: a dropped frame or input lag that turns a perfect press into a miss.
- A busy scene that hides the ball or the zones.

## Constraints (Q2 Pod's card games)

- `q2app` with `name=` and `game=1`; the program `.cricket/q2-cricket` on the card. Q2 Pod starts it
  from Library → Games and lists it under the App Store's Games tab.
- It draws on `/dev/fb0` itself: 320 × 375, 32 bpp, two pages; the 375 × 320 landscape view is stored
  turned clockwise.
- Input arrives as text datagrams on `/tmp/q2-cricket.input.sock`: `k d CODE` / `k u CODE` (Centre 218,
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
  the licensed ones are Unity 3D or text games.

## The game

**Screen (landscape 375 × 320).** Side view. Batter on the left at the crease, bowler running in from
the right. A row of houses behind, their rooftop line the six line. Score strip on top: runs,
wickets, over and ball (Endless) or "need N off M" (Chase).

**Timing zones.** A strip on the road in front of the batter: red | yellow | GREEN | yellow | red,
slightly see-through. The ball's position when you press decides the shot:

| Press when the ball is in | Result |
|---|---|
| green (centre) | perfect: straight over the rooftop, **6** |
| green, early or late half | 4, pulled to leg (early) or cut to off (late) |
| yellow | 1 or 2 (1 near the red edge) |
| red | an edge: caught (out) at 50%, else 1 |
| no press, or before the zones | the ball hits the stumps: **out** |

Shot direction follows timing: early goes to leg, late to off, perfect straight. One press per ball;
a press while the bat is swinging does nothing.

**Getting harder.** Each over (6 balls): the ball gets faster and the green zone narrower. From over
3, slower balls and higher bounces mix in, so the ball reaches the zones at different times.

**Out.** Bowled (no swing, a swing before the ball reaches the zones, or after it passes them),
caught off an edge, or a broken window: a 4 (pulled or cut) has a small chance of hitting a window
instead, which is out.

**Modes.**
- *Endless:* 3 wickets, play on until all are gone; best score kept.
- *Chase:* level 1 = 12 runs off 6 balls, each level more runs per ball; one wicket; best level kept.

**Menus.** Start screen: Endless, Chase, Help, Quit, picked with the wheel and Centre (or a tap).
Return during play opens Pause: Continue, Restart, Quit. Game over: score, best, Play again, Menu.
Menu colours from Q2 Pod's theme pop-up colours, as Buddy does.

**Saving.** Best Endless score and best Chase level in a small file in the game's card folder
(`.cricket/save`), written whole (temp file, then rename).

## Look (full colour)

- Sky: dusk gradient, orange at the horizon to deep purple, soft clouds, first stars.
- Street: dusty tan road, chalk-white crease and stumps, brick-red gutter.
- Houses: painted walls (turquoise, mustard, pink, mint), blue doors, lit yellow windows, rooftop
  water tanks, an antenna, laundry lines.
- Players: simple bold figures; batter and bowler in different bright jerseys, wooden-brown bat;
  red ball with a short trail.
- Effects: colour burst and a big "SIX!" on a perfect shot, glass shards on a broken window, dust
  where the ball lands.
- Menus and score: white text on dark panels, readable over the scene.

## Feel

- 60 frames a second during play; the ball's position is computed from time, not frames, so a slow
  frame never moves the ball wrongly.
- Input is read as soon as it arrives each frame; the press time is the datagram's arrival, judged
  against where the ball is at that moment.

## Testing

- Host build (`-DHOST`) renders scripted sessions to image frames on the Mac: the look, the zones,
  each result, menus, game over.
- Unit-style checks of the timing table (press time → result) and of difficulty per over.
- Then on the Q2 (dev build, q2remote): smoothness, input lag, Return-hold quit, music keeps
  playing.

## Left to the builder

- Exact speeds, zone widths and catch/window chances (tuned by play).
- Figure drawing details and animation frames.
- The font (Buddy's generated font header can be reused).

## Release

- Repo `js-surya/q2-cricket` (name may follow the game's), MIT, credits Buddy (MIT) for the Q2 layer.
- Release zip `q2-cricket-vX.Y-<sha>.zip` holding `.cricket/q2app` and `.cricket/q2-cricket`; then
  ask Bond to add it to `sources.list` under `[Games]`.
