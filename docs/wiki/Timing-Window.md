# Timing Window

Open it with **Custom Song → Timing**, or with the **TIME** button in the level editor ([Getting Started](Getting-Started)).

![Timing window](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/window.png)

## The parts of the window

From the top:

| Part | What it is |
|---|---|
| **Undo / Redo** (top left) | Two round green arrows. Undo / redo for the timing, rhythm patterns, Song Start and 1/N (**Ctrl+Z** / **Ctrl+Y**) |
| **Title** | *Timing Analyzer*. On its right: the pink **1/N** (waveform grid), the **magnifiers** (zoom out / in) and the blue **i** (help of the open tab, row by row) |
| **Song line** | The loaded file and where it comes from (`Jukebox NONG`, `Newgrounds / Music Library`, `local file`). On the right: the **Level** name and whether the timing comes from an **osu! file** |
| **Waveform** | The song, the beat grid, the timing points, the rhythm points and the yellow cursor |
| **Overview strip** | The whole song in one thin line under the waveform. Drag it to jump anywhere |
| **Info** (left of the tabs) | Time / song length and the BPM at the cursor, and under it where the cursor is (bar / beat / snap) |
| **Beat squares** (top right) | Four squares that blink on the beats while the song plays. The first beat of the bar is pink, the others purple |
| **Tabs** | Two rows: **Playback**, **Timing Points**, **Rhythm** and under them **Files**, **Triggers** |
| **Tab panel** | The buttons of the open tab |

Every button shows a short message above the window when you press it (for example *Metronome (BPM) turned on*).

![Help of a tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/help.png)

## The waveform

| What you see | Meaning |
|---|---|
| **Yellow line** | The cursor / play head |
| **Red lines with a triangle** | [Timing points](Timing-Points). The number is the BPM. The selected one is yellow |
| **Purple lines with a triangle** | [Rhythm points](Rhythm-Patterns) |
| **Turquoise line "Song Start"** | Where the level's song starts (its Start Offset). Only shown, it can't be dragged |
| **Beat grid** | White = bar, gray = beat, red = 1/2, purple = 1/3, blue = 1/4 (osu! colors) |
| **Orange line at the bottom** | The hits found in the song. The beat lines should sit on its peaks |
| **Green area** | On the **Triggers** tab: the part of the song where triggers will be placed |

### Mouse

| Action | Result |
|---|---|
| Click the waveform | Moves the cursor (snaps to the waveform grid, hold **Alt** for a free position) |
| Drag the waveform | Scrolls |
| Drag a red or purple marker | Moves that point (hold **Shift** to snap a timing point onto the nearest hit) |
| Drag the strip under the waveform | Jumps anywhere in the song |
| Wheel | Next / previous grid line |
| **Shift** + wheel | Scrolls |
| **Ctrl** + wheel | Zooms |

### The waveform grid (pink 1/N at the top)

The pink **1/N** button right of the title sets the **grid of the waveform**: where the cursor snaps, where the wheel
moves, and where rhythm points snap. It only changes this window, not the level.
Every click: 1/1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16.

---

## Playback

![Playback tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/playback.png)

Listening, the metronome, where the song starts, and the guidelines. Three rows, the volume sliders on the right.

### Row 1: Main

| Button | What it does |
|---|---|
| **Back arrows** (green) | Goes to the start of the song |
| **Play** (**Space**) | Plays / pauses from the yellow cursor |
| **Forward arrows** (green) | Goes to the end of the song |
| **Green arrow** | Scrolls the waveform to the yellow cursor |
| **Blue arrow** | Scrolls the waveform to the turquoise **Song Start** line |
| **Follow** (arrow + line) | Green: the waveform moves with the music while it plays. Gray: the waveform stays where it is, so you can look around while the song plays |
| **Speed** + **clock** | Plays the song slower. Every click: **100% → 75% → 50% → 25%**. The pitch stays the same, and the metronome follows the slower song |

### Row 2: Metronome

| Button | What it does |
|---|---|
| **BPM ON / OFF** | The metronome. The same switch as the **BPM** button in the editor. See [Metronome](Metronome) |
| **SFX** (black note) | The sound of the metronome: **classic** or **wood**. Every click switches and plays a preview |
| **1/N** (pink) | How often it clicks: **1/1** = every beat, **1/2**, **1/3**, **1/4** = also between the beats |
| **Editor Waveform: WAVE ON / OFF** | The song's waveform behind the level in the editor. The same switch as the **WAVE** button in the editor |

