# Editor Buttons

In the level editor the mod adds four round buttons next to the music button.

| Button | What it does |
|---|---|
| **BPM ON / OFF** | The metronome. Clicks on the beats while the editor music plays. Shared with the window's **Metronome** button |
| **WAVE ON / OFF** | Draws the song's waveform (semi-transparent, light blue) behind the level. It follows speed portals and the song's start offset, so you can see the music against your objects |
| **TIME** (note on a waveform) | Opens the [timing window](Timing-Window) at the moment you are looking at, and continues from there when you close it |
| **1/N** | Cycles the snap of the [guidelines](Guidelines) of the whole level: 1/1, 1/2, 1/3, 1/4, 1/6, 1/8. Redraws them at once |

A label at the top of the screen shows where you are on the beat grid:

- **Music: Bar 31  Beat 3**: while the music plays.
- **Object: Bar 30  Beat 4 - on 1/1 (beat)  (-0.5 ms)**: for the selected object. Green means it sits on the grid, red means it is off grid.

You can turn the label off in the [Settings](Settings) (`Show beat of selected object`).

## TIME and the selected object

- If **an object is selected** (or several: the leftmost one counts), **TIME** opens the window exactly at that object.
  Nothing has to play for this, so you can pick a spot and look at it in the waveform.
- With **nothing selected**, it opens at the middle of the screen.
- While the **music is playing**, it opens at the playing position and stops the editor music; the window plays on from there.
- When you close the window, the editor jumps to the cursor of the waveform, and plays on if the window was playing.

## Moving and resizing the buttons

The buttons can cover other buttons (especially with BetterEdit), so you can place them where there is room.

**In the editor, with a live preview:**

1. Open the **pause menu (ESC)** and press the button with the note on a waveform (next to Help).
2. The menu closes and a small window slides up from the bottom of the screen.
3. **Drag anywhere outside that window** to move the buttons.
4. Use **Layout** (row, 2x2, column), **- / +** for the size, and **Reset** to go back to the defaults.
5. Press **Done**.

**In the settings:** `Editor buttons X`, `Y`, `size` and `layout`. See [Settings](Settings).

The buttons can also **disappear during a playtest** (`Hide buttons while playtesting`, on by default).
