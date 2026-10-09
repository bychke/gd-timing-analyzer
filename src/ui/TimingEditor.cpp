#include "TimingEditor.hpp"

#include "../Session.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GameManager.hpp>

#include <algorithm>
#include <cmath>
#include <commdlg.h>

using namespace geode::prelude;

namespace {
constexpr float POP_W = 540.f, POP_H = 300.f;
// waveform area (m_mainLayer coordinates)
constexpr float WX = 15.f, WY = 166.f, WW = 510.f, WH = 92.f;
// whole-song overview strip
constexpr float OX = 15.f, OY = 153.f, OW = 510.f, OH = 9.f;
// tab content panel
constexpr float PY0 = 6.f, PY1 = 103.f;

ccColor4F rgba(float r, float g, float b, float a = 1.f) { return { r, g, b, a }; }

std::string fmtTime(double ms) {
    bool neg = ms < 0;
    ms = std::abs(ms);
    int m = int(ms / 60000);
    double s = std::fmod(ms, 60000.0) / 1000.0;
    return fmt::format("{}{}:{:06.3f}", neg ? "-" : "", m, s);
}

ccColor4F tickColor(TickKind k, int divisor, double t, TimingPoint const& p) {
    if (k == TickKind::Downbeat) return rgba(1, 1, 1, 0.9f);
    if (k == TickKind::Beat) return rgba(1, 1, 1, 0.45f);
    // same as osu!: 1/2 red, 1/4 blue, 1/3 purple, rest gray
    double pos = std::fmod((t - p.time) / p.beatLength + 1000.0, 1.0);
    auto isNear = [&](double v) { return std::abs(pos - v) < 1e-3; };
    if (isNear(0.5)) return rgba(1, 0.25f, 0.25f, 0.6f);
    if (isNear(0.25) || isNear(0.75)) return rgba(0.3f, 0.5f, 1, 0.55f);
    if (divisor % 3 == 0 && (isNear(1 / 3.0) || isNear(2 / 3.0))) return rgba(0.8f, 0.3f, 1, 0.55f);
    return rgba(0.7f, 0.7f, 0.7f, 0.35f);
}

// native Windows file dialog (Geode's file::pick crashes this MSVC version)
std::optional<std::filesystem::path> nativePick(bool save, wchar_t const* filter,
                                                std::filesystem::path const& defaultPath = {}) {
    // the mouse click that closes the dialog is also delivered to the game afterwards and would press the
    // same button again (endless dialogs) -> ignore requests while one is open and shortly after it closed
    static bool s_open = false;
    static ULONGLONG s_closedAt = 0;
    if (s_open || GetTickCount64() - s_closedAt < 600) return std::nullopt;
    s_open = true;
    wchar_t buf[MAX_PATH * 4] = {};
    if (!defaultPath.empty()) wcsncpy_s(buf, defaultPath.wstring().c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    auto ext = defaultPath.extension().wstring();
    if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
    ofn.lpstrDefExt = save && !ext.empty() ? ext.c_str() : nullptr;
    BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    s_open = false;
    s_closedAt = GetTickCount64();
    if (!ok) return std::nullopt;
    return std::filesystem::path(buf);
}

// per-level settings (see Session::setting)
double savedDouble(char const* key, double def) { return Session::get().settingDouble(key, def); }
bool savedBool(char const* key, bool def) { return Session::get().settingBool(key, def); }
void saveSetting(char const* key, matjson::Value v) { Session::get().setSetting(key, std::move(v)); }
} // namespace

TimingEditor* TimingEditor::s_current = nullptr;

void sessionChanged() {
    if (TimingEditor::s_current) TimingEditor::s_current->onSessionChanged();
}

TimingEditor* TimingEditor::create() {
    auto ret = new TimingEditor();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void TimingEditor::open(std::filesystem::path const& audio) {
    auto& s = Session::get();
    s.enterLevel();
    auto path = audio;
    // after "Unload everything" the window stays empty until something is loaded again
    if (path.empty() && !s.unloaded) path = s.audioPath.empty() ? Session::currentLevelSongPath() : s.audioPath;
    // decode if this file is not loaded yet (the editor may only have loaded its timing)
    if (!path.empty() && !s.busy && (path != s.audioPath || !s.audio)) s.loadAudio(path);
    else if (!path.empty() && !s.busy && !s.timingIsForCurrentLevel()) s.loadTimingFor(path);
    if (s_current) return;
    if (auto ed = create()) ed->show();
}

void TimingEditor::openFromEditor() {
    if (s_current) return;
    bool playing = false;
    auto time = Session::editorSongTime(&playing);
    // the waveform shows the level's own song
    auto song = Session::currentLevelSongPath();
    open(song);
    if (!s_current) return;
    s_current->m_fromEditor = true;
    if (!time) return;
    if (playing) Session::editorGoTo(*time, false); // stop the editor music, the popup continues it
    s_current->m_selected = Session::get().map.empty() ? -1 : s_current->m_selected;
    s_current->m_cursor = std::max(0.0, *time);
    s_current->m_viewStart = s_current->m_cursor - WW * s_current->m_msPerPx * 0.5;
    if (playing) s_current->togglePlay();
    s_current->refreshPanel();
}

// ---------------- building blocks ----------------

CCMenuItemSpriteExtra* TimingEditor::addButton(CCMenu* menu, char const* text, CCPoint pos, geode::Function<void()> cb,
                                               char const* bg, CCLabelBMFont** labelOut) {
    auto spr = ButtonSprite::create(text, "bigFont.fnt", bg, .7f);
    spr->setScale(.42f);
    if (labelOut) *labelOut = spr->m_label;
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    menu->addChild(btn);
    return btn;
}

TimingEditor::Tab& TimingEditor::addTab(char const* name, float x) {
    Tab tab;
    tab.node = CCNode::create();
    m_mainLayer->addChild(tab.node);
    tab.menu = CCMenu::create();
    tab.menu->setPosition({ 0, 0 });
    tab.node->addChild(tab.menu);
    int idx = (int)m_tabs.size();
    tab.button = addButton(m_menu, name, { x, 115 }, [this, idx] { switchTab(idx); }, "GJ_button_04.png", &tab.label);
    m_tabs.push_back(tab);
    return m_tabs.back();
}

// spreads the tab buttons evenly over the window width
void TimingEditor::layoutTabs() {
    float left = 12, right = POP_W - 12;
    float total = 0;
    for (auto const& t : m_tabs) total += t.button->getScaledContentSize().width;
    float avail = right - left;
    float gap = m_tabs.size() > 1 ? std::max(1.f, (avail - total) / (m_tabs.size() - 1)) : 0;
    float x = left;
    for (auto const& t : m_tabs) {
        float w = t.button->getScaledContentSize().width;
        t.button->setPositionX(x + w / 2);
        x += w + gap;
    }
}

void TimingEditor::switchTab(int idx) {
    if (idx < 0 || idx >= (int)m_tabs.size()) idx = 0;
    m_tab = idx;
    Mod::get()->setSavedValue("last-tab", idx);
    for (int i = 0; i < (int)m_tabs.size(); i++) {
        bool on = i == idx;
        m_tabs[i].node->setVisible(on);
        m_tabs[i].menu->setEnabled(on);
        for (auto in : m_tabs[i].inputs) in->setEnabled(on);
        m_tabs[i].label->setColor(on ? ccColor3B{ 255, 220, 60 } : ccColor3B{ 255, 255, 255 });
    }
}

TextInput* TimingEditor::addInput(Tab& tab, char const* caption, float x, float y, float width,
                                  char const* placeholder, CommonFilter filter) {
    auto l = CCLabelBMFont::create(caption, "bigFont.fnt");
    l->setScale(.3f);
    l->setAnchorPoint({ 1, .5f });
    l->setPosition({ x - width * .3f - 4, y });
    tab.node->addChild(l);
    auto in = TextInput::create(width, placeholder);
    in->setCommonFilter(filter);
    in->setScale(.6f);
    in->setPosition({ x, y });
    tab.node->addChild(in);
    tab.inputs.push_back(in);
    return in;
}

void TimingEditor::addDescription(Tab& tab, char const* text, float topY, float x) {
    auto l = CCLabelBMFont::create(text, "chatFont.fnt");
    l->setScale(.42f);
    l->setAnchorPoint({ 0, 1 });
    l->setAlignment(kCCTextAlignmentLeft);
    l->setPosition({ x, topY });
    l->setColor({ 200, 215, 255 });
    tab.node->addChild(l);
}

void TimingEditor::setToggle(CCLabelBMFont* label, char const* name, bool on) {
    if (!label) return;
    label->setString(fmt::format("{}: {}", name, on ? "ON" : "OFF").c_str());
    label->setColor(on ? ccColor3B{ 120, 255, 120 } : ccColor3B{ 255, 140, 140 });
}

// ---------------- layout ----------------

bool TimingEditor::init() {
    if (!Popup::init(POP_W, POP_H)) return false;
    s_current = this;
    this->setTitle("Geometry Dash Timing Analyzer", "goldFont.fnt", .65f, 11.f);

    m_fileLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_fileLabel->setScale(.5f);
    m_fileLabel->setAnchorPoint({ 0, .5f });
    m_fileLabel->setPosition({ 16, POP_H - 27 });
    m_mainLayer->addChild(m_fileLabel);

    m_draw = CCDrawNode::create();
    m_draw->setBlendFunc({ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA });
    m_mainLayer->addChild(m_draw);

    for (int i = 0; i < 24; i++) {
        auto l = CCLabelBMFont::create("", "chatFont.fnt");
        l->setScale(.4f);
        l->setAnchorPoint({ 0, 1 });
        l->setVisible(false);
        m_mainLayer->addChild(l, 2);
        m_markerLabels.push_back(l);
    }

    m_infoLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_infoLabel->setScale(.5f);
    m_infoLabel->setAnchorPoint({ 0, .5f });
    m_infoLabel->setPosition({ WX, 144 });
    m_mainLayer->addChild(m_infoLabel);

    m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_statusLabel->setScale(.5f);
    m_statusLabel->setAnchorPoint({ 1, .5f });
    m_statusLabel->setPosition({ WX + WW, 144 });
    m_statusLabel->setColor({ 120, 255, 120 });
    m_mainLayer->addChild(m_statusLabel);

    // which grid line the cursor is on (bar / beat / 1/8 ...)
    m_gridLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_gridLabel->setScale(.55f);
    m_gridLabel->setAnchorPoint({ 0, .5f });
    m_gridLabel->setPosition({ WX, 131 });
    m_mainLayer->addChild(m_gridLabel);

    // tab bar + always-visible play button
    m_menu = CCMenu::create();
    m_menu->setPosition({ 0, 0 });
    m_mainLayer->addChild(m_menu, 5);

    // ===== 1. Playback =====
    {
        auto& t = addTab("Playback", 50);
        addButton(t.menu, "Play / Pause", { 55, 86 }, [this] { togglePlay(); }, "GJ_button_01.png");
        addButton(t.menu, "Metronome", { 135, 86 }, [this] {
            // one metronome for the waveform and the level editor (BPM ON/OFF button there)
            auto& s = Session::get();
            s.setEditorMetronome(!s.editorMetronome());
            setToggle(m_metroLabel, "Metronome", s.editorMetronome());
        }, "GJ_button_04.png", &m_metroLabel);
        addButton(t.menu, "Grid", { 225, 86 }, [this] {
            static int const divs[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
            int i = 0;
            while (i < 8 && divs[i] != m_divisor) i++;
            m_divisor = divs[(i + 1) % 8];
        }, "GJ_button_04.png", &m_divLabel);
        addButton(t.menu, "Zoom -", { 295, 86 }, [this] { zoom(1.5, m_viewStart + WW * m_msPerPx / 2); });
        addButton(t.menu, "Zoom +", { 345, 86 }, [this] { zoom(1 / 1.5, m_viewStart + WW * m_msPerPx / 2); });
        addButton(t.menu, "Go to cursor", { 420, 86 }, [this] { m_viewStart = m_cursor - WW * m_msPerPx / 2; });
        addButton(t.menu, "To start", { 490, 86 }, [this] { seek(0); m_viewStart = -500; });
        addDescription(t,
            "Play / Pause (Space) - plays from the yellow cursor. Click the waveform\n"
            "   to move the cursor (it snaps to the grid, Alt = free position).\n"
            "Metronome - clicks on every beat, here AND in the editor (BPM button).\n"
            "Grid - snap 1/1 ... 1/16: white = bar, gray = beat, red = 1/2, blue = 1/4.\n"
            "Wheel/arrows - next grid line. Shift+wheel - scroll. Ctrl+wheel - zoom.\n"
            "Drag the waveform to scroll, drag the strip below it to jump anywhere.\n"
            "Editor: WAVE - waveform behind the level. TIME - opens this window at\n"
            "   the level's moment; closing it continues the editor music there.", 70);
        // metronome sound / ticks / first-beat accent (same settings as in the mod options, editor too)
        auto cycleSetting = [](char const* key, std::vector<std::string> const& values) {
            auto mod = Mod::get();
            auto cur = mod->getSettingValue<std::string>(key);
            auto it = std::find(values.begin(), values.end(), cur);
            size_t i = it == values.end() ? 0 : (it - values.begin() + 1) % values.size();
            mod->setSettingValue<std::string>(key, values[i]);
            return values[i];
        };
        addButton(t.menu, "Sound: classic", { 372, 60 }, [this, cycleSetting] {
            auto v = cycleSetting("metronome-sound", { "classic", "wood", "click" });
            m_soundLabel->setString(fmt::format("Sound: {}", v).c_str());
            metronome::click(TickKind::Downbeat); // preview
        }, "GJ_button_04.png", &m_soundLabel);
        addButton(t.menu, "Ticks: 1/1", { 372, 38 }, [this, cycleSetting] {
            auto v = cycleSetting("metronome-ticks", { "1/1", "1/2", "1/3", "1/4" });
            m_ticksLabel->setString(fmt::format("Ticks: {}", v).c_str());
        }, "GJ_button_04.png", &m_ticksLabel);
        addButton(t.menu, "Bar beat: 160%", { 372, 16 }, [this] {
            auto mod = Mod::get();
            int64_t v = mod->getSettingValue<int64_t>("metronome-accent") + 20;
            if (v > 300) v = 100;
            mod->setSettingValue<int64_t>("metronome-accent", v);
            m_accentLabel->setString(fmt::format("Bar beat: {}%", v).c_str());
            metronome::click(TickKind::Downbeat); // preview
        }, "GJ_button_04.png", &m_accentLabel);
        m_soundLabel->setString(fmt::format("Sound: {}", Mod::get()->getSettingValue<std::string>("metronome-sound")).c_str());
        m_ticksLabel->setString(fmt::format("Ticks: {}", Mod::get()->getSettingValue<std::string>("metronome-ticks")).c_str());
        m_accentLabel->setString(fmt::format("Bar beat: {}%", Mod::get()->getSettingValue<int64_t>("metronome-accent")).c_str());
        // volume sliders (bottom right)
        auto addVolume = [&](float y, SEL_MenuHandler handler, float value, CCLabelBMFont*& label) {
            label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setScale(.3f);
            label->setPosition({ 465, y + 11 });
            t.node->addChild(label);
            auto slider = Slider::create(this, handler, .5f);
            slider->setPosition({ 465, y });
            slider->setValue(value);
            t.node->addChild(slider);
            return slider;
        };
        m_musicSlider = addVolume(46, menu_selector(TimingEditor::onMusicVolume),
            FMODAudioEngine::get()->m_musicVolume, m_musicVolLabel);
        m_metroSlider = addVolume(16, menu_selector(TimingEditor::onMetronomeVolume),
            Mod::get()->getSettingValue<int64_t>("metronome-volume") / 100.f, m_metroVolLabel);
        onMusicVolume(nullptr);
        onMetronomeVolume(nullptr);
    }

    // ===== 2. Timing points =====
    {
        auto& t = addTab("Timing Points", 145);
        m_pointLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_pointLabel->setScale(.42f);
        m_pointLabel->setAnchorPoint({ 0, .5f });
        m_pointLabel->setPosition({ 16, 90 });
        t.node->addChild(m_pointLabel);
        float y1 = 90;
        addButton(t.menu, "<", { 105, y1 }, [this] { selectPoint(std::max(0, m_selected - 1)); });
        addButton(t.menu, ">", { 125, y1 }, [this] {
            selectPoint(std::min((int)Session::get().map.points.size() - 1, m_selected + 1));
        });
        addButton(t.menu, "+ Add", { 163, y1 }, [this] { addPointAtCursor(); }, "GJ_button_01.png");
        addButton(t.menu, "Delete", { 210, y1 }, [this] { deletePoint(); }, "GJ_button_06.png");
        addButton(t.menu, "-10", { 260, y1 }, [this] { shiftPoint(-10); });
        addButton(t.menu, "-1", { 287, y1 }, [this] { shiftPoint(-1); });
        addButton(t.menu, "+1", { 310, y1 }, [this] { shiftPoint(1); });
        addButton(t.menu, "+10", { 337, y1 }, [this] { shiftPoint(10); });
        addButton(t.menu, "All points", { 395, y1 }, [this] {
            m_shiftAll = !m_shiftAll;
            setToggle(m_allLabel, "All points", m_shiftAll);
        }, "GJ_button_04.png", &m_allLabel);
        addButton(t.menu, "Snap to hit", { 480, y1 }, [this] {
            auto& s = Session::get();
            if (m_selected < 0 || !s.envelope) return;
            auto& p = s.map.points[m_selected];
            p.time = std::round(nearestOnset(*s.envelope, p.time, p.beatLength * 0.25));
            changed();
        });

        float y2 = 68;
        m_timeInput = addInput(t, "Time (ms)", 85, y2, 80, "ms", CommonFilter::Float);
        m_timeInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || m_selected < 0) return;
            if (auto v = numFromString<double>(str)) {
                Session::get().map.points[m_selected].time = *v;
                changed();
            }
        });
        m_bpmInput = addInput(t, "BPM", 175, y2, 80, "BPM", CommonFilter::Float);
        m_bpmInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || m_selected < 0) return;
            if (auto v = numFromString<double>(str); v && *v >= 10 && *v <= 1000) {
                Session::get().map.points[m_selected].setBpm(*v);
                changed();
            }
        });
        // multiply the BPM of the selected point (e.g. 180 -> x1.5 = 270, /1.5 = 120)
        auto addBpmFactor = [&](char const* text, float x, double factor) {
            addButton(t.menu, text, { x, y2 }, [this, factor] {
                if (m_selected < 0) return;
                Session::get().map.points[m_selected].beatLength /= factor;
                changed();
            });
        };
        addBpmFactor("x2", 222, 2.0);
        addBpmFactor("/2", 246, 0.5);
        addBpmFactor("x1.5", 272, 1.5);
        addBpmFactor("/1.5", 300, 1 / 1.5);
        m_meterInput = addInput(t, "Meter", 365, y2, 40, "4", CommonFilter::Uint);
        m_meterInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || m_selected < 0) return;
            if (auto v = numFromString<int>(str); v && *v >= 1 && *v <= 16) {
                Session::get().map.points[m_selected].meter = *v;
                changed();
            }
        });
        addButton(t.menu, "Downbeat here", { 418, y2 }, [this] {
            auto& s = Session::get();
            if (m_selected < 0) return;
            auto& p = s.map.points[m_selected];
            double k = std::round((m_cursor - p.time) / p.beatLength);
            int shift = (int)(((long long)k % p.meter + p.meter) % p.meter);
            p.time += shift * p.beatLength;
            changed();
        });
        addButton(t.menu, "Tap", { 494, y2 }, [this] { tap(); }, "GJ_button_02.png");
        addButton(t.menu, "Clear all points", { 488, 16 }, [this] {
            createQuickPopup("Clear", "Delete <cr>all</c> timing points?", "Cancel", "Delete", [this](auto, bool yes) {
                if (!yes) return;
                Session::get().map.points.clear();
                m_selected = -1;
                changed();
            });
        }, "GJ_button_06.png");
        addButton(t.menu, "Undo", { 376, 16 }, [this] { undo(); });
        addButton(t.menu, "Redo", { 418, 16 }, [this] { redo(); });
        addDescription(t,
            "Red markers = timing points (like uninherited points in osu!). Each one sets BPM + beats per bar from its\n"
            "   time on. Click a marker to select it, drag to move it (hold Shift to snap to the nearest hit).\n"
            "+ Add (A) - new point at the cursor with the current BPM.  Delete (Del) - removes the selected point.\n"
            "-10 / -1 / +1 / +10 - nudge the point by ms (All points ON = move the whole timing = global offset).\n"
            "Snap to hit - moves the point onto the closest hit.  x2 /2 x1.5 /1.5 - multiply the BPM.  Meter = beats/bar.\n"
            "Downbeat here - makes the beat at the cursor beat 1 of the bar.  Tap (T) - tap along to set the BPM.\n"
            "Clear all points - deletes every timing point.  Undo / Redo - Ctrl+Z / Ctrl+Y.", 52);
    }

    // ===== 2b. Rhythm patterns =====
    {
        m_rhythmTab = (int)m_tabs.size();
        auto& t = addTab("Rhythm", 0);
        float y1 = 90;
        m_rhythmLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_rhythmLabel->setScale(.42f);
        m_rhythmLabel->setAnchorPoint({ 0, .5f });
        m_rhythmLabel->setPosition({ 16, y1 });
        t.node->addChild(m_rhythmLabel);
        addButton(t.menu, "<", { 104, y1 }, [this] { selectRhythm(std::max(0, m_rhythmSel - 1)); });
        addButton(t.menu, ">", { 123, y1 }, [this] {
            selectRhythm(std::min((int)Session::get().map.rhythm.size() - 1, m_rhythmSel + 1));
        });
        addButton(t.menu, "+ Rhythm", { 162, y1 }, [this] { addRhythmPoint(false); }, "GJ_button_01.png");
        addButton(t.menu, "Normal", { 214, y1 }, [this] { addRhythmPoint(true); }, "GJ_button_04.png");
        addButton(t.menu, "Delete", { 260, y1 }, [this] { deleteRhythmPoint(); }, "GJ_button_06.png");
        addButton(t.menu, "Length: 1 bar", { 324, y1 }, [this] {
            auto& s = Session::get();
            if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
            int bars = s.map.rhythm[m_rhythmSel].bars;
            setRhythmBars(bars == 1 ? 2 : bars == 2 ? 4 : 1);
        }, "GJ_button_04.png", &m_rhythmLenLabel);
        // how many times the pattern repeats (all = until the next rhythm point / BPM change)
        addButton(t.menu, "Loops: all", { 400, y1 }, [this] {
            auto& s = Session::get();
            if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
            auto& r = s.map.rhythm[m_rhythmSel];
            if (r.beats.empty()) return;
            static int const loops[] = { 0, 1, 2, 3, 4, 8 };
            int i = 0;
            while (i < 6 && loops[i] != r.loops) i++;
            r.loops = loops[(i + 1) % 6];
            changed();
        }, "GJ_button_04.png", &m_rhythmLoopsLabel);
        addButton(t.menu, "From audio", { 476, y1 }, [this] { rhythmFromAudio(); }, "GJ_button_02.png");

        // bar of the pattern shown in the grid
        m_rhythmPageLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_rhythmPageLabel->setScale(.32f);
        m_rhythmPageLabel->setPosition({ 52, 70 });
        t.node->addChild(m_rhythmPageLabel);
        addButton(t.menu, "<", { 36, 52 }, [this] {
            if (m_rhythmPage > 0) m_rhythmPage--;
            rebuildRhythmGrid();
        });
        addButton(t.menu, ">", { 68, 52 }, [this] {
            m_rhythmPage++;
            rebuildRhythmGrid();
        });
        m_rhythmGrid = CCMenu::create();
        m_rhythmGrid->setPosition({ 0, 0 });
        t.node->addChild(m_rhythmGrid);

        addDescription(t,
            "Button above a beat = its snap. Squares: click = off / purple (quiet) / pink (loud). - = drop that beat, + = split.\n"
            "Loops = repeats (all = until next point / BPM change). Normal = regular grid. NOTE: inside a rhythm the 1/N guidelines\n"
            "button (1/2, 1/4, 1/8...) and metronome Ticks do nothing - the guidelines and clicks follow the pattern there.", 40);
    }

    // ===== 3. Analysis =====
    {
        auto& t = addTab("Analysis", 240);
        float y = 86;
        m_minBpmInput = addInput(t, "Min BPM", 70, y, 50, "70", CommonFilter::Float);
        m_minBpmInput->setString(fmt::format("{:.0f}", savedDouble("min-bpm", 70)));
        m_minBpmInput->setCallback([](std::string const& s) {
            if (auto v = numFromString<double>(s); v && *v >= 30) saveSetting("min-bpm", *v);
        });
        m_maxBpmInput = addInput(t, "Max BPM", 160, y, 50, "230", CommonFilter::Float);
        m_maxBpmInput->setString(fmt::format("{:.0f}", savedDouble("max-bpm", 230)));
        m_maxBpmInput->setCallback([](std::string const& s) {
            if (auto v = numFromString<double>(s); v && *v <= 400) saveSetting("max-bpm", *v);
        });
        addButton(t.menu, "Tempo changes", { 250, y }, [this] {
            bool on = !savedBool("detect-changes", true);
            saveSetting("detect-changes", on);
            setToggle(m_changesLabel, "Tempo changes", on);
        }, "GJ_button_04.png", &m_changesLabel);
        addButton(t.menu, "Analyze!", { 345, y }, [this] {
            auto& s = Session::get();
            if (!s.envelope) {
                Notification::create("Load an audio file first", NotificationIcon::Error)->show();
                return;
            }
            AnalyzerSettings st;
            st.minBpm = savedDouble("min-bpm", 70);
            st.maxBpm = std::max(st.minBpm + 10, savedDouble("max-bpm", 230));
            st.preferredBpm = std::clamp(150.0, st.minBpm, st.maxBpm);
            st.detectChanges = savedBool("detect-changes", true);
            st.meter = m_selected >= 0 ? s.map.points[m_selected].meter : 4;
            s.analyze(st);
        }, "GJ_button_02.png");
        addButton(t.menu, "Tap", { 405, y }, [this] { tap(); }, "GJ_button_02.png");
        addDescription(t,
            "Analyze! - listens to the song and finds the BPM, tempo changes and where the beat starts (green mark\n"
            "   on the waveform). It REPLACES the current timing points - fine-tune them afterwards if needed.\n"
            "Min / Max BPM - search range. If the result is half or double the real tempo, narrow the range\n"
            "   (or use x2 / /2 in Timing Points).\n"
            "Tempo changes ON - adds a timing point wherever the tempo drifts (live drummers, old recordings).\n"
            "Tempo changes OFF - one constant BPM for the whole song (most electronic / studio tracks).\n"
            "Tap (T) - tap along with the music, after 4+ taps the selected point gets the tapped BPM.\n"
            "The orange line at the bottom of the waveform shows detected hits - beat lines should sit on its peaks.", 70);
    }

    // ===== 4. Files =====
    {
        auto& t = addTab("Files", 318);
        float y = 88;
        addButton(t.menu, "Import .osu", { 90, y }, [this] { pickOsu(); }, "GJ_button_01.png");
        addButton(t.menu, "Export .osu", { 90, 68 }, [this] { exportOsu(); });
        addButton(t.menu, "Export level", { 270, 68 }, [] {
            auto& s = Session::get();
            auto name = Session::levelName();
            if (name.empty()) name = "level";
            if (auto p = nativePick(true, L"Geometry Dash Timing Analyzer level\0*.json\0", std::filesystem::path(name + ".gdta.json"))) {
                bool ok = s.exportLevel(*p);
                Notification::create(ok ? "Level settings exported" : "Save failed",
                    ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
            }
        });
        addButton(t.menu, "Import level", { 270, y }, [this] {
            auto p = nativePick(false, L"Geometry Dash Timing Analyzer level\0*.json\0All files\0*.*\0");
            if (!p) return;
            std::string err;
            if (!Session::get().importLevel(*p, &err)) {
                Notification::create(err, NotificationIcon::Error)->show();
                return;
            }
            m_selected = Session::get().map.empty() ? -1 : 0;
            m_lastRecord = -10;
            recordHistory();
            if (m_minBpmInput) m_minBpmInput->setString(fmt::format("{:.0f}", savedDouble("min-bpm", 70)));
            if (m_maxBpmInput) m_maxBpmInput->setString(fmt::format("{:.0f}", savedDouble("max-bpm", 230)));
            setToggle(m_changesLabel, "Tempo changes", savedBool("detect-changes", true));
            m_halfLabel->setString(fmt::format("Lines: 1/{}", Session::get().guideDivisor()).c_str());
            setToggle(m_metroLabel, "Metronome", Session::get().editorMetronome());
            refreshPanel();
            Notification::create("Level settings imported", NotificationIcon::Success)->show();
        });
        addButton(t.menu, "Unload everything", { 455, 68 }, [this] {
            createQuickPopup("Unload everything",
                "Remove the <cy>song</c>, the waveform and <cr>all timing points</c> of this level?\n"
                "The saved timing is <cr>deleted</c> too.", "Cancel", "Remove", [this](auto, bool yes) {
                    if (!yes) return;
                    stopPlayback();
                    m_selected = -1;
                    m_cursor = 0;
                    m_viewStart = -500;
                    m_taps.clear();
                    Session::get().unloadEverything();
                    Notification::create("Everything removed", NotificationIcon::Success)->show();
                });
        }, "GJ_button_06.png");
        addDescription(t, "OSU! TIMING\nImport .osu - copies the timing points\nfrom an osu! beatmap (.osu file).\nExport .osu - saves your timing as an\nosu! [TimingPoints] section.", 52, 22);
        addDescription(t, "THIS LEVEL (.json file)\nExport level - saves this level's timing,\nsettings and song start to one file.\nImport level - loads such a file back\n(e.g. on another PC or as a backup).", 52, 202);
        addDescription(t, "START OVER\nUnload everything - removes the\nsong, the waveform and every\ntiming point (also the saved ones).", 52, 382);
    }

    // ===== 5. Level =====
    {
        auto& t = addTab("Level", 394);
        float y = 88;
        float y2 = 68;
        addButton(t.menu, "Load audio...", { 75, y }, [this] { pickAudio(); }, "GJ_button_01.png");
        addButton(t.menu, "Load level song", { 75, y2 }, [this] {
            auto p = Session::currentLevelSongPath();
            if (p.empty()) Notification::create("This level has no downloaded song", NotificationIcon::Error)->show();
            else handleDroppedFile(p);
        }, "GJ_button_01.png");
        addButton(t.menu, "Use as level song", { 205, y }, [this] {
            auto& s = Session::get();
            int id = s.useAudioInLevel();
            if (id <= 0) {
                Notification::create(s.audioPath.empty() ? "Load an audio file first" : "Open this from a level's song selection",
                    NotificationIcon::Error)->show();
                return;
            }
            Notification::create(fmt::format("Level now plays your file (song ID {})", id), NotificationIcon::Success)->show();
        }, "GJ_button_01.png");
        addButton(t.menu, "Restore song", { 205, y2 }, [] {
            bool ok = Session::get().restoreLevelSong();
            Notification::create(ok ? "Original song restored" : "This level has no local song",
                ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
        }, "GJ_button_06.png");
        addButton(t.menu, "Song starts here", { 430, y }, [this] {
            double ms = m_playing ? playPosition() : m_cursor;
            if (!Session::get().setSongStartOffset(ms)) {
                Notification::create("Open this from a level (editor or song selection)", NotificationIcon::Error)->show();
                return;
            }
            Notification::create(fmt::format("Song start offset = {:.3f} s", std::max(0.0, ms) / 1000.0),
                NotificationIcon::Success)->show();
        }, "GJ_button_02.png");
        addButton(t.menu, "Reset song start", { 430, y2 }, [] {
            if (!Session::get().setSongStartOffset(0)) {
                Notification::create("Open this from a level (editor or song selection)", NotificationIcon::Error)->show();
                return;
            }
            Notification::create("Song starts from the beginning again (offset 0 s)", NotificationIcon::Success)->show();
        }, "GJ_button_06.png");
        // where the level's song currently starts (Start Offset)
        m_offsetLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_offsetLabel->setScale(.5f);
        m_offsetLabel->setPosition({ 430, 103 });
        m_offsetLabel->setColor({ 255, 220, 60 });
        t.node->addChild(m_offsetLabel);
        addDescription(t,
            "Load audio... - mp3 / ogg / wav / flac (or drag it onto the game).\n"
            "   The level starts using it right away.\n"
            "Load level song - loads the level's song (also a Jukebox NONG).\n"
            "Use as level song - the level plays the loaded file (only on your PC).\n"
            "Restore song - puts the original song back.\n"
            "Song starts here - Start Offset = cursor (shown above).  Reset - back to 0.", 54);
    }

    // ===== 6. Guidelines =====
    {
        auto& t = addTab("Guidelines", 475);
        float y2 = 88;
        addButton(t.menu, "Create guidelines", { 70, y2 }, [this] {
            int n = Session::get().applyGuidelines();
            Notification::create(n ? fmt::format("Added {} guidelines", n) : std::string("No timing / not in the editor"),
                n ? NotificationIcon::Success : NotificationIcon::Error)->show();
        }, "GJ_button_02.png");
        addButton(t.menu, "Lines: 1/1", { 175, y2 }, [this] {
            auto& s = Session::get();
            static int const divs[] = { 1, 2, 3, 4, 6, 8 };
            int cur = s.guideDivisor(), next = divs[0];
            for (int d : divs)
                if (d > cur) { next = d; break; }
            s.setGuideDivisor(next);
            m_halfLabel->setString(fmt::format("Lines: 1/{}", next).c_str());
        }, "GJ_button_04.png", &m_halfLabel);
        addButton(t.menu, "Remove guidelines", { 285, y2 }, [] {
            bool ok = Session::get().clearGuidelines();
            Notification::create(ok ? "Guidelines removed" : "Not in the editor",
                ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
        }, "GJ_button_06.png");
        addDescription(t,
            "Guidelines are the coloured lines in the editor that show where the beats of the song are.\n"
            "Create guidelines - draws them: green = 1st beat of a bar, yellow = beat, orange = 1/2, 1/3, 1/4...\n"
            "   They start where the song starts (Start Offset). Old guidelines are replaced.\n"
            "Lines: 1/N - snap of the level (also the 1/N button in the editor). Inside a rhythm the lines follow the pattern.", 66);
    }

    setToggle(m_metroLabel, "Metronome", Session::get().editorMetronome());
    setToggle(m_allLabel, "All points", m_shiftAll);
    setToggle(m_changesLabel, "Tempo changes", savedBool("detect-changes", true));
    m_halfLabel->setString(fmt::format("Lines: 1/{}", Session::get().guideDivisor()).c_str());

    auto levelName = Session::levelName();
    auto help = CCLabelBMFont::create(levelName.empty() ? "Drop an audio or .osu file on the window to load it"
        : fmt::format("Level: {}  -  drop an audio or .osu file on the window", levelName).c_str(), "chatFont.fnt");
    help->setScale(.36f);
    help->setOpacity(150);
    help->setAnchorPoint({ 1, .5f });
    help->setPosition({ POP_W - 12, POP_H - 27 });
    m_mainLayer->addChild(help);

    layoutTabs();
    switchTab(Mod::get()->getSavedValue<int>("last-tab", 0));
    handleTouchPriority(this);

    auto& s = Session::get();
    if (!s.map.empty()) {
        m_selected = 0;
        m_cursor = s.map.points[0].time;
    }
    if (!s.map.rhythm.empty()) m_rhythmSel = 0;
    resetHistory();
    refreshPanel();
    redraw();
    this->scheduleUpdate();
    return true;
}

void TimingEditor::onClose(CCObject* sender) {
    // continue in the editor exactly where the waveform is
    if (m_fromEditor && LevelEditorLayer::get()) {
        bool wasPlaying = m_playing;
        double pos = m_playing ? playPosition() : m_cursor;
        stopPlayback();
        Session::editorGoTo(pos, wasPlaying);
    }
    stopPlayback();
    Session::get().save();
    s_current = nullptr;
    Popup::onClose(sender);
}

double TimingEditor::songLength() const {
    auto& s = Session::get();
    if (s.audio) return s.audio->lengthMs;
    return s.map.empty() ? 60000.0 : s.map.points.back().time + 30000.0;
}

double TimingEditor::timeAtX(float x) const { return m_viewStart + (x - WX) * m_msPerPx; }
float TimingEditor::xAtTime(double t) const { return float(WX + (t - m_viewStart) / m_msPerPx); }

bool TimingEditor::anyInputFocused() const {
    for (auto const& tab : m_tabs)
        for (auto in : tab.inputs)
            if (in->getInputNode() && in->getInputNode()->m_selected) return true;
    return false;
}

void TimingEditor::onSessionChanged() {
    auto& s = Session::get();
    if (m_selected >= (int)s.map.points.size()) m_selected = (int)s.map.points.size() - 1;
    if (m_selected < 0 && !s.map.empty()) m_selected = 0;
    if (s.firstBeatMs >= 0 && m_cursor == 0) m_cursor = s.firstBeatMs;
    if (m_soundPath != s.audioPath) stopPlayback();
    // analysis results can be undone; a newly loaded song / level starts a new history
    recordHistory();
    refreshPanel();
}

// ---------------- rhythm patterns ----------------

int TimingEditor::rhythmMeter(int idx) const {
    auto& s = Session::get();
    if (idx < 0 || idx >= (int)s.map.rhythm.size() || s.map.empty()) return 4;
    return std::max(1, s.map.points[std::max(0, s.map.indexAt(s.map.rhythm[idx].time + 5))].meter);
}

void TimingEditor::selectRhythm(int idx) {
    auto& s = Session::get();
    if (idx < 0 || idx >= (int)s.map.rhythm.size()) return;
    m_rhythmSel = idx;
    m_rhythmPage = 0;
    double t = s.map.rhythm[idx].time;
    if (xAtTime(t) < WX + 20 || xAtTime(t) > WX + WW - 20) m_viewStart = t - WW * m_msPerPx * 0.25;
    refreshPanel();
}

void TimingEditor::addRhythmPoint(bool normal) {
    auto& s = Session::get();
    if (s.map.empty()) {
        Notification::create("Add a timing point (BPM) first", NotificationIcon::Error)->show();
        return;
    }
    double t = s.map.snap(m_cursor, 1);
    int found = -1;
    for (int i = 0; i < (int)s.map.rhythm.size(); i++)
        if (std::abs(s.map.rhythm[i].time - t) < 1) found = i;
    if (found < 0) {
        RhythmPoint r;
        r.time = t;
        s.map.rhythm.push_back(r);
        found = (int)s.map.rhythm.size() - 1;
    }
    auto& r = s.map.rhythm[found];
    r.beats.clear();
    r.bars = 1;
    if (!normal) {
        m_rhythmSel = found;
        r.beats.assign(rhythmMeter(found), RhythmBeat{});
    }
    m_rhythmSel = found;
    m_rhythmPage = 0;
    changed();
    switchTab(m_rhythmTab);
}

void TimingEditor::deleteRhythmPoint() {
    auto& s = Session::get();
    if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
    s.map.rhythm.erase(s.map.rhythm.begin() + m_rhythmSel);
    if (m_rhythmSel >= (int)s.map.rhythm.size()) m_rhythmSel = (int)s.map.rhythm.size() - 1;
    changed();
}

void TimingEditor::setRhythmBars(int bars) {
    auto& s = Session::get();
    if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
    auto& r = s.map.rhythm[m_rhythmSel];
    if (r.beats.empty()) return;
    int meter = rhythmMeter(m_rhythmSel);
    // steps grouped by bar (a step never crosses a bar line)
    std::vector<std::vector<RhythmBeat>> old;
    int beat = 0;
    for (auto const& b : r.beats) {
        size_t bar = beat / meter;
        if (old.size() <= bar) old.resize(bar + 1);
        old[bar].push_back(b);
        beat += std::max(1, b.span);
    }
    // longer pattern = the existing bars repeated, shorter = cut
    r.beats.clear();
    for (int i = 0; i < bars; i++) r.beats.insert(r.beats.end(), old[i % old.size()].begin(), old[i % old.size()].end());
    r.bars = bars;
    if (m_rhythmPage >= bars) m_rhythmPage = bars - 1;
    changed();
}

// Pattern from the song: for every beat of the pattern the smallest snap that fits the detected hits
void TimingEditor::rhythmFromAudio() {
    auto& s = Session::get();
    if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size() || s.map.empty()) return;
    auto& r = s.map.rhythm[m_rhythmSel];
    if (r.beats.empty()) return;
    if (!s.envelope) {
        Notification::create("Load the song first (the waveform is needed)", NotificationIcon::Error)->show();
        return;
    }
    auto const& env = *s.envelope;
    double bl = s.map.points[std::max(0, s.map.indexAt(r.time + 5))].beatLength;
    // one step per beat again, each gets its own snap
    r.beats.assign(r.bars * rhythmMeter(m_rhythmSel), RhythmBeat{});
    double from = r.time - bl / 12, to = r.time + r.beats.size() * bl;
    constexpr double STEP = 2.0;
    float maxV = 0;
    for (double t = from; t < to; t += STEP) maxV = std::max(maxV, env.at(t));
    float thr = std::max(.08f, maxV * .3f);
    // local maxima above the threshold, closer than 25 ms = one hit
    std::vector<std::pair<double, float>> peaks;
    for (double t = from + STEP; t < to; t += STEP) {
        float v = env.at(t);
        if (v < thr || v < env.at(t - 4) || v < env.at(t + 4)) continue;
        if (!peaks.empty() && t - peaks.back().first < 25) {
            if (v > peaks.back().second) peaks.back() = { t, v };
            continue;
        }
        peaks.push_back({ t, v });
    }
    static int const divs[] = { 1, 2, 3, 4, 6, 8 };
    int hitsTotal = 0;
    for (size_t k = 0; k < r.beats.size(); k++) {
        double start = r.time + k * bl;
        std::vector<double> rel; // hit positions inside this beat, in beats
        for (auto const& [t, v] : peaks) {
            double x = (t - start) / bl;
            if (x >= -1 / 12.0 && x < 1 - 1 / 12.0) rel.push_back(x);
        }
        RhythmBeat b{ 1, 0, 1, 1 };
        double bestErr = 1e18;
        for (int d : divs) {
            double tol = std::min(bl / d / 4, 25.0);
            double err = 0;
            bool ok = true;
            unsigned hits = 0;
            for (double x : rel) {
                int n = (int)std::lround(x * d);
                double e = std::abs(x * d - n) * bl / d;
                err += e;
                if (e > tol) ok = false;
                if (n >= 0 && n < d) hits |= 1u << n;
            }
            // smallest snap that explains every hit wins; otherwise the one with the least error
            if (ok) { b = { d, hits, 1, defaultAccents(d, 1) }; bestErr = -1; break; }
            if (err < bestErr) { bestErr = err; b = { d, hits, 1, defaultAccents(d, 1) }; }
        }
        r.beats[k] = b;
        hitsTotal += (int)rel.size();
    }
    changed();
    Notification::create(fmt::format("Pattern from {} detected hit(s)", hitsTotal), NotificationIcon::Success)->show();
}

// Beat columns of the shown bar: snap button on top, one square per part below (rebuilt from update())
void TimingEditor::rebuildRhythmGrid() {
    if (!m_rhythmGrid) return;
    auto& s = Session::get();
    bool valid = m_rhythmSel >= 0 && m_rhythmSel < (int)s.map.rhythm.size() && !s.map.rhythm[m_rhythmSel].beats.empty();
    int meter = valid ? rhythmMeter(m_rhythmSel) : 4;
    int pages = valid ? std::max(1, (s.map.rhythm[m_rhythmSel].patternBeats() + meter - 1) / meter) : 1;
    m_rhythmPage = std::clamp(m_rhythmPage, 0, pages - 1);
    std::string key = "-";
    if (valid) {
        TimingMap one;
        one.rhythm = { s.map.rhythm[m_rhythmSel] };
        key = one.serialize() + fmt::format("#{}#{}#{}", m_rhythmSel, m_rhythmPage, meter);
    }
    if (key == m_rhythmGridKey) return;
    m_rhythmGridKey = key;
    m_rhythmGrid->removeAllChildren();
    m_rhythmPageLabel->setString(valid ? fmt::format("Bar {}/{}", m_rhythmPage + 1, pages).c_str() : "");
    if (!valid) return;

    auto const& beats = s.map.rhythm[m_rhythmSel].beats;
    float x0 = 100, x1 = POP_W - 16;
    float colW = (x1 - x0) / meter;
    int beat = 0; // beat where step bi starts
    for (int bi = 0; bi < (int)beats.size(); beat += std::max(1, beats[bi].span), bi++) {
        auto const& b = beats[bi];
        int span = std::max(1, b.span);
        int inBar = beat - m_rhythmPage * meter;
        if (inBar < 0 || inBar >= meter) continue;
        // a step is as wide as the beats it covers
        float left = x0 + colW * inBar, w = colW * span - 10;
        float cx = left + colW * span / 2;
        addButton(m_rhythmGrid, fmt::format("1/{}", b.divisor).c_str(), { cx, 72 }, [this, bi] {
            auto& s = Session::get();
            if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
            auto& step = s.map.rhythm[m_rhythmSel].beats[bi];
            static int const divs[] = { 1, 2, 3, 4, 6, 8 };
            int i = 0;
            while (i < 6 && divs[i] != step.divisor) i++;
            int nd = divs[(i + 1) % 6];
            // a new snap starts with every part playing (click squares to remove hits)
            step.divisor = nd;
            step.hits = (1u << nd) - 1;
            step.accents = defaultAccents(nd, std::max(1, step.span));
            changed();
        }, "GJ_button_04.png");
        // "-" = remove this beat, the step before gets longer (not across a bar line)
        if (inBar > 0)
            addButton(m_rhythmGrid, "-", { cx - 30, 72 }, [this, bi] {
                auto& s = Session::get();
                if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size() || bi < 1) return;
                auto& steps = s.map.rhythm[m_rhythmSel].beats;
                steps[bi - 1].span = std::max(1, steps[bi - 1].span) + std::max(1, steps[bi].span);
                steps.erase(steps.begin() + bi);
                changed();
            }, "GJ_button_06.png");
        // "+" = split the last beat off again
        if (span > 1)
            addButton(m_rhythmGrid, "+", { cx + 30, 72 }, [this, bi] {
                auto& s = Session::get();
                if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
                auto& steps = s.map.rhythm[m_rhythmSel].beats;
                if (steps[bi].span < 2) return;
                steps[bi].span--;
                steps.insert(steps.begin() + bi + 1, RhythmBeat{});
                changed();
            }, "GJ_button_01.png");
        float cw = w / b.divisor;
        for (int j = 0; j < b.divisor; j++) {
            bool on = b.hits & (1u << j), accent = b.accents & (1u << j);
            // pink = accented hit (louder), purple = quiet hit, gray = no hit
            ccColor3B col = on ? (accent ? ccColor3B{ 255, 150, 235 } : ccColor3B{ 170, 80, 255 }) : ccColor3B{ 70, 70, 85 };
            // drawn 4x larger and scaled down so the rounded corners stay small
            auto cell = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
            cell->setContentSize({ std::max(8.f, (cw - 2) * 4), 14 * 4 });
            cell->setScale(.25f);
            cell->setColor(col);
            auto item = CCMenuItemExt::createSpriteExtra(cell, [this, bi, j](auto) {
                auto& s = Session::get();
                if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
                // click: off -> purple -> pink -> off
                auto& step = s.map.rhythm[m_rhythmSel].beats[bi];
                unsigned bit = 1u << j;
                if (!(step.hits & bit)) { step.hits |= bit; step.accents &= ~bit; }
                else if (!(step.accents & bit)) step.accents |= bit;
                else { step.hits &= ~bit; step.accents &= ~bit; }
                changed();
            });
            item->setPosition({ left + 5 + cw * (j + .5f), 52 });
            m_rhythmGrid->addChild(item);
        }
    }
}

