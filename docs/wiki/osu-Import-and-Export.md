# osu! Import and Export

Already have a timed osu! map for your song? You can take its timing straight into Geometry Dash.

![Imported osu! timing](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/osu-import.png)

## Import

1. **Drag a `.osu` file onto the game window**, or use **Files → Import .osu**.
2. Pick the right file: every difficulty is its own `.osu` in the beatmap folder (`osu!/Songs/<beatmap>/`),
   named `Artist - Title (Mapper) [Difficulty].osu`.

![.osu difficulty files](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/osu-file.png)

What happens:

- All **red (uninherited) timing points** are copied: time, BPM, beats per bar, including every BPM change.
  Green slider-velocity points are skipped.
- If the beatmap's audio file is next to the `.osu` and no song is loaded yet, **the song is loaded too**.
- Your [rhythm patterns](Rhythm-Patterns) stay.
- The result is a normal timing map: edit it, use the [metronome](Metronome), make [guidelines](Guidelines) from it.
- You can undo the import with Ctrl+Z.

## Export

**Files → Export .osu** saves your timing as an osu! `[TimingPoints]` section. It is not a full beatmap:
paste it into the `[TimingPoints]` part of a `.osu` file, or use it to check your timing in the osu! editor.
Rhythm patterns are not exported (osu! has no such thing).

## Whole-level backup

**Export level** / **Import level** (also in the Files tab) save and load **everything** of a level: timing, rhythm patterns,
settings and song start. Use it as a backup, or to move your work to another PC.
