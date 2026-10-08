#include "Session.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/DrawGridLayer.hpp>
#include <Geode/binding/EditLevelLayer.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>

#include <fstream>
#include <sstream>
#include <thread>

using namespace geode::prelude;

Session& Session::get() {
    static Session s;
    return s;
}

std::filesystem::path Session::savePathFor(std::filesystem::path const& audio) const {
    std::error_code ec;
    auto size = std::filesystem::file_size(audio, ec);
    auto name = utils::string::pathToString(audio.filename());
    std::string safe;
    for (char c : name) safe += (std::isalnum((unsigned char)c) || c == '.' || c == '-') ? c : '_';
    return Mod::get()->getSaveDir() / "timing" / fmt::format("{}_{}.txt", safe, ec ? 0 : size);
}

// ---------------- per-level data ----------------

GJGameLevel* Session::currentLevel() {
    if (auto lel = LevelEditorLayer::get()) return lel->m_level;
    if (auto scene = CCDirector::get()->getRunningScene())
        if (auto ell = scene->getChildByType<EditLevelLayer>(0)) return ell->m_level;
    return nullptr;
}

std::string Session::levelKey() {
    auto lvl = currentLevel();
    if (!lvl) return "";
    int id = lvl->m_levelID.value();
    if (lvl->m_levelType != GJLevelType::Editor && id > 0) return fmt::format("online-{}", id);
    if (lvl->m_M_ID > 0) return fmt::format("local-{}", lvl->m_M_ID);
    std::string safe;
    for (char c : std::string(lvl->m_levelName)) safe += std::isalnum((unsigned char)c) ? c : '_';
    return "local-" + safe;
}

std::string Session::levelName() {
    auto lvl = currentLevel();
    return lvl ? std::string(lvl->m_levelName) : "";
}

std::filesystem::path Session::levelFile(std::string const& key) const {
    return Mod::get()->getSaveDir() / "levels" / (key + ".json");
}

matjson::Value& Session::levelData() {
    auto key = levelKey();
    if (key != m_levelKey) {
        m_levelKey = key;
        m_levelData = matjson::Value::object();
        std::error_code ec;
        if (!key.empty() && std::filesystem::exists(levelFile(key), ec)) {
            std::ifstream in(levelFile(key), std::ios::binary);
            std::stringstream ss; ss << in.rdbuf();
            if (auto res = matjson::parse(ss.str()); res && res.unwrap().isObject()) m_levelData = res.unwrap();
        }
    }
    return m_levelData;
}

void Session::writeLevelData() {
    if (m_levelKey.empty()) return;
    auto p = levelFile(m_levelKey);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    m_levelData["level"] = levelName();
    std::ofstream(p, std::ios::binary) << m_levelData.dump();
}

matjson::Value Session::setting(char const* key, matjson::Value def) {
    auto& data = levelData();
    if (!m_levelKey.empty() && data["settings"].isObject() && data["settings"].contains(key))
        return data["settings"][key];
    // not set for this level yet -> default (every level starts with the default settings)
    if (!m_levelKey.empty()) return def;
    // no level open -> last value used anywhere
    auto global = Mod::get()->getSavedValue<matjson::Value>(key);
    return global.isNull() ? def : global;
}

void Session::setSetting(char const* key, matjson::Value value) {
    Mod::get()->setSavedValue(key, value);
    auto& data = levelData();
    if (m_levelKey.empty()) return;
    if (!data["settings"].isObject()) data["settings"] = matjson::Value::object();
    data["settings"][key] = value;
    writeLevelData();
}

double Session::settingDouble(char const* key, double def) { return setting(key, def).asDouble().unwrapOr(def); }
bool Session::settingBool(char const* key, bool def) { return setting(key, def).asBool().unwrapOr(def); }