### Row 3: Song Start and Guidelines

**Song Start** is where the level's song starts (its **Start Offset**). It is the turquoise line on the waveform.

| Button | What it does |
|---|---|
| **Cursor** (green) | The song starts at the yellow cursor |
| **Back arrows** (gray) | The song starts at 0:00 again |
| **Field** | Type the start in seconds |

These need a level: when the window was opened without one, it says *Open this from a level*.

**Guidelines** are the beat lines in the editor. See [Guidelines](Guidelines).

| Button | What it does |
|---|---|
| **Lines** (green) | Draws the guidelines, starting where the song starts. Old guidelines are replaced |
| **Crossed lines** (red) | Removes all guidelines |
| **1/N** (pink) | A line every 1/N of a beat: 1/1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16. The same as the **1/N** button in the editor |
| **Automatic Guidelines** (checkmark) | The guidelines are redrawn by themselves after every timing change |

### Sliders (right)

| Slider | What it does |
|---|---|
| **Music** | The game's own music volume, so the window and the editor sound the same |
| **Metronome** | The volume of the clicks |

---

## Timing Points

![Timing Points tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/timing-points.png)

**Left:** the selected timing point. **Right:** finding the BPM. Everything is explained in [Timing Points](Timing-Points).

| Row | Left side | Right side |
|---|---|---|
| **1** | **Point 1/3** + arrows (choose a point), **+ Add**, **Delete** | **Analyze!**, **Tap (T)** |
| **2** | **Time** (ms), **-10 / -1 / +1 / +10**, **All points** | **Min** / **Max** BPM |
| **3** | **BPM**, **x2 / /2 / x1.5 / /1.5**, **Meter** | **Tempo changes** |
| **4** | **Snap to hit**, **Downbeat here**, **Clear all points** | |

---

## Rhythm

![Rhythm tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/rhythm.png)

Rhythm patterns: a different rhythm inside one BPM, without more timing points. Everything is explained in
[Rhythm Patterns](Rhythm-Patterns).

| Row | Buttons |
|---|---|
| **1** | **Rhythm 1/2** + arrows, **+ New**, **Normal grid**, **Trash**, **From ♪**, **1 BAR**, **LOOP ALL** |
| **2** | **Snap to grid**, **Clear bar**, **Bar 1/2** + arrows |
| **Beats** | One column per beat: its snap (**1/4**), **-**, **+**, **X**, and the hit squares |

---

## Files

![Files tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/files.png)

| Row | Buttons |
|---|---|
| **Song file** | **Load audio file...** loads an mp3 / ogg / wav / flac. **Load level's song** loads the level's own song (a Jukebox NONG too) |
| **In the level** | **Use as level song** makes the level play the loaded file. **Restore original song** puts the original back. See [Local Songs](Local-Songs) |
| **osu! timing** | **Import from .osu** and **Export to .osu**. See [osu! Import and Export](osu-Import-and-Export) |
| **Level backup** | **Import backup** / **Export backup**: all of a level's mod data (timing, rhythm patterns, settings, Song Start) as one `.json` file |
| (bottom right) | **Unload everything** removes the song, the waveform and **every timing point** of this level, also the saved ones |

You can also **drag an audio file or a `.osu` file onto the game window**.

---

## Triggers

![Triggers tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/triggers.png)

Puts pulse, spawn or color triggers into the level, exactly on the beats. Everything is explained in [Triggers](Triggers).

| Row | Buttons |
|---|---|
| **1** | **Make** (pulse / spawn / color), **As** (ON BEATS / SPAWN LOOP), **Every** (BAR, 1/1 ... 1/8, HITS) |
| **2** | **Start** and **End** (seconds), each with a **cursor** button |
| **3** | **Steps**: how many (1 - 8) and the step squares |
| **4** | The selected step: **Color** + **On** (pulse / color) or **Groups** (spawn) |
| (right) | **Preset**, how many triggers will be placed, **Place triggers** |

---

## UI Settings

The editor's buttons, texts and guideline colors are set in **UI Settings**: in the level editor press **ESC**, then
the **UI** button (a note with *UI* under it). See [UI Settings](UI-Settings).
