# Metronome

The metronome clicks on the beats of your timing. It is **one metronome** for the timing window and the level editor:
the **BPM ON / OFF** button in the window's **Playback** tab and the **BPM** button in the editor are the same switch.

## Using it

- In the **timing window**: press **BPM** in the **Playback** tab (green = on) and press Space.
- In the **editor**: press **BPM** and play the level's music (the editor's play button). The clicks follow the music.

## What it plays

- The **first beat of every bar** is higher and louder than the other beats.
- Inside a [rhythm pattern](Rhythm-Patterns) it plays the pattern: pink hits are louder, purple hits are quiet.

## Options

In the **Playback** tab ([Timing Window](Timing-Window#playback)), row 2 and the sliders:

| Option | What it does |
|---|---|
| **Sound** `< CLASSIC >` | `classic` (a short beep), `wood` (a dry knock), `click` (a very short tick). Changing it plays a preview |
| **Every** `< 1/1 >` | `1/1` = a click on every beat. `1/2`, `1/3`, `1/4` = also quieter clicks between the beats (only outside rhythm patterns) |
| **Metronome** slider | The volume of the clicks (0 to 100) |
| **Metronome offset (ms)** | In the [mod settings](Settings). Plays the clicks earlier (positive) or later (negative) to match your audio latency |

## Tips

- If the clicks are slightly late or early on **every** beat, it is your audio latency: use the offset, not the timing points.
- The music volume of the window is the game's own music volume, so the window and the editor sound the same.