void Session::save() {
    if (audioPath.empty()) return;
    auto text = map.serialize();
    auto& data = levelData();
    if (m_levelKey.empty()) {
        // no level open -> per audio file
        auto p = savePathFor(audioPath);
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        std::ofstream(p, std::ios::binary) << text;
        return;
    }
    // per level
    data["timing"] = text;
    data["song"] = utils::string::pathToString(audioPath.filename());
    data["audio"] = utils::string::pathToString(audioPath);
    writeLevelData();
    m_mapLevelKey = m_levelKey;
}

void Session::unloadEverything() {
    ++m_generation; // drop results of background work still running
    busy = false;
    std::error_code ec;
    if (!audioPath.empty()) std::filesystem::remove(savePathFor(audioPath), ec);
    auto& data = levelData();
    if (!m_levelKey.empty()) {
        // keep only the settings of this level
        auto kept = matjson::Value::object();
        if (data["settings"].isObject()) kept["settings"] = data["settings"];
        m_levelData = kept;
        writeLevelData();
    }
    audioPath.clear();
    audio.reset();
    envelope.reset();
    map = {};
    firstBeatMs = -1;
    m_mapLevelKey = m_levelKey;
    unloaded = true;
    sessionChanged();
}

void Session::loadTimingFor(std::filesystem::path const& path) {
    unloaded = false;
    if (path != audioPath) {
        audio.reset();
        envelope.reset();
        firstBeatMs = -1;
    }
    audioPath = path;
    map = {};
    // every level has its own timing (a level never used with the mod starts empty)
    auto& data = levelData();
    m_mapLevelKey = m_levelKey;
    if (!m_levelKey.empty()) {
        if (data["timing"].isString() &&
            data["song"].asString().unwrapOr("") == utils::string::pathToString(path.filename()))
            map = TimingMap::deserialize(data["timing"].asString().unwrap());
        return;
    }
    // no level open -> timing saved for this audio file
    if (auto p = savePathFor(path); std::filesystem::exists(p)) {
        std::ifstream in(p, std::ios::binary);
        std::stringstream ss; ss << in.rdbuf();
        map = TimingMap::deserialize(ss.str());
    }
}

void Session::enterLevel() {
    fixOldSongId();
    auto key = levelKey();
    if (key == m_activeLevel) return;
    m_activeLevel = key;
    ++m_generation; // drop background work of the previous level
    busy = false;
    unloaded = false;
    audioPath.clear();
    audio.reset();
    envelope.reset();
    map = {};
    firstBeatMs = -1;
    // the file this level used last time (e.g. a loaded mp3), else the level's own song
    std::filesystem::path song;
    auto& data = levelData();
    if (auto a = data["audio"].asString(); a && !a.unwrap().empty()) {
        std::filesystem::path p = utils::string::utf8ToWide(a.unwrap());
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) song = p;
    }
    if (song.empty()) song = currentLevelSongPath();
    m_mapLevelKey = m_levelKey;
    if (!song.empty()) loadTimingFor(song);
    sessionChanged();
}

bool Session::timingIsForCurrentLevel() {
    levelData();
    return m_mapLevelKey == m_levelKey;
}

bool Session::exportLevel(std::filesystem::path const& path) {
    auto& data = levelData();
    auto out = matjson::Value::object();
    out["format"] = "gd-timing-analyzer-level";
    out["version"] = 1;
    out["level"] = levelName();
    out["song"] = utils::string::pathToString(audioPath.filename());
    out["timing"] = map.serialize();
    out["settings"] = data["settings"].isObject() ? data["settings"] : matjson::Value::object();
    if (auto ls = levelSettings()) out["songOffset"] = ls->m_songOffset;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << out.dump();
    return true;
}

