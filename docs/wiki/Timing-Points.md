# Timing Points

A **timing point** works like an uninherited (red) point in osu!: from its time on, it sets the **BPM** and the
**beats per bar** (meter) until the next point. Most songs need only one. A song with tempo changes needs one
per change.

Open the **Timing Points** tab in the [timing window](Timing-Window).

## Selecting

- Click a red marker on the waveform, or use **<** / **>** (keys **Up** / **Down**).
- The label shows `Point 2/5`, and the fields show its time, BPM and meter.

## Adding and deleting

- **+ Add** (key **A**): a new point at the cursor, with the BPM that is active there.
- **Delete** (key **Delete**): removes the selected point.
- **Clear all points**: removes every point (asks first).

## Changing a point

| Control | What it does |
|---|---|
| **Time (ms)** field, or dragging the marker | Where the point starts. Hold **Shift** while dragging to snap to the nearest detected hit |
| **BPM** field | Exact BPM |
| **x2, /2, x1.5, /1.5** | Multiplies the BPM (fixes half-time and double-time results, and 3/2 feels) |
| **Meter** field | Beats per bar (1 to 16) |
| **-10, -1, +1, +10** | Nudges the point by that many milliseconds |
| **All points** | Makes the nudge buttons move **every** point (a global offset) |
| **Snap to hit** | Moves the point onto the closest detected hit |
| **Downbeat here** | Makes the beat at the cursor beat 1 of the bar |
| **Tap** (key **T**) | Tap along with the music. After 4 taps the selected point gets that BPM (or a new point is made) |

## Undo and Redo

- **Ctrl+Z** undoes, **Ctrl+Y** or **Ctrl+Shift+Z** redoes. The **Undo** / **Redo** buttons do the same.
- It works for timing points and [rhythm patterns](Rhythm-Patterns): adding, deleting, dragging, nudging, BPM changes,
  analysis results and imports.
- Several quick edits in a row (clicking **+1** a few times, typing a value) count as one step.
- The history holds 200 steps. It starts again when you change the song or the level, and is not kept after you close the window.

## Tips

- Zoom in (**Ctrl** + wheel) and put a point's red line on the start of a clear hit, then check the rest with the metronome.
- A long song with a steady tempo needs **one** point. Do not add points to "fix" small drifts: lower the
  **Tempo changes** setting in the Analysis tab instead.
- Inside one constant BPM, use [Rhythm Patterns](Rhythm-Patterns) to describe a changing rhythm. There is no need to add more points.