// ---------------- undo / redo ----------------

static std::string historyKeyNow() {
    auto& s = Session::get();
    return utils::string::pathToString(s.audioPath) + "|" + Session::levelKey();
}

void TimingEditor::resetHistory() {
    m_undo.clear();
    m_redo.clear();
    m_lastMap = Session::get().map;
    m_historyKey = historyKeyNow();
}

void TimingEditor::recordHistory() {
    auto& s = Session::get();
    // another song / level -> its own history
    if (m_historyKey != historyKeyNow()) return resetHistory();
    if (s.map.serialize() == m_lastMap.serialize()) return;
    // typing a value or clicking +1 several times in a row is one step (adding / deleting never merges)
    bool merge = m_clock - m_lastRecord < 0.6 && !m_undo.empty() &&
                 m_lastMap.points.size() == s.map.points.size() && m_undo.back().points.size() == s.map.points.size() &&
                 m_lastMap.rhythm.size() == s.map.rhythm.size() && m_undo.back().rhythm.size() == s.map.rhythm.size();
    if (!merge) {
        m_undo.push_back(m_lastMap);
        if (m_undo.size() > 200) m_undo.erase(m_undo.begin());
    }
    m_redo.clear();
    m_lastMap = s.map;
    m_lastRecord = m_clock;
}