bool Session::importLevel(std::filesystem::path const& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { if (error) *error = "Cannot open file"; return false; }
    std::stringstream ss; ss << in.rdbuf();
    auto res = matjson::parse(ss.str());
    auto format = res ? res.unwrap()["format"].asString().unwrapOr("") : std::string();
    if (format != "gd-timing-analyzer-level") {
        if (error) *error = "Not a Geometry Dash Timing Analyzer level file";
        return false;
    }
    auto json = res.unwrap();
    map = TimingMap::deserialize(json["timing"].asString().unwrapOr(""));
    auto& data = levelData();
    if (json["settings"].isObject()) {
        data["settings"] = json["settings"];
        for (auto& [key, value] : json["settings"]) Mod::get()->setSavedValue(key, value);
    }
    if (auto off = json["songOffset"].asDouble(); off) {
        if (auto ls = levelSettings()) ls->m_songOffset = (float)*off;
    }
    save();
    return true;
}

LevelSettingsObject* Session::levelSettings() {
    if (auto layer = Session::get().openSongLayer(); layer && layer->m_songDelegate)
        if (auto ls = layer->m_songDelegate->getLevelSettings()) return ls;
    if (auto lel = LevelEditorLayer::get()) return lel->m_levelSettings;
    return nullptr;
}

bool Session::setSongStartOffset(double ms) {
    auto ls = levelSettings();
    if (!ls) return false;
    ls->m_songOffset = (float)std::max(0.0, ms / 1000.0);
    FMODAudioEngine::get()->stopAllMusic(true);
    return true;
}

bool Session::editorMetronome() { return settingBool("editor-metronome", false); }
void Session::setEditorMetronome(bool on) { setSetting("editor-metronome", on); }
bool Session::editorWaveform() { return settingBool("editor-waveform", false); }
void Session::setEditorWaveform(bool on) { setSetting("editor-waveform", on); }

void Session::loadAudio(std::filesystem::path const& path) {
    if (busy) return;
    unloaded = false;
    audioPath = path;
    audio.reset();
    envelope.reset();
    firstBeatMs = -1;
    loadTimingFor(path);
    busy = true;
    busyText = "Loading audio";
    progress = 0.f;
    unsigned gen = ++m_generation;
    std::thread([this, path, gen] {
        auto res = decodeAudio(path);
        std::shared_ptr<AudioData> data;
        std::shared_ptr<OnsetEnvelope> env;
        std::string err;
        if (res) {
            data = std::make_shared<AudioData>(std::move(res).unwrap());
            env = std::make_shared<OnsetEnvelope>(computeOnsetEnvelope(data->mono, data->sampleRate, &progress));
        } else {
            err = res.unwrapErr();
        }
        queueInMainThread([this, gen, data, env, err] {
            if (gen != m_generation) return;
            busy = false;
            audio = data;
            envelope = env;
            if (!err.empty()) Notification::create(err, NotificationIcon::Error)->show();
            sessionChanged();
        });
    }).detach();
}

void Session::analyze(AnalyzerSettings settings) {
    if (busy || !envelope) return;
    busy = true;
    busyText = "Analyzing";
    progress = 0.6f;
    unsigned gen = ++m_generation;
    auto env = envelope;
    std::thread([this, env, settings, gen] {
        auto res = std::make_shared<AnalysisResult>(analyzeTiming(*env, settings, &progress));
        queueInMainThread([this, res, gen] {
            if (gen != m_generation) return;
            busy = false;
            if (res->map.empty()) {
                Notification::create("No beat detected", NotificationIcon::Error)->show();
            } else {
                map = res->map;
                firstBeatMs = res->firstBeatMs;
                save();
                Notification::create(fmt::format("Detected {} timing point(s)", map.points.size()),
                    NotificationIcon::Success)->show();
            }
            sessionChanged();
        });
    }).detach();
}

bool Session::importOsu(std::filesystem::path const& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { if (error) *error = "Cannot open file"; return false; }
    std::stringstream ss; ss << in.rdbuf();
    std::string audioName;
    auto m = TimingMap::fromOsu(ss.str(), &audioName);
    if (m.empty()) { if (error) *error = "No timing points in this file"; return false; }
    map = m;
    // if the beatmap's audio file sits next to the .osu and nothing is loaded yet - load it too
    if (!audio && !busy && !audioName.empty()) {
        auto a = path.parent_path() / audioName;
        if (std::filesystem::exists(a)) {
            loadAudio(a);
            map = m;
        }
    }
    save();
    return true;
}

