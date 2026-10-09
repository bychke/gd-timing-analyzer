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
