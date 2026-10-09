#pragma once
#include "../audio/Metronome.hpp"
#include "../timing/TimingMap.hpp"

#include <Geode/Geode.hpp>
#include <fmod.hpp>

#include <filesystem>
#include <vector>

// Timing editor window: waveform, beat grid, osu!-style timing points, grouped into tabs.
class TimingEditor : public geode::Popup {
public:
    static TimingEditor* s_current;
    static TimingEditor* create();
    static void open(std::filesystem::path const& audio = {});
    // Opened from the editor's WAVE button: jumps to where the editor is and returns there on close
    static void openFromEditor();

    // drag & drop / Session changes
    void handleDroppedFile(std::filesystem::path const& path);
    void onSessionChanged();
    void onMusicVolume(cocos2d::CCObject*);
    void onMetronomeVolume(cocos2d::CCObject*);
    bool onScroll(float y, float x);

protected:
    bool init();
    void onClose(cocos2d::CCObject*) override;
    void update(float dt) override;
    void keyDown(cocos2d::enumKeyCodes key, double timestamp) override;
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

private:
    struct Tab {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCMenu* menu = nullptr;
        cocos2d::CCLabelBMFont* label = nullptr; // label of the tab button
        cocos2d::CCNode* button = nullptr;
        std::vector<geode::TextInput*> inputs;
    };
    std::vector<Tab> m_tabs;
    int m_tab = 0;
    void layoutTabs();

    // rhythm patterns (Rhythm tab)
    int m_rhythmTab = -1;
    int m_rhythmSel = -1;
    int m_rhythmPage = 0; // which bar of the pattern is shown
    cocos2d::CCMenu* m_rhythmGrid = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmLabel = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmLenLabel = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmLoopsLabel = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmPageLabel = nullptr;
    std::string m_rhythmGridKey;
    void rebuildRhythmGrid();
    void selectRhythm(int idx);
    void addRhythmPoint(bool normal);
    void deleteRhythmPoint();
    void setRhythmBars(int bars);
    void rhythmFromAudio();
    int rhythmMeter(int idx) const;

    // view
    double m_viewStart = -500;
    double m_msPerPx = 8;
    double m_cursor = 0;
    int m_selected = -1;
    int m_divisor = 4;
    bool m_shiftAll = false;

    // playback
    FMOD::Sound* m_sound = nullptr;
    FMOD::Channel* m_channel = nullptr;
    std::filesystem::path m_soundPath;
    bool m_playing = false;
    bool m_fromEditor = false; // sync playback with the level editor on close
    metronome::Tracker m_tracker;

    // undo / redo of timing point edits (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z)
    std::vector<TimingMap> m_undo;
    std::vector<TimingMap> m_redo;
    TimingMap m_lastMap;           // timing after the last recorded change
    std::string m_historyKey;      // song + level the history belongs to
    double m_lastRecord = -10;     // m_clock of the last recorded change (quick edits are merged)

    // tap tempo
    std::vector<double> m_taps;
    double m_clock = 0;

    // dragging
    enum class Drag { None, Pending, Scroll, Marker, Rhythm, Overview };
    Drag m_drag = Drag::None;
    float m_dragStartX = 0;
    double m_dragStartView = 0;
    double m_dragStartTime = 0;

    cocos2d::CCDrawNode* m_draw = nullptr;
    cocos2d::CCLabelBMFont* m_fileLabel = nullptr;
    cocos2d::CCLabelBMFont* m_infoLabel = nullptr;
    cocos2d::CCLabelBMFont* m_gridLabel = nullptr;
    cocos2d::CCLabelBMFont* m_pointLabel = nullptr;
    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_divLabel = nullptr;
    cocos2d::CCLabelBMFont* m_playLabel = nullptr;
    cocos2d::CCLabelBMFont* m_metroLabel = nullptr;
    cocos2d::CCLabelBMFont* m_allLabel = nullptr;
    cocos2d::CCLabelBMFont* m_changesLabel = nullptr;
    cocos2d::CCLabelBMFont* m_halfLabel = nullptr;
    cocos2d::CCLabelBMFont* m_offsetLabel = nullptr;
    cocos2d::CCLabelBMFont* m_soundLabel = nullptr;
    cocos2d::CCLabelBMFont* m_ticksLabel = nullptr;
    cocos2d::CCLabelBMFont* m_accentLabel = nullptr;
    Slider* m_musicSlider = nullptr;
    Slider* m_metroSlider = nullptr;
    cocos2d::CCLabelBMFont* m_musicVolLabel = nullptr;
    cocos2d::CCLabelBMFont* m_metroVolLabel = nullptr;
    std::vector<cocos2d::CCLabelBMFont*> m_markerLabels;
    geode::TextInput* m_timeInput = nullptr;
    geode::TextInput* m_bpmInput = nullptr;
    geode::TextInput* m_meterInput = nullptr;
    geode::TextInput* m_minBpmInput = nullptr;
    geode::TextInput* m_maxBpmInput = nullptr;
    bool m_updatingInputs = false;

    // helpers
    cocos2d::CCMenu* m_menu = nullptr; // tab bar
    Tab& addTab(char const* name, float x);
    void switchTab(int idx);
    CCMenuItemSpriteExtra* addButton(cocos2d::CCMenu* menu, char const* text, cocos2d::CCPoint pos,
                                     geode::Function<void()> cb, char const* bg = "GJ_button_04.png",
                                     cocos2d::CCLabelBMFont** labelOut = nullptr);
    geode::TextInput* addInput(Tab& tab, char const* caption, float x, float y, float width,
                               char const* placeholder, geode::CommonFilter filter);
    void addDescription(Tab& tab, char const* text, float topY, float x = 16);
    void setToggle(cocos2d::CCLabelBMFont* label, char const* name, bool on);

    void redraw();
    void refreshPanel();
    void selectPoint(int idx);
    void changed(); // timing edited -> save + refresh
    void recordHistory(); // remembers the timing before an edit (call after the edit)
    void resetHistory();
    void undo();
    void redo();
    double timeAtX(float x) const;
    float xAtTime(double t) const;
    bool anyInputFocused() const;
    double songLength() const;

    void togglePlay();
    void stopPlayback();
    void seek(double ms);
    double playPosition() const;

    void pickAudio();
    void pickOsu();
    void exportOsu();
    void addPointAtCursor();
    void deletePoint();
    void shiftPoint(double ms);
    void tap();
    void zoom(double factor, double anchorMs);
};