bool Session::exportOsu(std::filesystem::path const& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "osu file format v14\n\n" << map.toOsuSection();
    return true;
}

int Session::applyGuidelines(int divisor) {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_levelSettings || map.empty()) return 0;
    double offsetMs = lel->m_levelSettings->m_songOffset * 1000.0;
    double endMs = audio ? audio->lengthMs : map.points.back().time + 600000.0;

    std::string out;
    int count = 0;
    map.forEachTick(0, endMs, std::max(1, divisor), [&](double t, TickKind k) {
        // GD guidelines use song time (the editor shifts them by the song offset itself);
        // skip the beats before the level's start offset
        if (t < offsetMs - 1) return;
        double sec = t / 1000.0;
        // GD guideline colors: 0.8 orange, 0.9 yellow, 1.0 green
        float color = k == TickKind::Downbeat ? 1.0f : (k == TickKind::Beat ? 0.9f : 0.8f);
        out += fmt::format("{:.4f}~{}~", sec, color);
        count++;
    });
    lel->m_levelSettings->m_guidelineString = out;
    lel->m_levelSettings->m_guidelinesUpdated = true;
    if (lel->m_drawGridLayer) lel->m_drawGridLayer->loadTimeMarkers(out);
    return count;
}

bool Session::clearGuidelines() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_levelSettings) return false;
    lel->m_levelSettings->m_guidelineString = "";
    lel->m_levelSettings->m_guidelinesUpdated = true;
    if (lel->m_drawGridLayer) lel->m_drawGridLayer->loadTimeMarkers("");
    return true;
}

std::optional<double> Session::songTimeAtLevelPos(CCPoint pos) {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_levelSettings) return std::nullopt;
    float sec = lel->timeForPos(pos, 0, 0, false, 0);
    return (sec + lel->m_levelSettings->m_songOffset) * 1000.0;
}

std::optional<double> Session::editorSongTime(bool* playing) {
    if (playing) *playing = false;
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_editorUI || !lel->m_levelSettings || !lel->m_objectLayer) return std::nullopt;
    auto engine = FMODAudioEngine::get();
    if (lel->m_editorUI->m_isPlayingMusic && engine->isMusicPlaying(0)) {
        if (playing) *playing = true;
        return (double)engine->getMusicTimeMS(0);
    }
    auto win = CCDirector::get()->getWinSize();
    return songTimeAtLevelPos(lel->m_objectLayer->convertToNodeSpace(win / 2));
}

void Session::editorGoTo(double ms, bool play) {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_editorUI || !lel->m_levelSettings || !lel->m_objectLayer) return;
    ms = std::max(0.0, ms);
    float sec = std::max(0.f, float(ms / 1000.0 - lel->m_levelSettings->m_songOffset));
    auto pos = lel->posForTime(sec);
    auto layer = lel->m_objectLayer;
    auto win = CCDirector::get()->getWinSize();
    // put that moment in the middle of the screen (keep the vertical scroll)
    layer->setPositionX(std::min(0.f, win.width / 2 - pos.x * layer->getScale()));

    auto ui = lel->m_editorUI;
    if (ui->m_isPlayingMusic) ui->onPlayback(nullptr); // stop the old playback first
    if (!play) return;
    ui->onPlayback(nullptr);
    if (ui->m_isPlayingMusic) FMODAudioEngine::get()->setMusicTimeMS((unsigned)ms, true, 0);
}

Ref<CustomSongLayer> Session::openSongLayer() const {
    // looked up on screen every time (a stored reference kept the closed window alive)
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return nullptr;
    auto layer = scene->getChildByType<CustomSongLayer>(0);
    if (!layer || !layer->isRunning()) return nullptr;
    return layer;
}

