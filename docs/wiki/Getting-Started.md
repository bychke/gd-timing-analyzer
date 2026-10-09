# Getting Started

## Requirements

- Geometry Dash **2.2081** on **Windows**
- [Geode](https://geode-sdk.org) **5.10.1** or newer

## Installation

1. Download `bychke.gd-timing-analyzer.geode` from the [Releases](https://github.com/bychke/gd-timing-analyzer/releases) page.
2. Put it in the `geode/mods` folder of your Geometry Dash.
3. Restart the game.

## Opening the timing window

- In a level's song selection (**Custom Song**), press the **Timing** button.
- In the level editor, press the **TIME** button (the blue button with the note on a waveform). It opens the window
  at the moment you are looking at. See [Editor Buttons](Editor-Buttons).
- **Drag an audio file or a `.osu` file onto the game window.** Inside the editor this opens the window and loads the file.

## First time with a song

1. Open the level in the editor and press **TIME**. The mod loads the level's own song.
   (To use another file, see [Local Songs](Local-Songs).)
2. Go to the **Analysis** tab and press **Analyze!**.
3. Press **Space** to listen. Turn on the **Metronome** in the **Playback** tab, and check that the clicks match the music.
4. If the clicks drift or are on the wrong beat, fix the timing points: see [Timing Points](Timing-Points).
5. Close the window. The editor continues exactly where the waveform was.

Where do you get the beat lines? [Guidelines](Guidelines) are drawn from your timing automatically.

## Where is my data?

Timing, settings and rhythm patterns are saved **per level**, one `.json` file per level, in the mod's save folder
(`%LOCALAPPDATA%\GeometryDash\geode\mods\bychke.gd-timing-analyzer\levels`). They are not part of the level itself,
so an uploaded level does not contain them. The only thing stored in the level is what the game stores:
the guidelines and the song's start offset.
