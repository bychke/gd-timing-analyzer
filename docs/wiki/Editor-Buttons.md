# Editor Buttons

In the level editor the mod adds four round buttons next to the music button.

| Button | What it does |
|---|---|
| **BPM ON / OFF** | The metronome. Clicks on the beats while the editor music plays. Shared with the window's **BPM** button |
| **WAVE ON / OFF** | Draws the song's waveform (semi-transparent) behind the level. It follows speed portals and the song's start offset, so you can see the music against your objects |
| **TIME** (note on a waveform) | Opens the [timing window](Timing-Window) at the moment you are looking at, and continues from there when you close it |
| **1/N** | Cycles the snap of the [guidelines](Guidelines) of the whole level: 1/1, 1/2, 1/3, 1/4, 1/6, 1/8. Redraws them at once |

Two lines at the top of the screen show where you are on the beat grid:

- **Music: Bar 31  Beat 3**: while the music plays.
- **Object: Bar 30  Beat 4 - on 1/1 (beat)  (-0.5 ms)**: for the selected object. Green means it sits on the grid, red means it is off grid.
- **Screen center: ...**: where the middle of the screen is. That is the line that activates triggers while you scroll.

Turn them off or move them in [UI Settings → Text](UI-Settings#text).

## TIME and the selected object

- If **an object is selected** (or several: the leftmost one counts), **TIME** opens the window exactly at that object.
  Nothing has to play for this, so you can pick a spot and look at it in the waveform.
- With **nothing selected**, it opens at the middle of the screen.
- While the **music is playing**, it opens at the playing position and stops the editor music; the window plays on from there.
- When you close the window, the editor jumps to the cursor of the waveform, and plays on if the window was playing.

## Moving, resizing and hiding the buttons

The buttons can cover other buttons (especially with BetterEdit), so you can place them where there is room.

1. In the editor press **TIME**, then **UI Settings**.
2. **Drag anywhere outside the settings window** to move the buttons.
3. Choose the **Layout**, the **size** and which buttons to **show**, and press **Done**.

See [UI Settings](UI-Settings). The buttons can also disappear during a playtest (**Hide in playtest**, on by default).