void TimingEditor::undo() {
    auto& s = Session::get();
    recordHistory();
    if (m_undo.empty()) {
        Notification::create("Nothing to undo", NotificationIcon::Info)->show();
        return;
    }
    m_redo.push_back(s.map);
    s.map = m_undo.back();
    m_undo.pop_back();
    m_lastMap = s.map;
    m_lastRecord = -10;
    if (m_selected >= (int)s.map.points.size()) m_selected = (int)s.map.points.size() - 1;
    if (m_selected < 0 && !s.map.empty()) m_selected = 0;
    s.save();
    refreshPanel();
}

void TimingEditor::redo() {
    auto& s = Session::get();
    recordHistory();
    if (m_redo.empty()) {
        Notification::create("Nothing to redo", NotificationIcon::Info)->show();
        return;
    }
    m_undo.push_back(s.map);
    s.map = m_redo.back();
    m_redo.pop_back();
    m_lastMap = s.map;
    m_lastRecord = -10;
    if (m_selected >= (int)s.map.points.size()) m_selected = (int)s.map.points.size() - 1;
    if (m_selected < 0 && !s.map.empty()) m_selected = 0;
    s.save();
    refreshPanel();
}

void TimingEditor::changed() {
    auto& s = Session::get();
    double selTime = m_selected >= 0 && m_selected < (int)s.map.points.size() ? s.map.points[m_selected].time : 0;
    bool hasRhythmSel = m_rhythmSel >= 0 && m_rhythmSel < (int)s.map.rhythm.size();
    double rhythmTime = hasRhythmSel ? s.map.rhythm[m_rhythmSel].time : 0;
    s.map.sort();
    if (hasRhythmSel)
        for (int i = 0; i < (int)s.map.rhythm.size(); i++)
            if (s.map.rhythm[i].time == rhythmTime) { m_rhythmSel = i; break; }
    recordHistory();
    if (m_selected >= 0) {
        for (int i = 0; i < (int)s.map.points.size(); i++)
            if (s.map.points[i].time == selTime) { m_selected = i; break; }
    }
    s.save();
    refreshPanel();
}

