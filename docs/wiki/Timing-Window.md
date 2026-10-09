# Timing Window

Open it with **Custom Song → Timing**, or the **TIME** button in the editor ([Getting Started](Getting-Started)).

![Timing window](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/window.png)

The window has, from the top:

1. The title and a **Song** line: the loaded file and where it comes from (`Jukebox NONG`, `Newgrounds / Music Library`,
   `local file`), and on the right the **Level** name and whether the timing comes from an **osu! file**.
2. The **waveform** with the beat grid, timing points and the cursor.
3. A thin **overview strip** of the whole song under it.
4. An info line: **time / song length** and **BPM** at the cursor, and under it the **cursor position** (bar / beat / snap).
5. The **tab bar**: **Playback**, **Timing Points**, **Rhythm**, **Files**, and **UI Settings** on the right
   (only when the window was opened from the level editor).
6. The **tab panel**. The **i** button in its corner explains the current tab.

## The waveform

- **Yellow line**: the cursor / play head.
- **Red lines with a triangle on top**: timing points (the number is the BPM). The selected one is yellow.
- **Purple lines**: [rhythm patterns](Rhythm-Patterns).
- **Beat grid**: white = bar line, gray = beat, red = 1/2, blue = 1/4, purple = 1/3 (the same colors as osu!).
- **Orange line at the bottom**: detected hits. Beat lines should sit on its peaks.
- **Green mark**: where the analysis found the first beat.
- Progress of a running analysis and the result of **Tap** appear in the top right corner of the waveform.

### Mouse

| Action | Result |
|---|---|
| Click the waveform | Moves the cursor (snaps to the **Grid**, hold **Alt** for a free position) |
| Drag the waveform | Scrolls |
| Drag a red or purple marker | Moves that point (hold **Shift** to snap a timing point to the nearest hit) |
| Drag the strip under the waveform | Jumps anywhere in the song |
| Wheel | Moves to the next grid line |
| **Shift** + wheel | Scrolls |
| **Ctrl** + wheel | Zooms |

## Playback

![Playback tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/playback.png)

Everything for listening and for the level's song start and guidelines. Three rows:

**Row 1: moving around**

- **Play / Pause** (Space): plays from the cursor.
- **Crosshair**: scrolls the waveform to the cursor.
- **|◀◀**: jumps to the start of the song.
- **Magnifiers**: zoom out (**-**) and zoom in (**+**).

**Row 2: metronome**

- **BPM ON / OFF**: the metronome. The same switch as the **BPM** button in the level editor. See [Metronome](Metronome).
- **Sound** `< CLASSIC >`: the click sound (classic, wood, click). Use the arrows or click the name.
- **Every** `< 1/1 >`: how often it clicks: every beat (1/1), or also between the beats (1/2, 1/3, 1/4).

**Row 3: song start and guidelines**

- **Song**: where the level's song starts (its **Start Offset**):
  the **cursor** button sets it to the yellow cursor, the **|◀◀** button sets it back to 0:00, and you can type the
  start in seconds into the field.
  This needs a level (it says *Open this from a level* otherwise).
- **Guidelines**: the green **lines** button creates the beat lines in the editor, the red **crossed lines** button removes them,
  and the pink **1/N** button is the snap of the guidelines (the same as the **1/N** button in the editor).
  See [Guidelines](Guidelines).

**Sliders** (right): **Music** (the game's own music volume, so the window and the editor sound the same) and
**Metronome** volume.

## Timing Points

![Timing Points tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/timing-points.png)

**Left side**: add, delete, move and fine-tune the timing points. **Right side**: finding the BPM automatically or by tapping.
Every control is explained in [Timing Points](Timing-Points).

- **Point 1/3** with arrows: the selected point. **+ Add** and **Delete**.
- **Grid** `< 1/4 >`: the snap of the cursor and of the mouse wheel (1/1 ... 1/16). This only changes this window.
- **Analyze!**, **Tap (T)**, **Min / Max** BPM and **Tempo changes**.
- **Undo / Redo**.

## Rhythm

![Rhythm tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/rhythm.png)

Rhythm patterns inside a constant BPM: a different rhythm without more timing points. Everything is explained in
[Rhythm Patterns](Rhythm-Patterns).

## Files

![Files tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/files.png)

| Row | Buttons |
|---|---|
| **Song file** | **Load audio file...** loads any mp3 / ogg / wav / flac / m4a. **Load level's song** loads the level's own song (a Jukebox NONG too) |
| **In the level** | **Use as level song** makes the level play the loaded file. **Restore original song** puts the original back. See [Local Songs](Local-Songs) |
| **osu! timing** | **Import from .osu** and **Export to .osu**. See [osu! Import and Export](osu-Import-and-Export) |
| **Level backup** | **Import backup** / **Export backup**: all of a level's mod data (timing, rhythm patterns, settings, song start) as one `.json` file |
| (bottom right) | **Unload everything** removes the song, the waveform and **every timing point** of this level, also the saved ones |

## UI Settings

Opens the live preview of the editor's buttons, texts and guideline colors. See [UI Settings](UI-Settings).