// Active NONG of a song ID set with the Jukebox mod (fleym.nongd), read from its manifest
static std::filesystem::path jukeboxSongPath(int id) {
    auto file = dirs::getModsSaveDir() / "fleym.nongd" / "manifest" / fmt::format("{}.json", id);
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return {};
    std::ifstream in(file, std::ios::binary);
    std::stringstream ss; ss << in.rdbuf();
    auto parsed = matjson::parse(ss.str());
    if (!parsed) return {};
    auto json = parsed.unwrap();
    auto active = json["active"].asString().unwrapOr("");
    if (active.empty()) return {};

    auto check = [&](matjson::Value const& song) -> std::filesystem::path {
        if (song["unique_id"].asString().unwrapOr("") != active) return {};
        auto path = song["path"].asString().unwrapOr("");
        if (path.empty()) return {};
        std::filesystem::path p = utils::string::utf8ToWide(path);
        std::error_code ec2;
        return std::filesystem::exists(p, ec2) ? p : std::filesystem::path{};
    };
    if (auto p = check(json["default"]); !p.empty()) return p;
    for (auto key : { "locals", "youtube", "hosted" }) {
        if (!json[key].isArray()) continue;
        for (auto& song : json[key]) if (auto p = check(song); !p.empty()) return p;
    }
    return {};
}

std::filesystem::path Session::currentLevelSongPath() {
    int id = 0;
    if (auto layer = Session::get().openSongLayer(); layer && layer->m_songDelegate)
        id = layer->m_songDelegate->getActiveSongID();
    else if (auto lel = LevelEditorLayer::get(); lel && lel->m_level)
        id = lel->m_level->m_songID;
    if (id <= 0) return {};

    if (auto p = Session::get().songOverride(id)) return *p;
    if (auto p = jukeboxSongPath(id); !p.empty()) return p;
    std::filesystem::path p = utils::string::utf8ToWide(std::string(MusicDownloadManager::sharedState()->pathForSong(id)));
    std::error_code ec;
    return std::filesystem::exists(p, ec) ? p : std::filesystem::path{};
}

// ---------------- local songs ----------------

// Local song IDs created by the mod. They must stay below 10000000 - GD treats IDs from there up as
// Music Library songs, which crashed the song widget.
static constexpr int LOCAL_ID_BASE = 9000000;
static constexpr int LOCAL_ID_END = 10000000;
// IDs used by older versions of the mod (moved into the range above on load)
static constexpr int OLD_LOCAL_ID_BASE = 999000000;

static bool isLocalId(int id) { return (id >= LOCAL_ID_BASE && id < LOCAL_ID_END) || id >= OLD_LOCAL_ID_BASE; }

void Session::loadOverrides() const {
    if (m_overridesLoaded) return;
    m_overridesLoaded = true;
    auto orig = Mod::get()->getSavedValue<matjson::Value>("song-originals");
    if (orig.isObject()) {
        for (auto& [key, value] : orig) {
            auto id = numFromString<int>(key);
            auto o = value.asInt();
            if (id && o) m_originals[*id] = (int)*o;
        }
    }
    auto json = Mod::get()->getSavedValue<matjson::Value>("song-overrides");
    if (!json.isObject()) return;
    for (auto& [key, value] : json) {
        auto id = numFromString<int>(key);
        auto path = value.asString();
        if (id && path) m_overrides[*id] = std::filesystem::path(utils::string::utf8ToWide(*path));
    }
    // move IDs of older versions (>= 10000000 = Music Library range) to the safe range
    bool changed = false;
    for (auto it = m_overrides.begin(); it != m_overrides.end();) {
        if (it->first < OLD_LOCAL_ID_BASE) { ++it; continue; }
        int fresh = LOCAL_ID_BASE;
        while (m_overrides.contains(fresh)) fresh++;
        m_migrated[it->first] = fresh;
        if (auto o = m_originals.find(it->first); o != m_originals.end()) {
            m_originals[fresh] = o->second;
            m_originals.erase(o);
        }
        auto path = it->second;
        it = m_overrides.erase(it);
        m_overrides[fresh] = path;
        changed = true;
    }
    if (changed) {
        saveOverrides();
        Mod::get()->setSavedValue("song-migrated", [&] {
            auto j = matjson::Value::object();
            for (auto const& [o, n] : m_migrated) j[std::to_string(o)] = n;
            return j;
        }());
    } else if (auto mig = Mod::get()->getSavedValue<matjson::Value>("song-migrated"); mig.isObject()) {
        for (auto& [key, value] : mig) {
            auto id = numFromString<int>(key);
            auto n = value.asInt();
            if (id && n) m_migrated[*id] = (int)*n;
        }
    }
}

