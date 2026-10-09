# Guidelines

Guidelines are the vertical lines in the editor that show where the beats of the song are.
This mod draws them from your timing, so they are always exactly on the beat.

## Colors

With **Custom guideline colors** on (the default, see [Settings](Settings)) the mod draws the lines itself, in the same
colors as the timing waveform, like osu!:

| Color | Meaning |
|---|---|
| **White** | A bar line / a beat |
| **Red** | 1/2 |
| **Purple** | 1/3 |
| **Blue** | 1/4 |
| other | 1/6, 1/8, 1/12 and the rest |

All the colors and the thickness can be changed in [UI Settings → Guideline Colors](UI-Settings#guideline-colors).

With the setting off, the mod puts Geometry Dash's own guidelines in the level instead: **green** for the first beat of a bar,
**yellow** for a beat and **orange** for the lines between beats.

They start where the song starts (the level's **Start Offset**).

## Automatic guidelines

With **Automatic guidelines** on (the default), the lines are redrawn **by themselves** whenever
the timing changes: adding, moving or deleting a timing point, changing a BPM, an analysis, an import, a rhythm pattern.
They are also drawn when you open the editor. Deleting all points or using **Unload everything** removes them.

With it off, press the green **lines** button in the **Guidelines** group of the **Playback** tab when you want them updated.

## Changing the snap

- The **1/N** button in the editor cycles the snap of the whole level: 1/1, 1/2, 1/3, 1/4, 1/6, 1/8.
  The lines are redrawn at once, and the choice is remembered **for that level**.
- The pink **1/N** button in the **Guidelines** group of the **Playback** tab does the same.
- The red **crossed lines** button removes all guidelines.

## Different snap in one part of the song

The 1/N button changes the whole level. To use another snap only for a part, make a [rhythm pattern](Rhythm-Patterns):
inside a pattern the guidelines follow the pattern (and 1/N does nothing there), and outside it they use 1/N again.

## Notes

- Custom-colored guidelines are drawn by the mod and **are not saved in the level**; the player does not see them without the mod.
  With the setting off, they are normal guidelines and stay in the level.
- They are lines at times, so they follow speed portals by themselves.
