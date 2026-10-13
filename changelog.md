# v1.3.0

## New: Triggers tab
- Puts **pulse**, **spawn** or **color** triggers into the level, exactly on the beats (speed portals included)
- **On beats** (one trigger per beat) or one **spawn loop** with a stop trigger at the end
- Every bar, 1/1 ... 1/8 or on the hits found in the song; rhythm patterns are followed
- Start / End of the part (or the yellow cursor), up to 8 **steps** (colors or groups one after another), presets
- Placed at the editor's height, removed with one Ctrl+Z

## Playback
- New rows: **Main**, **Metronome**, **Song Start / Guidelines**
- **Speed**: play slower (100% / 75% / 50% / 25%), the pitch stays
- Go to the end of the song, go to the Song Start line, **follow** the cursor on / off
- **Song Start** line on the waveform
- **Automatic Guidelines** checkmark moved here from the mod settings
- **WAVE** switch (Editor Waveform) in the window
- Four beat squares in the top right corner blink on the beats
- Metronome sounds: classic and wood

## Timing window
- Tabs in two rows (Playback, Timing Points, Rhythm / Files, Triggers)
- **Undo / Redo** buttons in the top left corner; undo also covers Song Start and 1/N
- Waveform grid (pink 1/N) and zoom next to the title
- A short message above the window after every action
- Rhythm: **Snap to grid** on / off, **Clear bar**, **X** per beat, every snap 1/1 ... 1/16 (odd ones too), better dragging
- New icons, simpler help (i) for every tab
- Guideline 1/N up to 1/12 and 1/16

## Editor
- Pause menu: the mod's settings (gear + note) and a separate **UI** button for UI Settings

## Fixes
- Timing, rhythm and settings of a level are saved reliably (stable level key, safe writes, saved even without the song loaded)

# v1.2.0

## Timing window
- Redesigned look: tabs with icons, compact rows of round buttons, GD-style buttons in Files
- **Grid** moved to the Timing Points tab
- **Editor Settings** renamed to **UI Settings**
- The top line shows the song and where it comes from (Jukebox NONG, Music Library, local file) and whether the timing comes from an .osu file

## Editor
- **Custom guideline colors**: guidelines drawn in the waveform's colors (setting, on by default)
- Second text line shows the position of the screen center; both texts can be moved
- Choose which editor buttons are shown (BPM, WAVE, TIME, 1/N)
- Waveform and guideline colors and thickness are editable in UI Settings

## Other
- Faster loading: the waveform appears right away, the analysis follows
- Editor-only options are no longer duplicated in Geode's settings list

# v1.1.0

## Editor
- **TIME** has a new icon and opens the timing window exactly at the **selected object** (leftmost one when several are selected); without a selection it uses the middle of the screen
- New **1/N** button next to TIME: cycles the guideline snap of the level (1/1, 1/2, 1/3, 1/4, 1/6, 1/8) and redraws the guidelines right away
- **Automatic guidelines** (setting, on by default): guidelines update by themselves after every timing change
- Editor buttons can be moved and resized: from the **pause menu (ESC)** with a live preview, or in the mod settings (position, size, row / 2x2 / column)
- The object beat label moved to the top of the screen
- **Hide buttons while playtesting** (setting)

## Timing window
- **Undo / Redo** for timing points and rhythm patterns (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z, or the buttons in Timing Points)
- New **Rhythm** tab: rhythm patterns inside a constant BPM (1/4, then four fast 1/6 hits, and so on), with accents, repeats and beats stretched over several beats. The metronome and the guidelines follow the pattern
- BPM buttons **x1.5** and **/1.5**
- Music volume now uses the game's music volume, so the window and the editor sound the same
- The tab buttons spread over the whole window width

## Metronome
- Three sounds to choose from: classic, wood, click
- Ticks between the beats (1/2, 1/3, 1/4)
- Louder, higher first beat of every bar (adjustable)

## Other
- Guide for every part of the mod on the project's GitHub wiki

# v1.0.0

- First release
- Waveform with BPM, tempo change and beat start detection
- Editable osu!-style timing points, .osu import / export
- Metronome and song waveform in the level editor
- Per-level timing and settings, level export / import
- Local songs in levels (Jukebox / NONG aware), song start offset, guidelines