void TimingEditor::selectPoint(int idx) {
    auto& s = Session::get();
    if (idx < 0 || idx >= (int)s.map.points.size()) return;
    m_selected = idx;
    auto t = s.map.points[idx].time;
    if (xAtTime(t) < WX + 20 || xAtTime(t) > WX + WW - 20) m_viewStart = t - WW * m_msPerPx * 0.25;
    refreshPanel();
}

void TimingEditor::onMusicVolume(CCObject*) {
    if (!m_musicSlider) return;
    float v = std::clamp(m_musicSlider->getValue(), 0.f, 1.f);
    // the game's own music volume, so this window and the editor always sound the same
    FMODAudioEngine::get()->setBackgroundMusicVolume(v);
    GameManager::get()->m_bgVolume = v;
    if (m_channel) m_channel->setVolume(v);
    m_musicVolLabel->setString(fmt::format("Music: {}%", (int)std::round(v * 100)).c_str());
}

void TimingEditor::onMetronomeVolume(CCObject*) {
    if (!m_metroSlider) return;
    int v = (int)std::round(std::clamp(m_metroSlider->getValue(), 0.f, 1.f) * 100);
    Mod::get()->setSettingValue<int64_t>("metronome-volume", v);
    m_metroVolLabel->setString(fmt::format("Metronome: {}%", v).c_str());
}

