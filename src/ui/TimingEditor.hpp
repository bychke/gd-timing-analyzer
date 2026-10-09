#pragma once
#include <map>
#include "../audio/Metronome.hpp"
#include "../timing/TimingMap.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/binding/ButtonSprite.hpp>
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
        cocos2d::CCNode* bg = nullptr;           // background of the tab button (tinted when selected)
        char const* name = "";
        char const* iconName = nullptr;
        std::vector<geode::TextInput*> inputs;
        std::string help; // shown by the "i" button
    };
    std::vector<Tab> m_tabs;
    int m_tab = 0;
    void layoutTabs();
    void makeTabButton(int idx, float width);

    // rhythm patterns (Rhythm tab)
    int m_rhythmTab = -1;
    int m_rhythmSel = -1;
    int m_rhythmPage = 0; // which bar of the pattern is shown
    cocos2d::CCMenu* m_rhythmGrid = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmLabel = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmLenLabel = nullptr;
    cocos2d::CCLabelBMFont* m_rhythmEmptyLabel = nullptr;
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
    enum class Drag { None, Pending, Scroll, Marker, Rhythm, Overview, Layout };
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
    cocos2d::CCSprite* m_playIcon = nullptr;
    geode::TextInput* m_offsetInput = nullptr;
    void updatePlayIcon();

    // "Move UI": drag the controls of a tab around; offsets are saved ("ui-layout")
    CCMenuItemSpriteExtra* m_helpBtn = nullptr;
    cocos2d::CCLabelBMFont* m_osuLabel = nullptr;
    bool m_layoutEdit = false;
    cocos2d::CCNode* m_editSel = nullptr;
    cocos2d::CCPoint m_editGrab, m_editStart;
    std::map<cocos2d::CCNode*, cocos2d::CCPoint> m_uiBase;
    std::map<cocos2d::CCNode*, std::string> m_uiIds;
    cocos2d::CCDrawNode* m_editDraw = nullptr;
    cocos2d::CCLabelBMFont* m_editHint = nullptr;
    cocos2d::CCLabelBMFont* m_editBtnLabel = nullptr;
    void applyUiLayout();
    void saveUiLayout();
    void toggleLayoutEdit();
    void drawLayoutEdit();
    bool inCurrentTab(cocos2d::CCNode* n) const;
    cocos2d::CCNode* pickUiNode(cocos2d::CCPoint p) const;
    CCMenuItemToggler* m_metroCheck = nullptr;
    CCMenuItemToggler* m_allCheck = nullptr;
    CCMenuItemToggler* m_changesCheck = nullptr;
    cocos2d::CCLabelBMFont* m_halfLabel = nullptr;
    cocos2d::CCLabelBMFont* m_offsetLabel = nullptr;
    cocos2d::CCLabelBMFont* m_soundLabel = nullptr;
    cocos2d::CCLabelBMFont* m_ticksLabel = nullptr;
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
    cocos2d::CCNode* m_settingsBtn = nullptr; // "UI Settings" at the right end of the tab bar
    Tab& addTab(char const* name, float x, char const* iconName = nullptr);
    static cocos2d::CCSprite* icon(char const* name);
    static cocos2d::CCNode* pill(char const* text, char const* iconName, char const* bg, float width, float height,
                                 float textScale = .32f, bool iconRight = false, cocos2d::CCLabelBMFont** labelOut = nullptr);
    CCMenuItemSpriteExtra* addPill(cocos2d::CCMenu* menu, char const* text, char const* iconName, char const* bg,
                                   cocos2d::CCPoint pos, float width, float height, geode::Function<void()> cb,
                                   bool iconRight = false, cocos2d::CCLabelBMFont** labelOut = nullptr, float textScale = .32f);
    cocos2d::CCNode* roundIcon(char const* name, float fill = .7f);
    CCMenuItemSpriteExtra* addRound(Tab& tab, cocos2d::CCNode* top, geode::CircleBaseColor color, cocos2d::CCPoint pos,
                                    char const* caption, geode::Function<void()> cb, float size = 24.f);
    cocos2d::CCLabelBMFont* addRoundText(Tab& tab, char const* text, geode::CircleBaseColor color, cocos2d::CCPoint pos,
                                         char const* caption, geode::Function<void()> cb, float size = 24.f);
    static void fitRoundText(cocos2d::CCLabelBMFont* lbl);
    static void setRoundText(cocos2d::CCLabelBMFont* lbl, std::string const& text);
    CCMenuItemSpriteExtra* addFrameButton(Tab& tab, char const* frame, cocos2d::CCPoint pos, char const* caption,
                                          geode::Function<void()> cb, float size = 24.f);
    void addCaption(Tab& tab, char const* text, cocos2d::CCPoint pos);
    void switchTab(int idx);
    CCMenuItemSpriteExtra* addButton(cocos2d::CCMenu* menu, char const* text, cocos2d::CCPoint pos,
                                     geode::Function<void()> cb, char const* bg = "GJ_button_04.png",
                                     cocos2d::CCLabelBMFont** labelOut = nullptr, float scale = .5f);
    geode::TextInput* addInput(Tab& tab, char const* caption, float x, float y, float width,
                               char const* placeholder, geode::CommonFilter filter);
    ButtonSprite* addSelector(Tab& tab, char const* text, cocos2d::CCPoint pos, char const* widest,
                              geode::Function<void()> cb);
    void addSeparator(Tab& tab, float x, bool lowOnly = false, float around = 0,
                      cocos2d::ccColor4B color = { 0, 0, 0, 70 });
    cocos2d::CCLabelBMFont* addArrowText(Tab& tab, char const* text, cocos2d::CCPoint pos, char const* caption,
                                         geode::Function<void(int)> cb, float half = 30.f);
    void addArrow(Tab& tab, cocos2d::CCPoint pos, int dir, geode::Function<void()> cb);
    static void addArrowTo(cocos2d::CCMenu* menu, cocos2d::CCPoint pos, int dir, geode::Function<void()> cb);
    void addHeader(Tab& tab, char const* text, cocos2d::CCPoint pos);
    void addHelp(Tab& tab, char const* text);
    CCMenuItemToggler* addCheckbox(cocos2d::CCMenu* menu, char const* text, cocos2d::CCPoint pos,
                                   geode::Function<void(bool)> cb, bool left = false, float* widthOut = nullptr);
    void setCheck(CCMenuItemToggler* toggle, bool on);

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
