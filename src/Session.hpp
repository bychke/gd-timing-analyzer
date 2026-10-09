#pragma once
#include "analysis/Analyzer.hpp"
#include "audio/AudioData.hpp"
#include "timing/TimingMap.hpp"

#include <Geode/Geode.hpp>

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Mod state that outlives the editor window (audio, timing, onset envelope).
class Session {
public:
    static Session& get();

    std::filesystem::path audioPath;
    std::shared_ptr<AudioData> audio;
    std::shared_ptr<OnsetEnvelope> envelope;
    TimingMap map;
    double firstBeatMs = -1;

    // Song selection window the Timing button was opened from (applies the song through its delegate)
    // The open song selection window (Custom Song Selection) on screen, or null. Not stored anywhere:
    // holding a reference kept the closed window and its song widget alive -> crash when music stopped.
    geode::Ref<CustomSongLayer> openSongLayer() const;

    bool busy = false;
    std::string busyText;
    std::atomic<float> progress = 0.f;

    // Decodes audio in the background (+ onset envelope). Also loads the saved timing for that file.
    void loadAudio(std::filesystem::path const& path);
    // Only switches to the saved timing of a file, without decoding it (cheap, used when entering the editor).
    void loadTimingFor(std::filesystem::path const& path);
    // Call when a level may have been opened: a different level starts clean with its own song,
    // its own timing (empty for a level never used with the mod) and default settings.
    void enterLevel();
    // Levels saved with a local song ID of an older version get the new ID
    void fixOldSongId();
    // Background BPM / offset / tempo change detection
    void analyze(AnalyzerSettings settings);
    bool importOsu(std::filesystem::path const& path, std::string* error = nullptr);
    bool exportOsu(std::filesystem::path const& path);

    void save();
    // Removes the song, waveform and every timing point of this level (also the saved files)
    void unloadEverything();
    bool unloaded = false;
    // Writes guidelines into the open GD editor (level divisor + zones). Returns line count.
    int applyGuidelines();
    bool clearGuidelines();
    // After a timing change: redraws the guidelines when "Automatic guidelines" is on (editor only)
    void autoGuidelines();
    // Guideline divisor of the whole level: 1 = beats, 2 = 1/2, 3 = 1/3... (per level)
    int guideDivisor();
    void setGuideDivisor(int divisor);

    // Metronome in the level editor (persisted)
    bool editorMetronome();
    void setEditorMetronome(bool on);
    // Semi-transparent song waveform behind the level in the editor (persisted)
    bool editorWaveform();
    void setEditorWaveform(bool on);

    // --- per-level data (timing + settings are stored separately for every level) ---
    static GJGameLevel* currentLevel();
    static std::string levelKey(); // "" when no level is open
    static std::string levelName();
    // Settings: per level when a level is open (falls back to the last value used anywhere)
    matjson::Value setting(char const* key, matjson::Value def);
    double settingDouble(char const* key, double def);
    bool settingBool(char const* key, bool def);
    void setSetting(char const* key, matjson::Value value);
    // false when the loaded timing belongs to another level (reload it)
    bool timingIsForCurrentLevel();
    // All of this level's mod data (timing, settings, song offset) as one .json file
    bool exportLevel(std::filesystem::path const& path);
    bool importLevel(std::filesystem::path const& path, std::string* error = nullptr);
    // Song "Start Offset" of the level (Custom Song > Settings) = this song time
    bool setSongStartOffset(double ms);
    static LevelSettingsObject* levelSettings();

    // --- local songs in levels ---
    // Makes the open level play the loaded audio file directly (no copying into AppData).
    // Returns the song ID now pointing at the file, or 0 on failure.
    int useAudioInLevel();
    // Removes the local file from the open level's song ID
    bool restoreLevelSong();
    // Local file that replaces a song ID (if any)
    std::optional<std::filesystem::path> songOverride(int id) const;

    // Song used by the open level (empty if none / not downloaded)
    static std::filesystem::path currentLevelSongPath();
    // Song time (ms) of a position in the level, accounting for speed portals and song offset
    static std::optional<double> songTimeAtLevelPos(cocos2d::CCPoint pos);

    // --- editor <-> waveform playback sync ---
    // Positions (object layer) of the leftmost and rightmost selected objects in the editor
    static std::optional<std::pair<cocos2d::CCPoint, cocos2d::CCPoint>> selectedObjectsRange();
    // Song time (ms) the editor is at: the playing music, the leftmost selected object, or the middle of
    // the screen. playing = music runs.
    static std::optional<double> editorSongTime(bool* playing = nullptr);
    // Moves the editor view to a song time (ms) and optionally starts editor music playback from there
    static void editorGoTo(double ms, bool play);

private:
    matjson::Value& levelData();
    void writeLevelData();
    std::filesystem::path levelFile(std::string const& key) const;
    std::string m_levelKey = "\x01"; // forces the first load
    matjson::Value m_levelData;
    std::string m_mapLevelKey;
    std::string m_activeLevel = "\x01";

    void loadOverrides() const;
    void saveOverrides() const;
    mutable std::map<int, std::filesystem::path> m_overrides;
    mutable std::map<int, int> m_originals; // local ID -> song ID the level had before
    mutable std::map<int, int> m_migrated;  // local ID of an older version -> new local ID
    mutable bool m_overridesLoaded = false;

    std::filesystem::path savePathFor(std::filesystem::path const& audio) const;
    unsigned m_generation = 0;
};

// called after background work finishes (UI refresh)
void sessionChanged();