void Session::fixOldSongId() {
    loadOverrides();
    auto lvl = currentLevel();
    if (!lvl) return;
    if (auto it = m_migrated.find(lvl->m_songID); it != m_migrated.end()) lvl->m_songID = it->second;
}

void Session::saveOverrides() const {
    auto json = matjson::Value::object();
    for (auto const& [id, path] : m_overrides) json[std::to_string(id)] = utils::string::pathToString(path);
    Mod::get()->setSavedValue("song-overrides", json);
    auto orig = matjson::Value::object();
    for (auto const& [id, o] : m_originals) orig[std::to_string(id)] = o;
    Mod::get()->setSavedValue("song-originals", orig);
}

std::optional<std::filesystem::path> Session::songOverride(int id) const {
    loadOverrides();
    auto it = m_overrides.find(id);
    if (it == m_overrides.end()) {
        auto m = m_migrated.find(id);
        if (m == m_migrated.end() || !m_overrides.contains(m->second)) return std::nullopt;
        it = m_overrides.find(m->second);
    }
    std::error_code ec;
    if (!std::filesystem::exists(it->second, ec)) return std::nullopt;
    return it->second;
}

// Applies a song ID to whatever the Custom Song window edits (level settings / level info), or to the open editor level
static void applySongId(CustomSongLayer* layer, int id) {
    auto lel = LevelEditorLayer::get();
    if (layer && layer->m_songDelegate) {
        layer->m_songDelegate->songIDChanged(id);
        if (layer->m_songIDInput) layer->m_songIDInput->setString(id > 0 ? std::to_string(id) : "");
        if (layer->m_songWidget && id > 0)
            layer->m_songWidget->updateSongObject(MusicDownloadManager::sharedState()->getSongInfoObject(id));
    } else if (lel && lel->m_level) {
        lel->m_level->m_songID = id;
    }
    // the level reopens the song from the new path next time music starts
    FMODAudioEngine::get()->stopAllMusic(true);
}

static int activeSongId(CustomSongLayer* layer) {
    if (layer && layer->m_songDelegate) return layer->m_songDelegate->getActiveSongID();
    auto lel = LevelEditorLayer::get();
    return lel && lel->m_level ? lel->m_level->m_songID : 0;
}

int Session::useAudioInLevel() {
    auto layer = openSongLayer();
    auto lel = LevelEditorLayer::get();
    if (audioPath.empty() || (!layer && (!lel || !lel->m_level))) return 0;
    loadOverrides();
    int id = activeSongId(layer);
    // Always use a private local ID: other song mods (NONG managers) own the real IDs and would override our file.
    if (!isLocalId(id) || id >= OLD_LOCAL_ID_BASE) {
        int original = id;
        id = LOCAL_ID_BASE;
        while (m_overrides.contains(id)) id++;
        m_originals[id] = original;
    }
    m_overrides[id] = audioPath;
    saveOverrides();
    applySongId(layer, id);
    return id;
}

bool Session::restoreLevelSong() {
    auto layer = openSongLayer();
    loadOverrides();
    int id = activeSongId(layer);
    if (!m_overrides.erase(id)) return false;
    int original = 0;
    if (auto it = m_originals.find(id); it != m_originals.end()) { original = it->second; m_originals.erase(it); }
    saveOverrides();
    if (isLocalId(id)) applySongId(layer, original);
    else FMODAudioEngine::get()->stopAllMusic(true);
    return true;
}
