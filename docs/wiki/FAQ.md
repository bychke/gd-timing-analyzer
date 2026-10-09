# FAQ

### The analysis found half or double the real BPM.
Narrow **Min BPM / Max BPM** in the **Analysis** tab and run **Analyze!** again, or press **/2** or **x2** in **Timing Points**.
See [Timing Window](Timing-Window).

### The beat lines are slightly off from the music.
Zoom in, move the first timing point so its red line is on the start of a clear hit (**Shift** while dragging snaps to hits),
and check with the metronome. If every click is a bit early or late, use **Metronome offset** in the [settings](Settings):
it is your audio latency, not the timing.

### The song has a tempo change and the analysis missed it.
Add a [timing point](Timing-Points) at the change (put the cursor there and press **A**) and set the new BPM.

### The rhythm of the song changes (1/4, then fast 1/6), but the BPM stays the same.
Use a [rhythm pattern](Rhythm-Patterns). You do not need more timing points.

### The 1/N button does nothing in some part of the level.
That part is inside a [rhythm pattern](Rhythm-Patterns). Guidelines and metronome follow the pattern there.
Place a **Normal** point where the pattern should stop.

### My guidelines disappeared or changed.
With **Automatic guidelines** on, they are redrawn after every timing change and replace the ones that were in the level.
Turn the setting off in the [settings](Settings) if you want to keep hand-made guidelines.

### The editor buttons cover other buttons (BetterEdit and others).
Move them: ESC → the note button next to Help, then drag. See [Editor Buttons](Editor-Buttons).

### The buttons are gone while I playtest.
That is the setting **Hide buttons while playtesting**. Turn it off in the settings, or in the layout window (ESC → the note button).

### I uploaded my level and the song is missing.
Local songs exist only on your PC. Use **Level → Restore song** before publishing. See [Local Songs](Local-Songs).

### The window and the editor have different volume.
The window uses the game's music volume. Change it with the **Music** slider in the Playback tab, or in the game's settings.

### Can I undo a timing change?
Yes: **Ctrl+Z** / **Ctrl+Y**. See [Timing Points](Timing-Points#undo-and-redo).

### Where are my timing and settings stored?
One file per level in `%LOCALAPPDATA%\GeometryDash\geode\mods\bychke.gd-timing-analyzer\levels`.
Use **Files → Export level** for a backup.

### It does not work with my game version.
The mod is made for Geometry Dash **2.2081** on **Windows** with Geode **5.10.1** or newer.
