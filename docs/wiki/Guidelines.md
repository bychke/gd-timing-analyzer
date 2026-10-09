# Guidelines

Guidelines are the colored vertical lines in the editor that show where the beats of the song are.
This mod draws them from your timing, so they are always exactly on the beat.

## Colors

| Color | Meaning |
|---|---|
| **Green** | The first beat of a bar |
| **Yellow** | A beat |
| **Orange** | A line between beats (1/2, 1/3, 1/4...) |

They start where the song starts (the level's **Start Offset**). The mod replaces the guidelines that are already in the level.

## Automatic guidelines

With **Automatic guidelines** on (the default, see [Settings](Settings)), the lines are redrawn **by themselves** whenever
the timing changes: adding, moving or deleting a timing point, changing a BPM, an analysis, an import, a rhythm pattern.
Deleting all points or using **Unload everything** removes them.

With it off, press **Create guidelines** in the **Guidelines** tab when you want them updated.

## Changing the snap

- The **1/N** button in the editor cycles the snap of the whole level: 1/1, 1/2, 1/3, 1/4, 1/6, 1/8.
  The lines are redrawn at once, and the choice is remembered **for that level**.
- The **Lines: 1/N** button in the **Guidelines** tab does the same.
- **Remove guidelines** deletes all of them.

## Different snap in one part of the song

The 1/N button changes the whole level. To use another snap only for a part, make a [rhythm pattern](Rhythm-Patterns):
inside a pattern the guidelines follow the pattern (and 1/N does nothing there), and outside it they use 1/N again.

## Notes

- Guidelines are part of the level, so they stay in it. They are lines at times, so they follow speed portals by themselves.
