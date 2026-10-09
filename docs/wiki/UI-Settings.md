# UI Settings

A small see-through window over the level editor with a **live preview**: what you change here changes in the editor at once.

## Opening it

In the level editor press **TIME**, then **UI Settings** (right end of the tab bar in the [timing window](Timing-Window)).
The timing window closes and the settings slide up from the bottom of the screen.
Press **Done** to close it.

There are three tabs. **Reset** (top right) resets only the tab you are on.

## Buttons

![UI Settings: Buttons](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/ui-settings-buttons.png)

For the four [editor buttons](Editor-Buttons) (BPM, WAVE, TIME, 1/N).

- **Drag anywhere outside this window** to move the buttons. A pulsing frame shows what you are moving.
- **Show**: which buttons are visible (`BPM`, `Wave`, `Timing window`, `1/N lines`). The others move together, with no gap.
- **Layout**: `row`, `2x2` or `column`.
- **- / +**: the size of the buttons (50% to 150%).
- **Hide in playtest**: the buttons disappear during a playtest.

## Text

![UI Settings: Text](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/ui-settings-text.png)

For the two lines at the top of the editor (the beat of the selected object / the music, and the position of the screen center).

- **Drag anywhere outside this window** to move the texts.
- **Show beat text**: turns both lines on or off.
- **Colored center text**: the screen center line is cyan when it is on the grid and pale blue when it is off the grid; off = plain white.

## Guideline Colors

![UI Settings: Guideline Colors](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/ui-settings-colors.png)

- **Guideline thickness**: `- 0.2 +` (0.1 to 3.0).
- **Swatches**: tap one to pick a color. **wave** is the color of the song's waveform behind the level (**WAVE** button).
  The others are the guidelines by snap: **1/1** (bar and beat), **1/2**, **1/3**, **1/4**, **1/6**, **1/8**, **1/12** and **other**.
- These colors are used when **Custom guideline colors** is on. See [Guidelines](Guidelines).