void TimingEditor::refreshPanel() {
    auto& s = Session::get();
    m_fileLabel->setString(s.audioPath.empty()
        ? "No audio loaded"
        : utils::string::pathToString(s.audioPath.filename()).c_str());
    limitNodeWidth(m_fileLabel, 230, .5f, .2f);

    m_updatingInputs = true;
    if (m_selected >= 0 && m_selected < (int)s.map.points.size()) {
        auto const& p = s.map.points[m_selected];
        m_pointLabel->setString(fmt::format("Point {}/{}", m_selected + 1, s.map.points.size()).c_str());
        if (!m_timeInput->getInputNode()->m_selected) m_timeInput->setString(fmt::format("{:.0f}", p.time));
        if (!m_bpmInput->getInputNode()->m_selected) m_bpmInput->setString(fmt::format("{:.3f}", p.bpm()));
        if (!m_meterInput->getInputNode()->m_selected) m_meterInput->setString(fmt::format("{}", p.meter));
    } else {
        m_pointLabel->setString("No points");
        m_timeInput->setString("");
        m_bpmInput->setString("");
        m_meterInput->setString("");
    }
    m_updatingInputs = false;

    // rhythm tab
    int rn = (int)s.map.rhythm.size();
    if (m_rhythmSel >= rn) m_rhythmSel = rn - 1;
    if (m_rhythmSel < 0 && rn > 0) m_rhythmSel = 0;
    if (m_rhythmLabel) {
        if (m_rhythmSel >= 0) {
            auto const& r = s.map.rhythm[m_rhythmSel];
            m_rhythmLabel->setString(fmt::format("Rhythm {}/{}", m_rhythmSel + 1, rn).c_str());
            m_rhythmLenLabel->setString(r.beats.empty() ? "Normal point"
                : fmt::format("Length: {} bar{}", r.bars, r.bars > 1 ? "s" : "").c_str());
            m_rhythmLoopsLabel->setString(r.beats.empty() || r.loops == 0 ? "Loops: all"
                : fmt::format("Loops: {}", r.loops).c_str());
        } else {
            m_rhythmLabel->setString("No rhythm");
            m_rhythmLenLabel->setString("Length: -");
            m_rhythmLoopsLabel->setString("Loops: -");
        }
    }
    // the grid itself is rebuilt from update() (its buttons call this, they must not delete themselves)
}

