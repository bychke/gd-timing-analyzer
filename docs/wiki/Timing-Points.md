# Timing Points

A **timing point** works like an uninherited (red) point in osu!: from its time on, it sets the **BPM** and the
**beats per bar** (meter) until the next point. Most songs need only one. A song with tempo changes needs one
per change.

Open the **Timing Points** tab in the [timing window](Timing-Window).

![Timing Points tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/timing-points.png)

## Selecting

- Click a red marker on the waveform, or use the **arrows** next to the label (keys **Up** / **Down**).
- The label shows `Point 2/5`, and the fields show its time, BPM and meter.

## Adding and deleting

- **+ Add** (key **A**): a new point at the cursor, with the BPM that is active there.
- **Delete** (key **Delete**): removes the selected point.
- **Clear all points**: removes every point (asks first).

## Changing a point

| Control | What it does |
|---|---|
| **Time** field, or dragging the marker | Where the point starts, in ms. Hold **Shift** while dragging to snap to the nearest detected hit |
| **BPM** field | Exact BPM |
| **x2, /2, x1.5, /1.5** | Multiplies the BPM (fixes half-time and double-time results, and 3/2 feels) |
| **Meter** field | Beats per bar (1 to 16) |
| **-10, -1, +1, +10** | Nudges the point by that many milliseconds |
| **All points** | Makes the nudge buttons move **every** point (a global offset) |
| **Snap to hit** | Moves the point onto the closest detected hit |
| **Downbeat here** | Makes the beat at the cursor beat 1 of the bar |
| **Grid** `< 1/4 >` | The snap of the cursor and of the mouse wheel. It does not change the timing |

## Finding the BPM (right side)

- **Analyze!**: listens to the song and finds the BPM, tempo changes and where the beat starts.
  It **replaces the current timing points** (rhythm patterns stay), so fine-tune them afterwards.
- **Min / Max**: the BPM search range. If the result is half or double the real tempo, narrow the range
  (or use **x2 / /2**).
- **Tempo changes** on: adds a timing point wherever the tempo drifts (live drummers, old recordings).
  Off: one constant BPM for the whole song (most electronic and studio tracks).
- **Tap (T)**: tap along with the music. After 4 taps the selected point gets that BPM (or a new point is made).

## Undo and Redo

- **Ctrl+Z** undoes, **Ctrl+Y** or **Ctrl+Shift+Z** redoes. The **Undo** / **Redo** buttons do the same.
- It works for timing points and [rhythm patterns](Rhythm-Patterns): adding, deleting, dragging, nudging, BPM changes,
  analysis results and imports.
- Several quick edits in a row (clicking **+1** a few times, typing a value) count as one step.
- The history holds 200 steps. It starts again when you change the song or the level, and is not kept after you close the window.

## Tips

- Zoom in (**Ctrl** + wheel) and put a point's red line on the start of a clear hit, then check the rest with the metronome.
- A long song with a steady tempo needs **one** point. Do not add points to "fix" small drifts: turn **Tempo changes** off instead.
- Inside one constant BPM, use [Rhythm Patterns](Rhythm-Patterns) to describe a changing rhythm. There is no need to add more points.
