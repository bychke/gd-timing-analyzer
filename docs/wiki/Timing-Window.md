# Timing Window

The window has the **waveform** on top, a thin **overview strip** of the whole song under it, a line with the time and
BPM, the **cursor position** (bar / beat / snap), and seven **tabs** at the bottom.

## The waveform

- **Yellow line**: the cursor / play head.
- **Red lines with a triangle on top**: timing points (the number is the BPM). The selected one is yellow.
- **Purple lines**: [rhythm patterns](Rhythm-Patterns).
- **Beat grid**: white = bar line, gray = beat, red = 1/2, blue = 1/4, purple = 1/3 (same colors as osu!).
- **Orange line at the bottom**: detected hits. Beat lines should sit on its peaks.
- **Green mark**: where the analysis found the first beat.

### Mouse

| Action | Result |
|---|---|
| Click the waveform | Moves the cursor (snaps to the grid, hold **Alt** for a free position) |
| Drag the waveform | Scrolls |
| Drag a red or purple marker | Moves that point (hold **Shift** to snap a timing point to the nearest hit) |
| Drag the strip under the waveform | Jumps anywhere in the song |
| Wheel | Moves to the next grid line |
| **Shift** + wheel | Scrolls |
| **Ctrl** + wheel | Zooms |

## Tabs

### Playback
- **Play / Pause** (Space): plays from the cursor.
- **Metronome**: on or off, shared with the **BPM** button in the editor. See [Metronome](Metronome).
- **Grid**: snap of the grid and of cursor clicks: 1/1 ... 1/16. This only affects this window.
- **Zoom - / +**, **Go to cursor**, **To start**.
- **Sound / Ticks / Bar beat** and the **Music** and **Metronome** volume sliders. Music uses the game's own music volume.

### Timing Points
Add, delete, move and fine-tune timing points. See [Timing Points](Timing-Points).

### Rhythm
Rhythm patterns inside a constant BPM. See [Rhythm Patterns](Rhythm-Patterns).

### Analysis
- **Analyze!**: listens to the song and finds the BPM, tempo changes and where the beat starts.
  It **replaces the current timing points** (rhythm patterns stay).
- **Min BPM / Max BPM**: the search range. If the result is half or double the real tempo, narrow the range
  (or use **x2 / /2** in Timing Points).
- **Tempo changes ON**: adds a timing point wherever the tempo drifts (live drummers, old recordings).
  **OFF**: one constant BPM for the whole song (most electronic and studio tracks).
- **Tap** (T): tap along with the music. After 4 taps the selected point gets the tapped BPM.

### Files
- **Import .osu / Export .osu**: see [osu! Import and Export](osu-Import-and-Export).
- **Export level / Import level**: saves all of a level's mod data (timing, settings, song start) as one `.json`
  file, e.g. as a backup or to move it to another PC.
- **Unload everything**: removes the song, the waveform and **every timing point** of this level, also the saved ones.

### Level
- **Load audio...**: any mp3 / ogg / wav / flac / m4a. The level starts using it right away (see [Local Songs](Local-Songs)).
- **Load level song**: loads the level's own song (a Jukebox NONG too).
- **Use as level song / Restore song**: see [Local Songs](Local-Songs).
- **Song starts here / Reset song start**: sets the level's **Start Offset** to the cursor.

### Guidelines
See [Guidelines](Guidelines).