void TimingEditor::zoom(double factor, double anchorMs) {
    double ax = (anchorMs - m_viewStart) / m_msPerPx;
    m_msPerPx = std::clamp(m_msPerPx * factor, 0.25, songLength() / WW * 1.2);
    m_viewStart = anchorMs - ax * m_msPerPx;
}

bool TimingEditor::onScroll(float y, float) {
    auto kb = CCKeyboardDispatcher::get();
    double mouseT = m_viewStart + WW * m_msPerPx / 2;
    auto mp = m_mainLayer->convertToNodeSpace(getMousePos());
    if (mp.x >= WX && mp.x <= WX + WW) mouseT = timeAtX(mp.x);
    if (kb->getControlKeyPressed()) {
        zoom(y > 0 ? 1.2 : 1 / 1.2, mouseT);
    } else {
        // like osu!: the wheel moves by one grid line
        auto& s = Session::get();
        int dir = y > 0 ? 1 : -1;
        if (kb->getShiftKeyPressed() || s.map.empty()) {
            m_viewStart += dir * WW * m_msPerPx * 0.15;
        } else {
            double t = m_playing ? playPosition() : m_cursor;
            int i = std::max(0, s.map.indexAt(t));
            double step = s.map.points[i].beatLength / m_divisor;
            double next = s.map.snap(t + dir * step * 0.999, m_divisor);
            if (std::abs(next - t) < 0.5) next = s.map.snap(t + dir * step * 1.5, m_divisor);
            seek(next);
            if (xAtTime(next) < WX + 40 || xAtTime(next) > WX + WW - 40) m_viewStart = next - WW * m_msPerPx / 2;
        }
    }
    return true;
}

void TimingEditor::keyDown(enumKeyCodes key, double ts) {
    if (anyInputFocused()) return Popup::keyDown(key, ts);
    auto& s = Session::get();
    auto kb = CCKeyboardDispatcher::get();
    if (kb->getControlKeyPressed()) {
        if (key == KEY_Z) return kb->getShiftKeyPressed() ? redo() : undo();
        if (key == KEY_Y) return redo();
    }
    switch (key) {
        case KEY_Space: togglePlay(); return;
        case KEY_Left: onScroll(-1, 0); return;
        case KEY_Right: onScroll(1, 0); return;
        case KEY_Delete: m_tab == m_rhythmTab ? deleteRhythmPoint() : deletePoint(); return;
        case KEY_T: tap(); return;
        case KEY_A: addPointAtCursor(); return;
        case KEY_Up: if (m_selected > 0) selectPoint(m_selected - 1); return;
        case KEY_Down: if (m_selected + 1 < (int)s.map.points.size()) selectPoint(m_selected + 1); return;
        default: break;
    }
    Popup::keyDown(key, ts);
}

// ---------------- touch / mouse ----------------

bool TimingEditor::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    auto& s = Session::get();
    if (p.x >= OX && p.x <= OX + OW && p.y >= OY - 2 && p.y <= OY + OH + 2) {
        m_drag = Drag::Overview;
        ccTouchMoved(touch, event);
        return true;
    }
    if (p.x >= WX && p.x <= WX + WW && p.y >= WY && p.y <= WY + WH) {
        m_dragStartX = p.x;
        m_dragStartView = m_viewStart;
        m_drag = Drag::Pending;
        int best = -1;
        float bestDist = 6;
        for (int i = 0; i < (int)s.map.points.size(); i++) {
            float d = std::abs(xAtTime(s.map.points[i].time) - p.x);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best >= 0) {
            m_selected = best;
            m_drag = Drag::Marker;
            m_dragStartTime = s.map.points[best].time;
            refreshPanel();
        } else {
            // rhythm markers (purple)
            int bestR = -1;
            float bestDistR = 6;
            for (int i = 0; i < (int)s.map.rhythm.size(); i++) {
                float d = std::abs(xAtTime(s.map.rhythm[i].time) - p.x);
                if (d < bestDistR) { bestDistR = d; bestR = i; }
            }
            if (bestR >= 0) {
                if (bestR != m_rhythmSel) m_rhythmPage = 0;
                m_rhythmSel = bestR;
                m_drag = Drag::Rhythm;
                m_dragStartTime = s.map.rhythm[bestR].time;
                if (m_rhythmTab >= 0) switchTab(m_rhythmTab);
                refreshPanel();
            }
        }
        return true;
    }
    return Popup::ccTouchBegan(touch, event);
}

