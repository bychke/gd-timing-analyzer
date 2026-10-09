# Geometry Dash Timing Analyzer: Guide

An osu!-style timing analyzer for the Geometry Dash level editor. Load a song, let the mod find its BPM, fine-tune the
timing points, and build your level exactly on the beat.

![Timing window](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/playback.png)

## Where to start

| I want to... | Read |
|---|---|
| Install the mod and open it for the first time | [Getting Started](Getting-Started) |
| Understand every tab of the timing window (Playback, Timing Points, Rhythm, Files) | [Timing Window](Timing-Window) |
| Edit, add, delete and undo timing points | [Timing Points](Timing-Points) |
| Make a rhythm that changes inside one BPM (1/4, then fast 1/6...) | [Rhythm Patterns](Rhythm-Patterns) |
| Use the buttons in the level editor (BPM, WAVE, TIME, 1/N) | [Editor Buttons](Editor-Buttons) |
| Move, resize or hide those buttons, change the guideline colors | [UI Settings](UI-Settings) |
| Use the metronome | [Metronome](Metronome) |
| Put beat lines in the editor | [Guidelines](Guidelines) |
| Use my own mp3 in a level | [Local Songs](Local-Songs) |
| Take the timing from an osu! beatmap | [osu! Import and Export](osu-Import-and-Export) |
| Change the mod's options | [Settings](Settings) |
| Look up a key or mouse shortcut | [Shortcuts](Shortcuts) |
| Fix a problem | [FAQ](FAQ) |

## The idea in one minute

1. Open the timing window (**Custom Song → Timing**, or the **TIME** button in the editor).
2. Open the **Timing Points** tab and press **Analyze!**. The mod finds the BPM, tempo changes and where the beat starts.
3. Check the red lines (timing points) against the waveform and nudge them if needed.
4. Turn on the **BPM** metronome in the editor, or let **Automatic guidelines** draw beat lines for you.
5. Build the level on the beat.

Everything is saved **per level**: every level has its own song, timing and settings.
