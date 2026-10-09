# Geometry Dash Timing Analyzer

An osu!-style timing analyzer inside the Geometry Dash level editor.

Open it with **Custom Song -> Timing**, the **TIME** button in the editor, or drag an audio / `.osu` file onto the game window. A full guide of every part is on the project's GitHub wiki.

## Timing window
- **Playback**: waveform with zoom and scrolling, metronome (3 sounds, ticks, accented first beat), beat grid 1/1 ... 1/16, volume sliders
- **Timing Points**: add, delete and drag points, edit time / BPM / beats per bar, x2 /2 x1.5 /1.5, nudge by ms, "downbeat here", tap tempo, **undo / redo** (Ctrl+Z / Ctrl+Y)
- **Rhythm**: a rhythm pattern inside a constant BPM (1/4, then four fast 1/6 hits...). The metronome and the guidelines follow it
- **Analysis**: automatic BPM, tempo changes and beat start detection
- **Files**: osu! import / export, a level's mod data as one file, unload everything
- **Level**: play any audio file in a level (Jukebox NONGs too), song start offset from the cursor
- **Guidelines**: made from your timing, automatic by default

## osu! beatmaps
- **Import .osu** (or drag a `.osu` file onto the game) copies every red timing point: time, BPM and beats per bar, including all BPM changes
- If the map's audio file is next to the `.osu`, the song is loaded too
- **Export .osu** saves your timing as an osu! `[TimingPoints]` section

## In the editor
- **BPM**: metronome, **WAVE**: the song's waveform behind the level, **TIME**: opens the timing window at the selected object, **1/N**: guideline snap of the level
- The buttons can be moved and resized: pause menu (ESC) -> the note button, or the mod settings
- The label at the top shows the bar / beat of the selected object (can be turned off in the mod settings)
