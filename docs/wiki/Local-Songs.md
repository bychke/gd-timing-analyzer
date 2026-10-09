# Local Songs

A level can play **any audio file from your PC** (mp3, ogg, wav, flac, m4a) instead of a Newgrounds / Music Library song.
This is handy for songs that are not on Newgrounds, or when you want to build on a remix.

## How to use a file

1. Open the [timing window](Timing-Window) from the level (**TIME** in the editor, or **Custom Song → Timing**).
2. **Drag the file onto the game window**, or use **Level → Load audio...**.
3. With the setting `Use loaded song in the level` on (the default), the level plays that file **right away**.
   Otherwise press **Level → Use as level song**.

The mod gives the level a special **local song ID** that points to your file. Nothing is copied: the file stays where it is.
The song widget in the level shows the file's name.

## Going back

**Level → Restore song** puts the level's original song back.

## Other buttons

- **Load level song**: loads the level's own song into the window (also the active song of a **Jukebox / NONG** mod).
  It does not change the level.
- **Song starts here / Reset song start**: sets the level's **Start Offset** (where the song begins) to the cursor.

## Important

- **A local song exists only on your PC.** If you upload the level, other players will not hear it. Before publishing,
  press **Restore song**, or make the level use a real song.
- If you move or rename the file, the level cannot find it. Load it again.
- When timing a level, the window always shows the **waveform of the song the level plays**.