void TimingEditor::ccTouchMoved(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    auto& s = Session::get();
    switch (m_drag) {
        case Drag::Overview: {
            double t = (p.x - OX) / OW * songLength();
            m_viewStart = t - WW * m_msPerPx / 2;
            break;
        }
        case Drag::Pending:
            if (std::abs(p.x - m_dragStartX) > 6) m_drag = Drag::Scroll;
            break;
        case Drag::Scroll:
            m_viewStart = m_dragStartView - (p.x - m_dragStartX) * m_msPerPx;
            break;
        case Drag::Marker:
            if (m_selected >= 0 && m_selected < (int)s.map.points.size()) {
                double t = std::round(m_dragStartTime + (p.x - m_dragStartX) * m_msPerPx);
                if (CCKeyboardDispatcher::get()->getShiftKeyPressed() && s.envelope)
                    t = std::round(nearestOnset(*s.envelope, t, 8 * m_msPerPx));
                s.map.points[m_selected].time = t;
                refreshPanel();
            }
            break;
        case Drag::Rhythm:
            if (m_rhythmSel >= 0 && m_rhythmSel < (int)s.map.rhythm.size()) {
                double t = m_dragStartTime + (p.x - m_dragStartX) * m_msPerPx;
                // rhythm points sit on beats (Alt = free)
                if (!CCKeyboardDispatcher::get()->getAltKeyPressed() && !s.map.empty()) t = s.map.snap(t, 1);
                s.map.rhythm[m_rhythmSel].time = t;
                refreshPanel();
            }
            break;
        default: Popup::ccTouchMoved(touch, event);
    }
}

void TimingEditor::ccTouchEnded(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    if (m_drag == Drag::Pending) {
        double t = timeAtX(p.x);
        auto& s = Session::get();
        if (!CCKeyboardDispatcher::get()->getAltKeyPressed() && !s.map.empty()) t = s.map.snap(t, m_divisor);
        seek(t);
    } else if (m_drag == Drag::Marker) {
        if (std::abs(p.x - m_dragStartX) < 3) {
            // a click (no drag) on a timing point still moves the cursor there
            auto& s = Session::get();
            if (m_selected >= 0 && m_selected < (int)s.map.points.size())
                s.map.points[m_selected].time = m_dragStartTime;
            double t = timeAtX(p.x);
            if (!CCKeyboardDispatcher::get()->getAltKeyPressed() && !s.map.empty()) t = s.map.snap(t, m_divisor);
            seek(t);
        }
        changed();
    } else if (m_drag == Drag::Rhythm) {
        changed();
    } else if (m_drag == Drag::None) {
        Popup::ccTouchEnded(touch, event);
    }
    m_drag = Drag::None;
}

void TimingEditor::ccTouchCancelled(CCTouch* touch, CCEvent* event) {
    if (m_drag == Drag::Marker || m_drag == Drag::Rhythm) changed();
    if (m_drag == Drag::None) Popup::ccTouchCancelled(touch, event);
    m_drag = Drag::None;
}

// ---------------- playback ----------------

double TimingEditor::playPosition() const {
    if (!m_channel) return m_cursor;
    unsigned int pos = 0;
    if (m_channel->getPosition(&pos, FMOD_TIMEUNIT_MS) != FMOD_OK) return m_cursor;
    return pos;
}

void TimingEditor::stopPlayback() {
    if (m_channel) m_channel->stop();
    m_channel = nullptr;
    if (m_sound) m_sound->release();
    m_sound = nullptr;
    m_soundPath.clear();
    m_playing = false;
    if (m_playLabel) m_playLabel->setString("Play");
}

void TimingEditor::seek(double ms) {
    m_cursor = std::max(0.0, ms);
    if (m_channel) m_channel->setPosition((unsigned)m_cursor, FMOD_TIMEUNIT_MS);
    m_tracker.reset();
}

void TimingEditor::togglePlay() {
    auto& s = Session::get();
    if (s.audioPath.empty()) return;
    if (m_playing) {
        m_cursor = playPosition();
        if (m_channel) m_channel->setPaused(true);
        m_playing = false;
        if (m_playLabel) m_playLabel->setString("Play");
        return;
    }
    auto sys = FMODAudioEngine::get()->m_system;
    // a channel that finished or was stolen by FMOD is invalid - make a new one
    if (m_channel) {
        bool isPlaying = false;
        if (m_channel->isPlaying(&isPlaying) != FMOD_OK || !isPlaying) m_channel = nullptr;
    }
    if (!m_sound || m_soundPath != s.audioPath) {
        stopPlayback();
        auto u8 = utils::string::pathToString(s.audioPath);
        if (sys->createSound(u8.c_str(), FMOD_CREATESTREAM | FMOD_ACCURATETIME, nullptr, &m_sound) != FMOD_OK) {
            m_sound = nullptr;
            Notification::create("Cannot play this file", NotificationIcon::Error)->show();
            return;
        }
        m_soundPath = s.audioPath;
    }
    if (!m_channel) {
        sys->playSound(m_sound, nullptr, true, &m_channel);
        if (!m_channel) return;
        m_channel->setVolume(FMODAudioEngine::get()->m_musicVolume);
    }
    if (m_channel->setPosition((unsigned)std::max(0.0, m_cursor), FMOD_TIMEUNIT_MS) != FMOD_OK) {
        // stale channel - start a fresh one at the cursor
        sys->playSound(m_sound, nullptr, true, &m_channel);
        if (!m_channel) return;
        m_channel->setVolume(FMODAudioEngine::get()->m_musicVolume);
        m_channel->setPosition((unsigned)std::max(0.0, m_cursor), FMOD_TIMEUNIT_MS);
    }
    m_channel->setPaused(false);
    m_tracker.reset();
    m_playing = true;
    if (m_playLabel) m_playLabel->setString("Pause");
}

// ---------------- actions ----------------

void TimingEditor::handleDroppedFile(std::filesystem::path const& path) {
    auto ext = utils::string::toLower(utils::string::pathToString(path.extension()));
    auto& s = Session::get();
    if (ext == ".osu") {
        std::string err;
        if (s.importOsu(path, &err)) {
            m_selected = 0;
            m_lastRecord = -10;
            recordHistory();
            Notification::create(fmt::format("Imported {} timing point(s)", s.map.points.size()),
                NotificationIcon::Success)->show();
        } else {
            Notification::create(err, NotificationIcon::Error)->show();
        }
    } else {
        stopPlayback();
        s.loadAudio(path);
        // a new song is used by the open level right away (not when it is the level's own song)
        if (Mod::get()->getSettingValue<bool>("auto-use-song") && path != Session::currentLevelSongPath()) {
            if (int id = s.useAudioInLevel(); id > 0)
                Notification::create(fmt::format("Level now plays this song (song ID {})", id), NotificationIcon::Success)->show();
        }
        m_selected = s.map.empty() ? -1 : 0;
        m_cursor = 0;
        m_viewStart = -500;
    }
    refreshPanel();
}

void TimingEditor::pickAudio() {
    if (auto p = nativePick(false, L"Audio\0*.mp3;*.ogg;*.wav;*.flac;*.m4a\0All files\0*.*\0")) handleDroppedFile(*p);
}

void TimingEditor::pickOsu() {
    if (auto p = nativePick(false, L"osu! beatmap\0*.osu\0")) handleDroppedFile(*p);
}

