# Triggers

The **Triggers** tab puts **pulse**, **spawn** or **color** triggers into your level, exactly on the beats of your
timing. Speed portals are taken into account, and [rhythm patterns](Rhythm-Patterns) are followed.

Open the **Triggers** tab in the [timing window](Timing-Window). It needs the level editor and at least one
[timing point](Timing-Points).

![Triggers tab](https://raw.githubusercontent.com/bychke/gd-timing-analyzer/main/docs/screens/triggers.png)

## Quick start

1. Choose **what** to make and **how often** (row 1).
2. Pick a **Preset**, or set the steps yourself (rows 3 and 4).
3. Scroll the editor to the height where you want the triggers, open the window and press **Place triggers**.
4. Don't like it? **One Ctrl+Z** in the editor removes all of them.

## Row 1: what and how often

| Option | Choices | What it means |
|---|---|---|
| **Make** | **Pulse**, **Spawn**, **Color** | The trigger to make. **Color** sets a color channel to a color right away (static, no fade) |
| **As** | **ON BEATS**, **SPAWN LOOP** | **ON BEATS**: one trigger on every beat. Always exact, and works from start positions. **SPAWN LOOP**: one spawn loop that repeats itself (much fewer objects), with a stop trigger at the end |
| **Every** | **BAR**, **1/1**, **1/2** ... **1/8**, **HITS** | **BAR** = the first beat of every bar. **1/1** = every beat. **1/2** ... **1/8** = also between the beats. **HITS** = the hits the mod found in the song |

Inside a [rhythm pattern](Rhythm-Patterns) the triggers play the pattern's hits instead (the spawn loop too).

**SPAWN LOOP** makes a new free group for the loop: one loop per BPM and per rhythm pattern.

## Row 2: which part of the song

| Option | What it does |
|---|---|
| **Start** | Where the triggers start, in seconds. The **cursor** button puts the yellow cursor there |
| **End** | Where they end, in seconds. The **cursor** button puts the yellow cursor there |

An empty field means *from the start* / *to the end* of the song. The chosen part is **green** on the waveform.

## Row 3: Steps

The **steps** are what the triggers do, **one after another**, and then again from the first one.

- The **arrows** set how many steps there are (1 to 8).
- **Click a square** to choose it. Its settings are in row 4.

**Example:** **Every 1/1** with two steps, **pink** and **blue**: beat 1 is pink, beat 2 blue, beat 3 pink, beat 4 blue ...
With one step, every trigger does the same.

## Row 4: the selected step

For **Pulse** and **Color**:

| Option | What it does |
|---|---|
| **Color** swatch | Opens the color picker |
| **+** | Brightens the channel's **own** color (a flash in the color the object already has) |
| **X** | Nothing on this step (a pause) |
| **On** | What changes color: **BG**, **GROUND**, **OBJECTS** or **LINE** |

For **Spawn**:

| Option | What it does |
|---|---|
| **Groups** | The groups this step spawns, for example `1,5,12`. Empty = nothing on this step |

## Right side

| Button | What it does |
|---|---|
| **Preset** | Ready steps to start from: **every beat**, **two colors**, **alternate**, **accent**, **rainbow** |
| (text) | How many triggers will be placed |
| **Place triggers** | Puts them into the level |

## Where they go

- **Horizontally**: on the beats, at the right place for every speed portal.
- **Vertically**: at the height of the middle of the editor screen. Scroll the editor where you want them before you open the window.
- On the current editor **layer**.
- All of them are **one undo step**: one **Ctrl+Z** in the editor removes them.
