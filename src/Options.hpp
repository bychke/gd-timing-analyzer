#pragma once
#include <Geode/Geode.hpp>

#include <map>
#include <string>

// Options that are edited in the TIME window or its Editor Settings preview. They are kept as saved values
// (not in mod.json), so they don't show up a second time in Geode's settings list.
namespace opt {

inline constexpr int64_t rgba(int r, int g, int b, int a) {
    return ((int64_t)r << 24) | ((int64_t)g << 16) | ((int64_t)b << 8) | (int64_t)a;
}

inline std::map<std::string, matjson::Value> const& defaults() {
    static std::map<std::string, matjson::Value> const d = {
        // metronome (TIME > Playback)
        { "metronome-volume", (int64_t)70 }, { "metronome-sound", std::string("classic") },
        { "metronome-ticks", std::string("1/1") }, { "metronome-accent", (int64_t)160 },
        // editor buttons / texts (Editor Settings)
        { "show-object-beat", true }, { "center-text-colors", true }, { "text-x", (int64_t)0 }, { "text-y", (int64_t)0 },
        { "hide-buttons-playtest", true }, { "buttons-layout", std::string("row") }, { "buttons-x", (int64_t)0 },
        { "buttons-y", (int64_t)0 }, { "buttons-scale", 1.0 },
        // which of the editor buttons are shown
        { "show-btn-bpm", true }, { "show-btn-wave", true }, { "show-btn-time", true }, { "show-btn-div", true },
        // colors (Editor Settings)
        { "guideline-thickness", 0.4 }, { "editor-waveform-color", rgba(89, 204, 255, 56) },
        { "guide-color-1", rgba(255, 255, 255, 255) }, { "guide-color-2", rgba(237, 17, 33, 255) },
        { "guide-color-3", rgba(136, 102, 238, 255) }, { "guide-color-4", rgba(102, 204, 255, 255) },
        { "guide-color-6", rgba(204, 136, 238, 255) }, { "guide-color-8", rgba(255, 204, 34, 255) },
        { "guide-color-12", rgba(192, 144, 238, 255) }, { "guide-color-16", rgba(142, 142, 142, 255) },
    };
    return d;
}

inline matjson::Value const& def(std::string const& key) { return defaults().at(key); }

template <class T> T get(std::string const& key);
template <class T> void set(std::string const& key, T value);

template <> inline bool get<bool>(std::string const& key) {
    return geode::Mod::get()->getSavedValue<bool>(key, def(key).asBool().unwrapOr(false));
}
template <> inline int64_t get<int64_t>(std::string const& key) {
    return geode::Mod::get()->getSavedValue<int64_t>(key, def(key).asInt().unwrapOr(0));
}
template <> inline double get<double>(std::string const& key) {
    return geode::Mod::get()->getSavedValue<double>(key, def(key).asDouble().unwrapOr(0.0));
}
template <> inline std::string get<std::string>(std::string const& key) {
    return geode::Mod::get()->getSavedValue<std::string>(key, def(key).asString().unwrapOr(""));
}
template <> inline cocos2d::ccColor4B get<cocos2d::ccColor4B>(std::string const& key) {
    auto v = geode::Mod::get()->getSavedValue<int64_t>(key, def(key).asInt().unwrapOr(0));
    return { (GLubyte)(v >> 24 & 255), (GLubyte)(v >> 16 & 255), (GLubyte)(v >> 8 & 255), (GLubyte)(v & 255) };
}

template <> inline void set<bool>(std::string const& key, bool v) { geode::Mod::get()->setSavedValue(key, v); }
template <> inline void set<int64_t>(std::string const& key, int64_t v) { geode::Mod::get()->setSavedValue(key, v); }
template <> inline void set<double>(std::string const& key, double v) { geode::Mod::get()->setSavedValue(key, v); }
template <> inline void set<std::string>(std::string const& key, std::string v) {
    geode::Mod::get()->setSavedValue(key, std::move(v));
}
template <> inline void set<cocos2d::ccColor4B>(std::string const& key, cocos2d::ccColor4B c) {
    geode::Mod::get()->setSavedValue<int64_t>(key, rgba(c.r, c.g, c.b, c.a));
}

inline void reset(std::string const& key) {
    auto const& v = def(key);
    if (v.isBool()) set<bool>(key, v.asBool().unwrapOr(false));
    else if (v.isString()) set<std::string>(key, v.asString().unwrapOr(""));
    else if (key == "guideline-thickness" || key == "buttons-scale") set<double>(key, v.asDouble().unwrapOr(0.0));
    else set<int64_t>(key, v.asInt().unwrapOr(0));
}

} // namespace opt
