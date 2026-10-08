# Geometry Dash Timing Analyzer

An osu!-style timing analyzer inside the Geometry Dash level editor.

Open it with **Custom Song -> Timing**, the **TIME** button next to the editor's music button, or drag an audio / `.osu` file onto the game window.

## osu! beatmaps
- **Import .osu** (or drag a `.osu` file onto the game) copies every red timing point of the map: time, BPM and beats per bar, including all BPM changes
- Green (slider velocity) points are skipped
- If the map's audio file is next to the `.osu`, the song is loaded too
- **Export .osu** saves your timing as an osu! `[TimingPoints]` section

## Playback
- Waveform with zoom and scrolling, plus an overview strip of the whole song
- Play / pause (Space), metronome, beat grid 1/1 ... 1/16 (osu! colors), music and metronome volume sliders

## Timing Points
- Add, delete and drag timing points (Shift = snap to the nearest hit)
- Edit time / BPM / beats per bar, nudge by 1 or 10 ms (single point or all), x2 / /2, "downbeat here", tap tempo

## Analysis
- Automatic BPM, tempo changes and beat start detection, with BPM range and "tempo changes" on/off

## Files
- osu! import / export, export / import all of a level's mod data as one file, unload everything

## Level
- Load any audio file or the level's song (Jukebox NONGs too); a loaded file becomes the level's song right away
- Song start offset straight from the waveform cursor

## Guidelines
- Create / remove guidelines from your timing (green = bar, yellow = beat, orange = 1/2)

## In the editor
- **BPM**: metronome on / off, **WAVE**: the song's waveform behind the level, **TIME**: opens the timing window at the level's moment
- The label next to them shows the bar / beat of the selected object (can be turned off in the mod settings)