void TimingEditor::exportOsu() {
    auto& s = Session::get();
    if (s.map.empty()) return;
    auto name = s.audioPath.empty() ? std::string("timing") : utils::string::pathToString(s.audioPath.stem());
    if (auto p = nativePick(true, L"osu! timing\0*.osu\0", std::filesystem::path(name + ".osu"))) {
        bool ok = s.exportOsu(*p);
        Notification::create(ok ? "Saved" : "Save failed", ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
    }
}

void TimingEditor::addPointAtCursor() {
    auto& s = Session::get();
    TimingPoint p;
    p.time = std::round(m_playing ? playPosition() : m_cursor);
    if (!s.map.empty()) {
        auto const& prev = s.map.points[std::max(0, s.map.indexAt(p.time))];
        p.beatLength = prev.beatLength;
        p.meter = prev.meter;
    } else {
        p.setBpm(120);
    }
    for (auto const& q : s.map.points) if (std::abs(q.time - p.time) < 1) return;
    s.map.points.push_back(p);
    m_selected = (int)s.map.points.size() - 1;
    changed();
}

void TimingEditor::deletePoint() {
    auto& s = Session::get();
    if (m_selected < 0 || m_selected >= (int)s.map.points.size()) return;
    s.map.points.erase(s.map.points.begin() + m_selected);
    m_selected = std::min(m_selected, (int)s.map.points.size() - 1);
    changed();
}

void TimingEditor::shiftPoint(double ms) {
    auto& s = Session::get();
    if (m_shiftAll) {
        for (auto& p : s.map.points) p.time += ms;
    } else if (m_selected >= 0 && m_selected < (int)s.map.points.size()) {
        s.map.points[m_selected].time += ms;
    }
    changed();
}

void TimingEditor::tap() {
    m_taps.push_back(m_clock * 1000.0);
    if (m_taps.size() >= 2 && m_taps.back() - m_taps[m_taps.size() - 2] > 2000) m_taps.erase(m_taps.begin(), m_taps.end() - 1);
    if (m_taps.size() > 16) m_taps.erase(m_taps.begin());
    if (m_taps.size() < 4) {
        m_statusLabel->setString(fmt::format("Tap: {}...", m_taps.size()).c_str());
        return;
    }
    double bpm = 60000.0 * (m_taps.size() - 1) / (m_taps.back() - m_taps.front());
    m_statusLabel->setString(fmt::format("Tap: {:.2f} BPM", bpm).c_str());
    auto& s = Session::get();
    if (s.map.empty()) {
        TimingPoint p;
        p.time = std::round(m_cursor);
        s.map.points.push_back(p);
        m_selected = 0;
    }
    if (m_selected >= 0) {
        s.map.points[m_selected].setBpm(std::round(bpm * 100) / 100);
        changed();
    }
}

// ---------------- loop / drawing ----------------

void TimingEditor::update(float dt) {
    m_clock += dt;
    auto& s = Session::get();
    if (m_offsetLabel) {
        auto ls = Session::levelSettings();
        m_offsetLabel->setString(ls ? fmt::format("Song starts at {:.3f} s", ls->m_songOffset).c_str()
                                    : "Song start: open from a level");
    }
    if (m_playing) {
        bool isPlaying = false;
        if (m_channel) m_channel->isPlaying(&isPlaying);
        if (!isPlaying) {
            stopPlayback();
        } else {
            double pos = playPosition();
            if (s.editorMetronome()) m_tracker.update(pos, s.map);
            m_cursor = pos;
            double rel = (pos - m_viewStart) / (WW * m_msPerPx);
            if (rel > 0.75 || rel < 0) m_viewStart = pos - WW * m_msPerPx * 0.25;
        }
    }
    if (s.busy) {
        m_statusLabel->setString(fmt::format("{}... {:.0f}%", s.busyText, s.progress.load() * 100).c_str());
    } else {
        std::string st = m_statusLabel->getString();
        if (st.find("%") != std::string::npos) m_statusLabel->setString("");
    }
    m_divLabel->setString(fmt::format("Grid 1/{}", m_divisor).c_str());

    // grid position of the cursor
    if (s.map.empty()) {
        m_gridLabel->setString("Cursor: no timing points yet");
        m_gridLabel->setColor({ 180, 180, 180 });
    } else {
        auto g = s.map.locate(m_cursor, 2.0);
        if (m_playing) {
            m_gridLabel->setString(fmt::format("Cursor: Bar {}  Beat {}", g.bar, g.beat).c_str());
            m_gridLabel->setColor({ 255, 255, 255 });
        } else {
            m_gridLabel->setString(("Cursor: " + g.describe()).c_str());
            m_gridLabel->setColor(g.den == 0 ? ccColor3B{ 255, 120, 120 } : ccColor3B{ 140, 255, 140 });
        }
    }
    rebuildRhythmGrid();
    redraw();
}

void TimingEditor::redraw() {
    auto& s = Session::get();
    m_draw->clear();
    for (auto l : m_markerLabels) l->setVisible(false);

    // tab panel background
    m_draw->drawRect(CCRect(8, PY0, POP_W - 16, PY1 - PY0), rgba(0, 0, 0, 0.25f), 0.4f, rgba(0, 0, 0, 0.35f));

    double viewEnd = m_viewStart + WW * m_msPerPx;
    m_draw->drawRect(CCRect(WX, WY, WW, WH), rgba(0.05f, 0.06f, 0.1f, 0.9f), 0.5f, rgba(0, 0, 0, 1));
    float midY = WY + WH / 2;

    // beat grid (below the waveform)
    if (!s.map.empty()) {
        s.map.forEachTick(std::max(m_viewStart, -60000.0), viewEnd, m_divisor, [&](double t, TickKind k) {
            float x = xAtTime(t);
            if (x < WX || x > WX + WW) return;
            auto const& p = s.map.points[std::max(0, s.map.indexAt(t))];
            float h = k == TickKind::Downbeat ? WH : (k == TickKind::Beat ? WH * 0.7f : WH * 0.4f);
            float w = k == TickKind::Downbeat ? 0.8f : 0.4f;
            m_draw->drawSegment({ x, WY + WH - h }, { x, WY + WH }, w, tickColor(k, m_divisor, t, p));
        });
    }

    // waveform
    if (s.audio) {
        auto const& a = *s.audio;
        float amp = 0.001f;
        for (size_t i = 0; i < a.peakMax.size(); i += 16) amp = std::max({ amp, a.peakMax[i], -a.peakMin[i] });
        for (int px = 0; px < (int)WW; px++) {
            double t0 = m_viewStart + px * m_msPerPx;
            if (t0 + m_msPerPx < 0 || t0 > a.lengthMs) continue;
            auto [lo, hi] = a.range(t0, t0 + m_msPerPx);
            float y0 = midY + lo / amp * WH * 0.48f, y1 = midY + hi / amp * WH * 0.48f;
            if (y1 - y0 < 0.6f) { y0 -= 0.3f; y1 += 0.3f; }
            m_draw->drawSegment({ WX + px + 0.5f, y0 }, { WX + px + 0.5f, y1 }, 0.5f, rgba(0.35f, 0.75f, 1.f, 0.85f));
        }
        // onset envelope at the bottom (shows the hits)
        if (s.envelope && m_msPerPx < 25) {
            CCPoint prev{ WX, WY };
            for (int px = 0; px < (int)WW; px++) {
                double t0 = m_viewStart + px * m_msPerPx;
                float v = 0;
                for (double t = t0; t < t0 + m_msPerPx; t += 2.5) v = std::max(v, s.envelope->at(t));
                CCPoint cur{ WX + px, WY + 1 + v * WH * 0.22f };
                if (px) m_draw->drawSegment(prev, cur, 0.35f, rgba(1, 0.65f, 0.2f, 0.7f));
                prev = cur;
            }
        }
    } else if (s.busy) {
        m_draw->drawSegment({ WX + 10, midY }, { WX + 10 + (WW - 20) * s.progress.load(), midY }, 2,
            rgba(0.4f, 1, 0.4f, 0.8f));
    }

    // timing points
    int labelIdx = 0;
    for (int i = 0; i < (int)s.map.points.size(); i++) {
        auto const& p = s.map.points[i];
        float x = xAtTime(p.time);
        if (x < WX - 1 || x > WX + WW + 1) continue;
        bool sel = i == m_selected;
        auto col = sel ? rgba(1, 0.85f, 0.2f, 1) : rgba(1, 0.2f, 0.2f, 0.95f);
        m_draw->drawSegment({ x, WY }, { x, WY + WH }, sel ? 1.f : 0.7f, col);
        CCPoint tri[3] = { { x - 4, WY + WH }, { x + 4, WY + WH }, { x, WY + WH - 6 } };
        m_draw->drawPolygon(tri, 3, col, 0, col);
        if (labelIdx < (int)m_markerLabels.size() && x < WX + WW - 30) {
            auto l = m_markerLabels[labelIdx++];
            l->setString(fmt::format("{:.2f}", p.bpm()).c_str());
            l->setPosition({ x + 2, WY + WH - 1 });
            l->setColor(sel ? ccColor3B{ 255, 220, 60 } : ccColor3B{ 255, 110, 110 });
            l->setVisible(true);
        }
    }
    // rhythm patterns: purple hits along the bottom + purple markers (triangle at the bottom)
    if (!s.map.rhythm.empty()) {
        s.map.forEachRhythmHit(m_viewStart, viewEnd, [&](double t, TickKind k) {
            float x = xAtTime(t);
            if (x < WX || x > WX + WW) return;
            float h = k == TickKind::Sub ? 10.f : 16.f;
            m_draw->drawSegment({ x, WY }, { x, WY + h }, k == TickKind::Sub ? .5f : .8f, rgba(.85f, .4f, 1, .9f));
        });
        for (int i = 0; i < (int)s.map.rhythm.size(); i++) {
            auto const& r = s.map.rhythm[i];
            float x = xAtTime(r.time);
            if (x < WX - 1 || x > WX + WW + 1) continue;
            bool sel = i == m_rhythmSel;
            auto col = sel ? rgba(1, .75f, 1, 1) : rgba(.75f, .35f, 1, .95f);
            m_draw->drawSegment({ x, WY }, { x, WY + WH }, sel ? .9f : .6f, col);
            CCPoint tri[3] = { { x - 4, WY }, { x + 4, WY }, { x, WY + 6 } };
            m_draw->drawPolygon(tri, 3, col, 0, col);
            if (labelIdx < (int)m_markerLabels.size() && x < WX + WW - 30) {
                auto l = m_markerLabels[labelIdx++];
                l->setString(r.beats.empty() ? "Normal"
                    : fmt::format("Rhythm {} bar{}", r.bars, r.bars > 1 ? "s" : "").c_str());
                l->setPosition({ x + 2, WY + 30 });
                l->setColor(sel ? ccColor3B{ 255, 190, 255 } : ccColor3B{ 200, 130, 255 });
                l->setVisible(true);
            }
        }
    }
    // beat start found by the analysis
    if (s.firstBeatMs >= 0) {
        float x = xAtTime(s.firstBeatMs);
        if (x >= WX && x <= WX + WW) m_draw->drawSegment({ x, WY }, { x, WY + 10 }, 1, rgba(0.3f, 1, 0.3f, 1));
    }
    // cursor / play head
    float cx = xAtTime(m_cursor);
    if (cx >= WX && cx <= WX + WW)
        m_draw->drawSegment({ cx, WY - 2 }, { cx, WY + WH + 2 }, 0.8f, rgba(1, 1, 0.4f, 1));

    // whole-song overview
    double len = std::max(1.0, songLength());
    m_draw->drawRect(CCRect(OX, OY, OW, OH), rgba(0.1f, 0.1f, 0.15f, 0.9f), 0.4f, rgba(0, 0, 0, 1));
    if (s.audio) {
        auto const& a = *s.audio;
        float amp = 0.001f;
        for (size_t i = 0; i < a.peakMax.size(); i += 16) amp = std::max(amp, a.peakMax[i]);
        for (int px = 0; px < (int)OW; px += 2) {
            double t0 = px / OW * len;
            auto [lo, hi] = a.range(t0, t0 + len / OW * 2);
            float h = std::max(hi, -lo) / amp * OH * 0.5f;
            m_draw->drawSegment({ OX + px, OY + OH / 2 - h }, { OX + px, OY + OH / 2 + h }, 0.5f, rgba(0.3f, 0.6f, 0.9f, 0.7f));
        }
    }
    for (auto const& p : s.map.points) {
        float x = float(OX + p.time / len * OW);
        if (x >= OX && x <= OX + OW) m_draw->drawSegment({ x, OY }, { x, OY + OH }, 0.4f, rgba(1, 0.2f, 0.2f, 0.9f));
    }
    float v0 = float(OX + std::max(0.0, m_viewStart) / len * OW);
    float v1 = float(OX + std::min(len, viewEnd) / len * OW);
    if (v1 > v0) m_draw->drawRect(CCRect(v0, OY, std::max(1.f, v1 - v0), OH), rgba(1, 1, 1, 0.12f), 0.4f, rgba(1, 1, 1, 0.6f));
    float ocx = float(OX + m_cursor / len * OW);
    m_draw->drawSegment({ ocx, OY - 1 }, { ocx, OY + OH + 1 }, 0.5f, rgba(1, 1, 0.4f, 1));

    double bpm = s.map.bpmAt(m_cursor);
    m_infoLabel->setString(fmt::format("{} / {}    BPM: {}    timing points: {}",
        fmtTime(m_cursor), fmtTime(s.audio ? s.audio->lengthMs : 0),
        bpm > 0 ? fmt::format("{:.2f}", bpm) : std::string("-"), s.map.points.size()).c_str());
}
