# Metronome

The metronome clicks on the beats of your timing. It is **one metronome** for the timing window and the level editor:
the **Metronome** button in the window and the **BPM ON / OFF** button in the editor are the same switch.

## Using it

- In the **timing window**: turn it on in the **Playback** tab and press Space.
- In the **editor**: press **BPM** and play the level's music (the editor's play button). The clicks follow the music.

## What it plays

- The **first beat of every bar** is higher and louder than the other beats.
- Inside a [rhythm pattern](Rhythm-Patterns) it plays the pattern: pink hits are louder, purple hits are quiet.

## Options

Set in the **Playback** tab (right side) or in the [mod settings](Settings):

| Option | What it does |
|---|---|
| **Sound** | `classic` (a short beep), `wood` (a dry knock), `click` (a very short tick). Changing it plays a preview |
| **Ticks** | `1/1` = a click on every beat. `1/2`, `1/3`, `1/4` = also quieter clicks between the beats (only outside rhythm patterns) |
| **Bar beat** | How loud the first beat of a bar is compared to the others, 100% to 300% |
| **Metronome volume** | The slider in the Playback tab (0 to 100) |
| **Metronome offset (ms)** | Settings only. Plays the clicks earlier (positive) or later (negative) to match your audio latency |

## Tips

- If the clicks are slightly late or early on **every** beat, it is your audio latency: use the offset, not the timing points.
- The music volume of the window is the game's own music volume, so the window and the editor sound the same.
