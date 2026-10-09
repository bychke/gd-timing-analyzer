# Rhythm Patterns

A **rhythm pattern** describes a rhythm *inside* a constant BPM, without changing the BPM and without adding timing points.

**Example:** the song is 278 BPM and the grid is 1/4, but the melody plays one normal hit, and then four fast hits in
the time of one beat (1/6 feel). A rhythm pattern lets the metronome and the guidelines play exactly that.

Open the **Rhythm** tab in the [timing window](Timing-Window). You need at least one [timing point](Timing-Points) first.

## Making a pattern

1. Put the yellow cursor where the pattern starts and press **+ Rhythm**. The point snaps to the nearest beat.
   It starts as a pattern of one bar with a single hit on every beat, which sounds like the normal metronome.
2. Every beat is a column. The **button above it** (for example `1/6`) sets the **snap of that beat**.
   Click it to cycle 1/1, 1/2, 1/3, 1/4, 1/6, 1/8.
3. The **squares below** are the hits of that beat. Click a square to cycle:
   - **gray**: no hit,
   - **purple**: a quiet hit (a "sub-beat"),
   - **pink**: an accented hit (a louder click).
   After you change a snap, every square plays. The ones that fall exactly on the beat are pink.
4. The pattern **repeats** from its start until the next rhythm point or the next BPM change.

## Buttons

| Button | What it does |
|---|---|
| **< / >** | Select the previous or next rhythm point |
| **+ Rhythm** | A new pattern at the cursor |
| **Normal** | A "normal" point: ends the pattern and goes back to the regular grid |
| **Delete** (key Delete) | Removes the selected point |
| **Length: 1 / 2 / 4 bars** | Length of one repeat. Longer patterns repeat what you already have |
| **Loops: all / 1 / 2 / 3 / 4 / 8** | How many times the pattern repeats. `all` = until the next point or BPM change |
| **From audio** | Tries to build the pattern from the hits it finds in the song. A starting point, not a final answer: check it by ear |
| **Bar < / >** | Shows another bar of a longer pattern |
| **-** over a beat | Removes that beat and makes the **previous** one longer (never across a bar line) |
| **+** over a beat | Splits a stretched beat again |

## Stretching a beat over several beats

Remove a beat with **-**, and the beat before it becomes twice as wide. Its snap then covers both beats.
For example, a stretched beat set to `1/3` gives **three even hits over two beats**.
To play 1/3 over the second half of a bar, press **-** on the 4th beat, and set the 3rd beat (now two beats wide) to `1/3`.

## On the waveform

- Purple lines with a triangle are rhythm points. Click one to select it, drag it to move it (it snaps to beats, **Alt** = free).
- Short purple ticks along the bottom are the hits of the patterns.

## What follows a pattern

- The **metronome** clicks the pattern: pink hits are louder, purple ones are quiet. See [Metronome](Metronome).
- The **guidelines** use the pattern's hits: pink hits are beat-colored, purple ones are orange. See [Guidelines](Guidelines).
- **Inside a pattern the snap buttons do nothing**: the **1/N** button, the **Lines: 1/N** button and the metronome **Ticks** only
  change the parts of the level that are *not* inside a pattern.

Patterns are saved with the timing of the level, are included in **Export level**, can be undone, and stay when you re-run **Analyze!**.
