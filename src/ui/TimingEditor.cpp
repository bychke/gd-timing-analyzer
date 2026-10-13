#include "TimingEditor.hpp"
#include "../Options.hpp"

#include "../Session.hpp"
#include "LayoutPopup.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/ui/MDPopup.hpp>
#include <Geode/ui/ColorPickPopup.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>

#include <algorithm>
#include <cmath>
#include <commdlg.h>

using namespace geode::prelude;

namespace {
constexpr float POP_W = 540.f, POP_H = 312.f;
// waveform area (m_mainLayer coordinates)
constexpr float WX = 15.f, WY = 178.f, WW = 510.f, WH = 92.f;
// whole-song overview strip
constexpr float OX = 15.f, OY = 165.f, OW = 510.f, OH = 9.f;
// tab content panel
constexpr float PY0 = 6.f, PY1 = 113.f;
// rows inside the panel: round buttons (RA) with their captions (RC) under them, then a row of small controls (RB)
constexpr float HY = 75.f, RA = 55.f, RC = 35.f, RB = 16.f;
constexpr int kArrowTextTag = 7701;
constexpr float kButtonScale = .5f;
// the tabs: two rows in the strip between the overview and the panel, three equal cells per row
constexpr float kTabY1 = 151.f, kTabY2 = 127.f, kTabH = 22.f;
constexpr float kTabLeft = 140.f, kTabGap = 3.f, kTabW = (WX + WW - kTabLeft - 2 * kTabGap) / 3;
constexpr float kWindowScale = .9f; // the whole window, drawn 10% smaller

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
    if (auto ed = create()) {
        ed->show();
        ed->m_mainLayer->stopAllActions();
        ed->m_mainLayer->setScale(.1f);
        ed->m_mainLayer->runAction(CCEaseElasticOut::create(CCScaleTo::create(.5f, kWindowScale), .6f));
    }
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
                                               char const* bg, CCLabelBMFont** labelOut, float scale) {
    auto spr = ButtonSprite::create(text, "bigFont.fnt", bg, .7f);
    spr->setScale(scale); // before the menu item is made: it takes its hitbox from the sprite
    if (labelOut) *labelOut = spr->m_label;
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    menu->addChild(btn);
    return btn;
}

TimingEditor::Tab& TimingEditor::addTab(char const* name, float x, char const* iconName) {
    Tab tab;
    tab.node = CCNode::create();
    m_mainLayer->addChild(tab.node);
    tab.menu = CCMenu::create();
    tab.menu->setPosition({ 0, 0 });
    tab.node->addChild(tab.menu);
    tab.name = name;
    tab.iconName = iconName;
    m_tabs.push_back(tab);
    makeTabButton((int)m_tabs.size() - 1, 0);
    return m_tabs.back();
}

// icon + name on a gray GD button (layoutTabs() places them; width 0 = natural width)
void TimingEditor::makeTabButton(int idx, float width) {
    auto& t = m_tabs[idx];
    CCPoint pos{ 0, kTabY1 };
    if (t.button) {
        pos = t.button->getPosition();
        t.button->removeFromParent();
    }
    t.button = addPill(m_menu, t.name, t.iconName, "GJ_button_04.png", pos, width, kTabH,
                       [this, idx] { switchTab(idx); }, false, &t.label, .42f);
    t.bg = static_cast<CCMenuItemSpriteExtra*>(t.button)->getNormalImage()->getChildByTag(1);
}

// picture of a GD button: rounded background, optional icon and a label; width 0 = as wide as the content
// (otherwise the label shrinks to fit). The background is the child with tag 1.
CCNode* TimingEditor::pill(char const* text, char const* iconName, char const* bg, float width, float height,
                           float textScale, bool iconRight, CCLabelBMFont** labelOut) {
    auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
    lbl->setScale(textScale);
    CCSprite* ic = iconName ? icon(iconName) : nullptr;
    float icSize = height * .7f, pad = height * .27f, gap = 3;
    float icW = ic ? icSize + gap : 0;
    if (width <= 0) width = pad * 2 + icW + lbl->getScaledContentSize().width;
    lbl->limitLabelWidth(std::max(10.f, width - pad * 2 - icW), textScale, .1f);
    float lw = lbl->getScaledContentSize().width;

    auto node = CCNode::create();
    node->setContentSize({ width, height });
    node->setAnchorPoint({ .5f, .5f });
    // drawn twice as big and scaled down: same rounded corners as ButtonSprite
    auto back = CCScale9Sprite::create(bg, { 0, 0, 40, 40 });
    back->setContentSize({ width * 2, height * 2 });
    back->setScale(.5f);
    back->setPosition({ width / 2, height / 2 });
    back->setTag(1);
    node->addChild(back);

    float x = (width - icW - lw) / 2;
    auto placeIcon = [&](float left) {
        ic->setScale(icSize / std::max(ic->getContentSize().width, ic->getContentSize().height));
        ic->setPosition({ left + icSize / 2, height / 2 });
        node->addChild(ic, 2);
    };
    if (ic && !iconRight) { placeIcon(x); x += icW; }
    lbl->setPosition({ x + lw / 2, height / 2 + 1 });
    node->addChild(lbl, 2);
    if (ic && iconRight) placeIcon(x + lw + gap);
    if (labelOut) *labelOut = lbl;
    return node;
}

CCMenuItemSpriteExtra* TimingEditor::addPill(CCMenu* menu, char const* text, char const* iconName, char const* bg, CCPoint pos,
                                             float width, float height, geode::Function<void()> cb, bool iconRight,
                                             CCLabelBMFont** labelOut, float textScale) {
    auto spr = pill(text, iconName, bg, width, height, textScale, iconRight, labelOut);
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    menu->addChild(btn);
    return btn;
}

// a sprite from GD's sheets or from the mod's own resources
CCSprite* TimingEditor::icon(char const* name) {
    // the mod's own pictures ("mod.id/file.png") are separate files, GD's are frames in its sprite sheets.
    // (Asking the frame cache is no test: Geode answers a missing frame with its pink/black placeholder.)
    if (std::strchr(name, '/')) return CCSprite::create(name);
    return CCSprite::createWithSpriteFrameName(name);
}

// round GD button (colored circle) with an icon or a label on it and a small caption under it
CCMenuItemSpriteExtra* TimingEditor::addRound(Tab& tab, CCNode* top, CircleBaseColor color, CCPoint pos,
                                              char const* caption, geode::Function<void()> cb, float size) {
    auto spr = CircleButtonSprite::create(top, color, CircleBaseSize::Small);
    spr->setScale(size / spr->getContentSize().height);
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    tab.menu->addChild(btn);
    if (caption) addCaption(tab, caption, { pos.x, RC });
    return btn;
}

// icon sized to sit inside a round button
CCNode* TimingEditor::roundIcon(char const* name, float fill) {
    auto ic = icon(name);
    if (!ic) return CCNode::create();
    ic->setScale(34.f * fill / std::max(ic->getContentSize().width, ic->getContentSize().height));
    return ic;
}

// round button showing a value ("1/4", "WOOD"); returns the label to update
CCLabelBMFont* TimingEditor::addRoundText(Tab& tab, char const* text, CircleBaseColor color, CCPoint pos,
                                          char const* caption, geode::Function<void()> cb, float size) {
    auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
    lbl->setAlignment(kCCTextAlignmentCenter);
    fitRoundText(lbl);
    addRound(tab, lbl, color, pos, caption, std::move(cb), size);
    return lbl;
}

void TimingEditor::fitRoundText(CCLabelBMFont* lbl) {
    // a value between arrows stays small (and fits between them), one on a round button may be bigger
    if (lbl->getTag() == kArrowTextTag) {
        auto w = typeinfo_cast<CCFloat*>(lbl->getUserObject("max-width"));
        lbl->limitLabelWidth(w ? w->getValue() : 40.f, .45f, .15f);
    }
    else lbl->limitLabelWidth(40.f, .6f, .2f);
}

void TimingEditor::setRoundText(CCLabelBMFont* lbl, std::string const& text) {
    if (!lbl) return;
    // label of a plain "Prefix: value" button: the button keeps its width and fits the text
    if (auto pre = typeinfo_cast<CCString*>(lbl->getUserObject())) {
        auto full = std::string(pre->getCString()) + text;
        if (full != lbl->getString())
            if (auto bs = typeinfo_cast<ButtonSprite*>(lbl->getParent())) bs->setString(full.c_str());
        return;
    }
    if (text == lbl->getString()) return;
    lbl->setString(text.c_str());
    fitRoundText(lbl);
}

// one of GD's own round buttons (GJ_plusBtn, GJ_trashBtn ...) with a caption
CCMenuItemSpriteExtra* TimingEditor::addFrameButton(Tab& tab, char const* frame, CCPoint pos, char const* caption,
                                                    geode::Function<void()> cb, float size) {
    auto spr = icon(frame);
    if (!spr) spr = CCSprite::create();
    spr->setScale(size / std::max(1.f, spr->getContentSize().height));
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    tab.menu->addChild(btn);
    if (caption) addCaption(tab, caption, { pos.x, RC });
    return btn;
}

// value between two green GD arrows ("<  WOOD  >"); cb gets -1 / +1; caption under it
CCLabelBMFont* TimingEditor::addArrowText(Tab& tab, char const* text, CCPoint pos, char const* caption,
                                          geode::Function<void(int)> cb, float half) {
    auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
    lbl->setScale(.4f);
    lbl->setTag(kArrowTextTag);
    lbl->setUserObject("max-width", CCFloat::create(half * 2 - 20));
    fitRoundText(lbl);
    lbl->setPosition(pos);
    tab.node->addChild(lbl);
    auto shared = std::make_shared<geode::Function<void(int)>>(std::move(cb));
    addArrow(tab, pos - CCPoint{ half, 0 }, -1, [shared] { (*shared)(-1); });
    addArrow(tab, pos + CCPoint{ half, 0 }, 1, [shared] { (*shared)(1); });
    if (caption) addCaption(tab, caption, { pos.x, RC });
    return lbl;
}

void TimingEditor::addCaption(Tab& tab, char const* text, CCPoint pos) {
    auto l = CCLabelBMFont::create(text, "bigFont.fnt");
    l->setScale(.26f);
    l->limitLabelWidth(44.f, .26f, .15f);
    l->setPosition(pos);
    tab.node->addChild(l);
}

// the tab buttons in two rows of three equal cells right of the position info:
// Playback, Timing Points, Rhythm / (free: room for the cursor text) Files, Triggers
void TimingEditor::layoutTabs() {
    for (int i = 0; i < (int)m_tabs.size(); i++) {
        int row = i / 3, col = i % 3 + (row == 1 ? 1 : 0);
        makeTabButton(i, kTabW);
        m_tabs[i].button->setPosition({ kTabLeft + col * (kTabW + kTabGap) + kTabW / 2, row == 0 ? kTabY1 : kTabY2 });
    }
    if (m_settingsBtn) m_settingsBtn->setPosition({ kTabLeft + 2 * (kTabW + kTabGap) + kTabW / 2, kTabY2 });
}

// a short message above the window
void TimingEditor::toast(std::string const& text) {
    if (!m_toast || !m_toastLabel) return;
    m_toastLabel->setString(text.c_str());
    m_toastLabel->limitLabelWidth(330, .38f, .15f);
    float w = m_toastLabel->getScaledContentSize().width + 18, h = 16;
    if (auto bg = typeinfo_cast<CCScale9Sprite*>(m_toast->getChildByTag(1))) {
        bg->setContentSize({ w * 4, h * 4 });
        bg->setScale(.25f);
    }
    m_toast->stopAllActions();
    m_toast->setVisible(true);
    m_toast->runAction(CCSequence::create(CCDelayTime::create(.5f), CCHide::create(), nullptr));
}

void TimingEditor::switchTab(int idx) {
    if (idx < 0 || idx >= (int)m_tabs.size()) idx = 0;
    m_tab = idx;
    Mod::get()->setSavedValue("last-tab", idx);
    for (int i = 0; i < (int)m_tabs.size(); i++) {
        bool on = i == idx;
        m_tabs[i].node->setVisible(on);
        m_tabs[i].menu->setEnabled(on && !m_layoutEdit);
        for (auto in : m_tabs[i].inputs) in->setEnabled(on && !m_layoutEdit);
        m_tabs[i].label->setColor(on ? ccColor3B{ 255, 220, 60 } : ccColor3B{ 255, 255, 255 });
        if (auto bg = typeinfo_cast<CCRGBAProtocol*>(m_tabs[i].bg))
            bg->setColor(on ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 150, 150, 150 });
    }
    if (m_helpBtn) m_helpBtn->setVisible(!m_tabs[idx].help.empty());
    refreshTriggerTab();
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

// one compact fixed-width button ("Grid: 1/4") that cycles through the values on click
ButtonSprite* TimingEditor::addSelector(Tab& tab, char const* text, CCPoint pos, char const* widest,
                                        geode::Function<void()> cb) {
    // fixed size = a normal button with the widest text: changing the text never moves the picture off its hitbox
    auto probe = ButtonSprite::create(widest, "bigFont.fnt", "GJ_button_04.png", .7f);
    auto size = probe->getContentSize();
    auto spr = ButtonSprite::create(text, (int)size.width, true, "bigFont.fnt", "GJ_button_04.png", size.height, .7f);
    spr->setScale(kButtonScale);
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    tab.menu->addChild(btn);
    return spr;
}

// thin vertical line between two groups of a tab (lowOnly = only next to the bottom row; around = center y of a short one)
void TimingEditor::addSeparator(Tab& tab, float x, bool lowOnly, float around, ccColor4B color) {
    float y0 = PY0 + 5, y1 = lowOnly ? RB + 12 : PY1 - 5;
    if (around > 0) { y0 = around - 12; y1 = around + 12; }
    auto line = CCLayerColor::create(color, 1.f, y1 - y0);
    line->setPosition({ x, y0 });
    tab.node->addChild(line);
}

// GD's green page arrow (dir < 0 = left)
void TimingEditor::addArrow(Tab& tab, CCPoint pos, int dir, geode::Function<void()> cb) {
    addArrowTo(tab.menu, pos, dir, std::move(cb));
}

void TimingEditor::addArrowTo(CCMenu* menu, CCPoint pos, int dir, geode::Function<void()> cb) {
    auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    spr->setScale(.32f);
    spr->setFlipX(dir > 0);
    auto btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
    btn->setPosition(pos);
    menu->addChild(btn);
}

// gold group title
void TimingEditor::addHeader(Tab& tab, char const* text, CCPoint pos) {
    auto l = CCLabelBMFont::create(text, "goldFont.fnt");
    l->setScale(.5f);
    l->limitLabelWidth(200.f, .5f, .2f);
    l->setAnchorPoint({ 0, .5f });
    l->setPosition(pos);
    tab.node->addChild(l);
}

// the tab's description, shown by the "i" button in the window's top right corner
void TimingEditor::addHelp(Tab& tab, char const* text) {
    // Geode's markdown popup: normal-sized text that scrolls (GD's alert scaled it up huge);
    // an empty line between the lines = separate paragraphs with space between them
    std::string desc = text;
    for (size_t i = 0; (i = desc.find('\n', i)) != std::string::npos; i += 2) desc.insert(i, "\n");
    tab.help = desc;
}


// checkbox + label, centered on pos like a button (left = pos is the left edge); widthOut = width of both
CCMenuItemToggler* TimingEditor::addCheckbox(CCMenu* menu, char const* text, CCPoint pos, geode::Function<void(bool)> cb,
                                             bool left, float* widthOut) {
    auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
    lbl->setScale(.38f);
    lbl->setAnchorPoint({ 0, .5f });
    float box = 21, total = box + 3 + lbl->getScaledContentSize().width;
    float x0 = left ? pos.x : pos.x - total / 2;
    if (widthOut) *widthOut = total;
    // the callback runs before the toggler flips
    auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(.64f,
        [cb = std::move(cb)](CCMenuItemToggler* t) mutable { cb(!t->isToggled()); });
    toggle->setPosition({ x0 + box / 2, pos.y });
    menu->addChild(toggle);
    lbl->setPosition({ x0 + box + 3, pos.y });
    menu->getParent()->addChild(lbl);
    return toggle;
}

void TimingEditor::setCheck(CCMenuItemToggler* toggle, bool on) {
    if (toggle && toggle->isToggled() != on) toggle->toggle(on);
}

// ---------------- layout ----------------

bool TimingEditor::init() {
    if (!Popup::init(POP_W, POP_H)) return false;
    s_current = this;
    // centered in the strip above the Song / Level line, away from the window's edge
    this->setTitle("Timing Analyzer", "goldFont.fnt", .6f, 15.f);

    m_fileLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_fileLabel->setScale(.5f);
    m_fileLabel->setAnchorPoint({ 0, .5f });
    m_fileLabel->setPosition({ 16, POP_H - 33 });
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
    m_infoLabel->setPosition({ WX, kTabY1 });
    m_mainLayer->addChild(m_infoLabel);

    m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_statusLabel->setScale(.5f);
    // top right corner of the waveform (the tabs are on the right of the info rows)
    m_statusLabel->setAnchorPoint({ 1, 1 });
    m_statusLabel->setPosition({ WX + WW - 3, WY + WH - 2 });
    m_statusLabel->setColor({ 120, 255, 120 });
    m_mainLayer->addChild(m_statusLabel, 3);

    // which grid line the cursor is on (bar / beat / 1/8 ...)
    m_gridLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_gridLabel->setScale(.5f);
    m_gridLabel->setAnchorPoint({ 0, .5f });
    m_gridLabel->setPosition({ WX, kTabY2 });
    m_mainLayer->addChild(m_gridLabel);

    // short messages above the window ("... turned on / off")
    m_toast = CCNode::create();
    m_toast->setPosition({ POP_W / 2, POP_H + 11 }); // above the window
    m_toast->setVisible(false);
    auto toastBg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    toastBg->setColor({ 0, 0, 0 });
    toastBg->setOpacity(190);
    toastBg->setTag(1);
    m_toast->addChild(toastBg);
    m_toastLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_toast->addChild(m_toastLabel, 1);
    m_mainLayer->addChild(m_toast, 20);

    // "Song Start" next to its line on the waveform
    m_startLabel = CCLabelBMFont::create("Song Start", "chatFont.fnt");
    m_startLabel->setScale(.4f);
    m_startLabel->setAnchorPoint({ 0, .5f });
    m_startLabel->setColor({ 64, 255, 204 });
    m_startLabel->setVisible(false);
    m_mainLayer->addChild(m_startLabel, 3);

    // tab bar + always-visible play button
    m_menu = CCMenu::create();
    m_menu->setPosition({ 0, 0 });
    m_mainLayer->addChild(m_menu, 5);

    // Each tab: up to three rows of big plain buttons, left to right. The description is behind the "i" button.
    float const BTN = kButtonScale; // size of the buttons in the tabs
    auto smallBtn = [&](Tab& t, char const* text, float x, float y, auto cb, char const* bg = "GJ_button_04.png") {
        return addButton(t.menu, text, { x, y }, cb, bg, nullptr, kButtonScale);
    };
    Tab* ft = nullptr;
    float fx = 0, fy = 0;
    // starts a row at (x, y)
    auto row = [&](Tab& t, float x, float y) {
        ft = &t;
        fx = x;
        fy = y;
    };
    auto btn = [&](char const* text, geode::Function<void()> cb, char const* bg = "GJ_button_04.png") {
        auto b = addButton(ft->menu, text, { 0, fy }, std::move(cb), bg, nullptr, BTN);
        float w = b->getScaledContentSize().width;
        b->setPositionX(fx + w / 2);
        fx += w + 5;
        return b;
    };
    auto text = [&](char const* str) {
        auto l = CCLabelBMFont::create(str, "bigFont.fnt");
        l->setScale(.38f);
        l->setAnchorPoint({ 0, .5f });
        l->setPosition({ fx, fy });
        l->setColor({ 190, 225, 255 });
        ft->node->addChild(l);
        fx += l->getScaledContentSize().width + 5;
        return l;
    };
    auto input = [&](float width, char const* placeholder, CommonFilter filter) {
        auto in = addInput(*ft, "", fx + width * .35f, fy, width, placeholder, filter);
        in->setScale(.7f);
        fx += width * .7f + 6;
        return in;
    };
    auto check = [&](char const* label, geode::Function<void(bool)> cb) {
        float w = 0;
        auto c = addCheckbox(ft->menu, label, { fx, fy }, std::move(cb), true, &w);
        fx += w + 8;
        return c;
    };
    // cycles a saved text option through its values (dir = -1 / +1)
    auto step = [](char const* key, std::vector<std::string> const& values, int dir) {
        auto cur = opt::get<std::string>(key);
        auto it = std::find(values.begin(), values.end(), cur);
        int n = (int)values.size();
        int i = it == values.end() ? 0 : (((int)(it - values.begin()) + dir) % n + n) % n;
        opt::set<std::string>(key, values[i]);
        return values[i];
    };
    auto upper = [](std::string v) { for (auto& c : v) c = (char)std::toupper((unsigned char)c); return v; };
    // the description is shown by the "i" button (each line = a paragraph, "   " lines continue the one above)
    auto desc = [&](Tab& t, char const* text) {
        std::string d = text;
        for (size_t i; (i = d.find("\n   ")) != std::string::npos;) d.replace(i, 4, " ");
        // an empty line between the lines, except inside a list ("- " / "1. " lines stay together)
        auto isItem = [&](size_t at) {
            return d.compare(at, 2, "- ") == 0 || (at + 1 < d.size() && std::isdigit((unsigned char)d[at]) && d[at + 1] == '.');
        };
        for (size_t i = 0; (i = d.find('\n', i)) != std::string::npos;) {
            size_t lineStart = d.rfind('\n', i - 1);
            lineStart = lineStart == std::string::npos || i == 0 ? 0 : lineStart + 1;
            if (isItem(lineStart) && isItem(i + 1)) { i += 1; continue; }
            d.insert(i, "\n");
            i += 2;
        }
        if (!t.help.empty()) t.help += "\n\n";
        t.help += d;
    };

    // ===== 1. Playback (+ where the song starts, + editor guidelines) =====
    {
        auto& t = addTab("Playback", 50, "ta_note.png"_spr);
        float const YA = 90, YB = 58, YC = 24; // three rows
        // a round green GD button with an icon (same look as the play button)
        auto round = [&](char const* iconName, CircleBaseColor color, geode::Function<void()> cb) {
            auto b = addRound(t, roundIcon(iconName, .62f), color, { 0, fy }, nullptr, std::move(cb), 26.f);
            b->setPositionX(fx + 13);
            fx += 31;
            return b;
        };
        // a value between two arrows ("<  CLASSIC  >"), cb gets -1 / +1
        auto stepper = [&](char const* value, float half, geode::Function<void(int)> cb) {
            auto l = addArrowText(t, value, { fx + half + 6, fy }, nullptr, std::move(cb), half);
            fx += half * 2 + 20;
            return l;
        };

        // row 1 "Main": to the start, play, to the end | the waveform: to the cursor, to Song Start, follow
        row(t, 16, YA);
        text("Main");
        fx += 4;
        round("ta_to_start.png"_spr, CircleBaseColor::Green, [this] {
            seek(0);
            m_viewStart = -500;
            toast("Back to the start of the song");
        });
        {
            // GD's own play / stop music button
            auto spr = CCSprite::createWithSpriteFrameName("GJ_playMusicBtn_001.png");
            spr->setScale(26.f / spr->getContentSize().height);
            m_playIcon = spr;
            auto b = CCMenuItemExt::createSpriteExtra(spr, [this](auto) { togglePlay(); });
            b->setPosition({ fx + 13, fy });
            t.menu->addChild(b);
            fx += 31;
        }
        // jump to the end of the song
        round("ta_to_end.png"_spr, CircleBaseColor::Green, [this] {
            double end = songLength();
            seek(std::max(0.0, end - 50));
            m_viewStart = end - WW * m_msPerPx * 0.75;
            toast("Jumped to the end of the song");
        });
        round("ta_goto.png"_spr, CircleBaseColor::Green, [this] {
            m_viewStart = m_cursor - WW * m_msPerPx / 2;
            toast("Waveform scrolled to the cursor");
        });
        round("ta_goto_song.png"_spr, CircleBaseColor::Blue, [this] {
            auto ls = Session::levelSettings();
            if (!ls) {
                Notification::create("Open this from a level (editor or song selection)", NotificationIcon::Error)->show();
                return;
            }
            m_viewStart = ls->m_songOffset * 1000.0 - WW * m_msPerPx / 2;
            toast("Waveform scrolled to Song Start");
        });
        // follow: while playing the waveform scrolls with the cursor (off = it stays where you put it)
        m_followPlay = Mod::get()->getSavedValue<bool>("follow-playback", true);
        {
            auto makeSpr = [this](bool on) {
                auto spr = CircleButtonSprite::create(roundIcon("ta_follow.png"_spr, .62f),
                    on ? CircleBaseColor::Green : CircleBaseColor::Gray, CircleBaseSize::Small);
                spr->setScale(26.f / spr->getContentSize().height);
                return spr;
            };
            auto tg = CCMenuItemExt::createToggler(makeSpr(true), makeSpr(false), [this](CCMenuItemToggler* tgl) {
                // the callback runs before the toggler flips
                m_followPlay = !tgl->isToggled();
                Mod::get()->setSavedValue<bool>("follow-playback", m_followPlay);
                toast(m_followPlay ? "Following the cursor turned on" : "Following the cursor turned off");
            });
            tg->setPosition({ fx + 13, fy });
            t.menu->addChild(tg);
            setCheck(tg, m_followPlay);
            fx += 31;
        }
        m_rate = std::clamp((float)Mod::get()->getSavedValue<double>("playback-speed", 1.0), .25f, 1.f);

        // row 2 "Metronome": BPM, the waveform in the editor, the sound and how often it clicks
        row(t, 16, YB);
        text("Metronome");
        fx += 4;
        // metronome (one for the waveform and the level editor's BPM button), next to follow
        {
            // same green "BPM ON / gray BPM OFF" round button as in the level editor
            auto makeSpr = [](bool on) {
                auto lbl = CCLabelBMFont::create(on ? "BPM\nON" : "BPM\nOFF", "bigFont.fnt");
                lbl->setAlignment(kCCTextAlignmentCenter);
                lbl->setScale(.45f);
                auto spr = CircleButtonSprite::create(lbl, on ? CircleBaseColor::Green : CircleBaseColor::Gray,
                                                      CircleBaseSize::Small);
                spr->setScale(26.f / spr->getContentSize().height);
                return spr;
            };
            m_metroCheck = CCMenuItemExt::createToggler(makeSpr(true), makeSpr(false), [this](CCMenuItemToggler* tg) {
                // the callback runs before the toggler flips
                Session::get().setEditorMetronome(!tg->isToggled());
                toast(tg->isToggled() ? "Metronome (BPM) turned off" : "Metronome (BPM) turned on");
            });
            m_metroCheck->setPosition({ fx + 13, fy });
            t.menu->addChild(m_metroCheck);
            fx += 31;
        }
        // a sound that no longer exists (click, deep, drums, clap of older versions) becomes the classic one
        {
            static char const* const names[] = { "classic", "wood" };
            auto cur = opt::get<std::string>("metronome-sound");
            if (std::find(std::begin(names), std::end(names), cur) == std::end(names))
                opt::set<std::string>("metronome-sound", "classic");
        }
        // two small round buttons: the metronome's sound (a black note + SFX; plays a preview) ...
        {
            auto node = CCNode::create();
            node->setContentSize({ 34, 34 });
            node->ignoreAnchorPointForPosition(false);
            node->setAnchorPoint({ .5f, .5f });
            auto ic = icon("ta_note_black.png"_spr);
            ic->setScale(19.f / std::max(ic->getContentSize().width, ic->getContentSize().height));
            ic->setPosition({ 17, 21 });
            node->addChild(ic);
            auto lbl = CCLabelBMFont::create("SFX", "bigFont.fnt");
            lbl->setScale(.32f);
            lbl->setPosition({ 17, 6 });
            node->addChild(lbl, 1);
            auto b = addRound(t, node, CircleBaseColor::Green, { fx + 13, fy }, nullptr, [this, step, upper] {
                auto name = upper(step("metronome-sound", { "classic", "wood" }, 1));
                metronome::click(TickKind::Downbeat); // preview
                toast("Metronome sound: " + name);
            }, 26.f);
            (void)b;
            fx += 31;
        }
        // ... and how often it clicks (pink, like the 1/N buttons): 1/1 = every beat, 1/2 ... = also between the beats
        m_ticksLabel = addRoundText(t, opt::get<std::string>("metronome-ticks").c_str(), CircleBaseColor::Pink,
                                    { fx + 13, fy }, nullptr, [this, step] {
            auto v = step("metronome-ticks", { "1/1", "1/2", "1/3", "1/4" }, 1);
            setRoundText(m_ticksLabel, v);
            toast(v == "1/1" ? std::string("Metronome clicks on every beat")
                             : fmt::format("Metronome also clicks {} between the beats", v));
        }, 26.f);
        fx += 31;

        // WAVE: the song's waveform behind the level in the editor (the same button as in the editor), right of the
        // metronome buttons, with its name
        fx += 2;
        addSeparator(t, fx, false, fy, { 255, 255, 255, 150 });
        fx += 10;
        {
            auto cap = CCLabelBMFont::create("Editor\nWaveform", "bigFont.fnt");
            cap->setScale(.34f);
            cap->setAnchorPoint({ 0, .5f });
            cap->setColor({ 190, 225, 255 });
            cap->setPosition({ fx, fy });
            t.node->addChild(cap);
            fx += cap->getScaledContentSize().width + 8;
        }
        {
            auto makeSpr = [](bool on) {
                auto lbl = CCLabelBMFont::create(on ? "WAVE\nON" : "WAVE\nOFF", "bigFont.fnt");
                lbl->setAlignment(kCCTextAlignmentCenter);
                lbl->setScale(.38f);
                auto spr = CircleButtonSprite::create(lbl, on ? CircleBaseColor::Blue : CircleBaseColor::Gray,
                                                      CircleBaseSize::Small);
                spr->setScale(26.f / spr->getContentSize().height);
                return spr;
            };
            m_waveCheck = CCMenuItemExt::createToggler(makeSpr(true), makeSpr(false), [this](CCMenuItemToggler* tg) {
                // the callback runs before the toggler flips
                Session::get().setEditorWaveform(!tg->isToggled());
                toast(tg->isToggled() ? "Waveform in the editor turned off" : "Waveform in the editor turned on");
            });
            m_waveCheck->setPosition({ fx + 13, fy });
            t.menu->addChild(m_waveCheck);
            setCheck(m_waveCheck, Session::get().editorWaveform());
            fx += 31;
        }

        // row 3, left: where the level's song starts (its Start Offset)
        row(t, 16, YC);
        {
            // "Song" with "Start" under it: this is where the level's song starts
            auto l = text("Song\nStart");
            float before = l->getScaledContentSize().width;
            l->setScale(.34f);
            fx -= before - l->getScaledContentSize().width;
        }
        round("ta_cursor.png"_spr, CircleBaseColor::Green, [this] {
            double ms = m_playing ? playPosition() : m_cursor;
            if (!Session::get().setSongStartOffset(ms)) {
                Notification::create("Open this from a level (editor or song selection)", NotificationIcon::Error)->show();
                return;
            }
            recordHistory();
            toast(fmt::format("Song Start set to {:.3f} s", std::max(0.0, ms) / 1000.0));
        });
        round("ta_to_start.png"_spr, CircleBaseColor::Gray, [this] {
            if (!Session::get().setSongStartOffset(0)) {
                Notification::create("Open this from a level (editor or song selection)", NotificationIcon::Error)->show();
                return;
            }
            recordHistory();
            toast("Song Start set to 0 s (the beginning)");
        });
        float songX = fx;
        m_offsetInput = input(56, "sec", CommonFilter::Float);
        m_offsetInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs) return;
            if (auto v = numFromString<double>(str); v && *v >= 0 && Session::get().setSongStartOffset(*v * 1000.0))
                recordHistory();
        });
        // "Open this from a level" under the field when there is no level to change
        m_offsetLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_offsetLabel->setScale(.36f);
        m_offsetLabel->setPosition({ (songX + fx) / 2 - 3, PY0 + 5 });
        m_offsetLabel->setColor({ 255, 120, 120 });
        t.node->addChild(m_offsetLabel);

        // row 3, right: the beat lines in the editor
        fx += 6;
        addSeparator(t, fx, false, fy, { 255, 255, 255, 150 });
        fx += 12;
        text("Guidelines");
        round("ta_lines.png"_spr, CircleBaseColor::Green, [this] {
            int n = Session::get().applyGuidelines();
            if (n) toast(fmt::format("{} guidelines drawn", n));
            else Notification::create("No timing / not in the editor", NotificationIcon::Error)->show();
        });
        round("ta_lines_x.png"_spr, CircleBaseColor::Red, [this] {
            bool ok = Session::get().clearGuidelines();
            if (ok) toast("Guidelines removed");
            else Notification::create("Not in the editor", NotificationIcon::Error)->show();
        });
        // pink 1/N button like the one in the editor
        m_halfLabel = addRoundText(t, "1/1", CircleBaseColor::Pink, { fx + 13, fy }, nullptr, [this] {
            auto& s = Session::get();
            static int const divs[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
            int i = 0;
            while (i < 8 && divs[i] != s.guideDivisor()) i++;
            int next = divs[(i + 1) % 8];
            s.setGuideDivisor(next);
            s.autoGuidelines();
            recordHistory();
            setRoundText(m_halfLabel, fmt::format("1/{}", next));
            toast(fmt::format("Guidelines and grid set to 1/{}", next));
        }, 26.f);
        fx += 31;
        // automatic guidelines: redrawn by themselves after every timing change (checkbox + two-line label)
        {
            auto box = CCMenuItemExt::createTogglerWithStandardSprites(.64f, [this](CCMenuItemToggler* tg) {
                // the callback runs before the toggler flips
                bool on = !tg->isToggled();
                opt::set<bool>("auto-guidelines", on);
                toast(on ? "Automatic guidelines turned on" : "Automatic guidelines turned off");
                if (on) Session::get().autoGuidelines();
            });
            box->setPosition({ fx + 11, fy });
            t.menu->addChild(box);
            setCheck(box, opt::get<bool>("auto-guidelines"));
            auto lbl = CCLabelBMFont::create("Automatic\nGuidelines", "bigFont.fnt");
            lbl->setScale(.3f);
            lbl->setAnchorPoint({ 0, .5f });
            lbl->setColor({ 190, 225, 255 });
            lbl->setPosition({ fx + 26, fy });
            t.node->addChild(lbl);
            fx += 26 + lbl->getScaledContentSize().width + 6;
        }



        // volume sliders (right), the label above each says which one it is
        auto addVolume = [&](float y, SEL_MenuHandler handler, float value, CCLabelBMFont*& label) {
            label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setScale(.4f);
            label->setPosition({ 430, y + 13 });
            t.node->addChild(label);
            auto s = Slider::create(this, handler, .85f);
            s->setPosition({ 430, y });
            s->setValue(value);
            t.node->addChild(s);
            return s;
        };
        m_musicSlider = addVolume(91, menu_selector(TimingEditor::onMusicVolume),
            FMODAudioEngine::get()->m_musicVolume, m_musicVolLabel);
        m_metroSlider = addVolume(55, menu_selector(TimingEditor::onMetronomeVolume),
            opt::get<int64_t>("metronome-volume") / 100.f, m_metroVolLabel);
        onMusicVolume(nullptr);
        onMetronomeVolume(nullptr);
        // playback speed (the pitch stays, the metronome follows it): "Speed" with the value under it and the clock,
        // in the free place between the first rows and the two sliders. The clock cycles 100% > 75% > 50% > 25%.
        {
            float left = 336, y = YA;
            auto cap = CCLabelBMFont::create("Speed", "bigFont.fnt");
            cap->setScale(.3f);
            cap->setAnchorPoint({ 0, .5f });
            cap->setColor({ 190, 225, 255 });
            cap->setPosition({ 258, y + 8 });
            addSeparator(t, 248, false, y, { 255, 255, 255, 150 });
            t.node->addChild(cap);
            m_rateLabel = CCLabelBMFont::create("100%", "bigFont.fnt");
            m_rateLabel->setScale(.34f);
            m_rateLabel->setAnchorPoint({ 0, .5f });
            m_rateLabel->setPosition({ 258, y - 8 });
            m_rateLabel->setString(fmt::format("{}%", (int)std::lround(m_rate * 100)).c_str());
            t.node->addChild(m_rateLabel);
            addRound(t, roundIcon("ta_clock.png"_spr, .62f), CircleBaseColor::Blue, { 304, y }, nullptr, [this] {
                static float const rates[] = { 1.f, .75f, .5f, .25f };
                int i = 0;
                while (i < 3 && std::abs(rates[i] - m_rate) > .01f) i++;
                m_rate = rates[(i + 1) % 4];
                Mod::get()->setSavedValue<double>("playback-speed", m_rate);
                m_rateLabel->setString(fmt::format("{}%", (int)std::lround(m_rate * 100)).c_str());
                applyRate();
                toast(fmt::format("Playback speed {}%", (int)std::lround(m_rate * 100)));
            }, 26.f);
        }
        desc(t,
            "<cy>Row 1 - Main</c>\n"
            "- <cg>Back arrows</c> - go to the start of the song.\n"
            "- <cg>Play</c> (<cy>Space</c>) - play / pause from the yellow cursor.\n"
            "- <cg>Forward arrows</c> - go to the end of the song.\n"
            "- <cg>Green arrow</c> - show the yellow cursor on the waveform.\n"
            "- <cl>Blue arrow</c> - show the Song Start line (turquoise) on the waveform.\n"
            "- <cg>Follow</c> (arrow + line) - green: the waveform moves with the music. Gray: it stays where it is.\n"
            "- <cl>Speed</c> + <cl>clock</c> - play slower: 100% > 75% > 50% > 25%. The pitch stays the same.\n"
            "<cy>Row 2 - Metronome</c>\n"
            "- <cg>BPM ON / OFF</c> - the metronome, here and in the editor.\n"
            "- <cg>SFX</c> (black note) - the sound: classic or wood.\n"
            "- <cp>1/N</c> (pink) - how often it clicks: 1/1 = every beat, 1/2 = also between the beats, ...\n"
            "- <cl>WAVE ON / OFF</c> (Editor Waveform) - the song's waveform behind the level in the editor.\n"
            "<cy>Row 3 - Song Start</c> (where the level's song starts, the turquoise line)\n"
            "- <cg>Cursor</c> - the song starts at the yellow cursor.\n"
            "- <cl>Gray back arrows</c> - the song starts at 0:00 again.\n"
            "- Or type the start in seconds into the field.\n"
            "<cy>Row 3 - Guidelines</c> (the beat lines in the editor)\n"
            "- <cg>Lines</c> - draw them (the old ones are replaced).\n"
            "- <cr>Crossed lines</c> - delete them.\n"
            "- <cp>1/N</c> (pink) - a line every 1/N of a beat (the same as the 1/N button in the editor).\n"
            "- <cg>Automatic Guidelines</c> - they are redrawn by themselves after every timing change.\n"
            "- Colors like osu!: white = beat, <cr>red</c> = 1/2, <cp>purple</c> = 1/3, <cl>blue</c> = 1/4, <cy>yellow</c> = 1/8.\n"
            "<cy>Sliders</c> (right) - the music and metronome volume.\n"
            "<cy>Top of the window</c>\n"
            "- <cg>Undo / Redo</c> (top left, <cy>Ctrl+Z</c> / <cy>Ctrl+Y</c>) - timing, rhythm, Song Start and 1/N.\n"
            "- <cp>Pink 1/N</c> - the waveform grid (where the cursor snaps).\n"
            "- <cy>Magnifiers</c> - zoom in / out.\n"
            "- <cy>Squares</c> (top right) - blink on every beat while the song plays.\n"
            "<cy>Waveform and mouse</c>\n"
            "- Click - move the cursor (it snaps to the grid, <cy>Alt</c> = free).\n"
            "- Drag - scroll. Drag the thin strip under it - jump anywhere.\n"
            "- <cy>Wheel</c> - next grid line. <cy>Shift + wheel</c> - scroll. <cy>Ctrl + wheel</c> - zoom.\n"
            "Colors and the editor buttons: the <cy>UI</c> button in the editor's pause menu.");
    }

    // ===== 2. Timing points (+ the BPM analysis on the right) =====
    {
        auto& t = addTab("Timing Points", 145, "ta_clock.png"_spr);
        float const YA = 94, YB = 70, YC = 46, YD = 22; // four rows
        float const RX = 362;                           // left of the analysis column

        // --- left: the selected timing point ("<  Point 1/3  >") ---
        row(t, 16, YA);
        addArrow(t, { 20, YA }, -1, [this] { selectPoint(std::max(0, m_selected - 1)); });
        m_pointLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_pointLabel->setScale(.48f);
        m_pointLabel->setPosition({ 62, YA });
        t.node->addChild(m_pointLabel);
        addArrow(t, { 104, YA }, 1, [this] {
            selectPoint(std::min((int)Session::get().map.points.size() - 1, m_selected + 1));
        });
        fx = 118;
        btn("+ Add", [this] { addPointAtCursor(); }, "GJ_button_01.png");
        btn("Delete", [this] { deletePoint(); }, "GJ_button_06.png");

        row(t, 16, YB);
        text("Time");
        m_timeInput = input(64, "ms", CommonFilter::Float);
        m_timeInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || m_selected < 0) return;
            if (auto v = numFromString<double>(str)) {
                Session::get().map.points[m_selected].time = *v;
                changed();
            }
        });
        btn("-10", [this] { shiftPoint(-10); });
        btn("-1", [this] { shiftPoint(-1); });
        btn("+1", [this] { shiftPoint(1); });
        btn("+10", [this] { shiftPoint(10); });
        fx += 4;
        m_allCheck = check("All points", [this](bool on) {
            m_shiftAll = on;
            toast(on ? "Nudge moves all points" : "Nudge moves the selected point");
        });

        row(t, 16, YC);
        text("BPM");
        m_bpmInput = input(64, "BPM", CommonFilter::Float);
        m_bpmInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || m_selected < 0) return;
            if (auto v = numFromString<double>(str); v && *v >= 10 && *v <= 1000) {
                Session::get().map.points[m_selected].setBpm(*v);
                changed();
            }
        });
        // multiply the BPM of the selected point (e.g. 180 -> x1.5 = 270, /1.5 = 120)
        auto factor = [&](char const* label, double f) {
            btn(label, [this, f] {
                if (m_selected < 0) return;
                Session::get().map.points[m_selected].beatLength /= f;
                changed();
            });
        };
        factor("x2", 2.0);
        factor("/2", 0.5);
        factor("x1.5", 1.5);
        factor("/1.5", 1 / 1.5);
        fx += 6;
        text("Meter");
        m_meterInput = input(40, "4", CommonFilter::Uint);
        m_meterInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || m_selected < 0) return;
            if (auto v = numFromString<int>(str); v && *v >= 1 && *v <= 16) {
                Session::get().map.points[m_selected].meter = *v;
                changed();
            }
        });

        row(t, 16, YD);
        btn("Snap to hit", [this] {
            auto& s = Session::get();
            if (m_selected < 0 || !s.envelope) return;
            auto& p = s.map.points[m_selected];
            p.time = std::round(nearestOnset(*s.envelope, p.time, p.beatLength * 0.25));
            changed();
            toast("Timing point moved to the nearest hit");
        });
        btn("Downbeat here", [this] {
            auto& s = Session::get();
            if (m_selected < 0) return;
            auto& p = s.map.points[m_selected];
            double k = std::round((m_cursor - p.time) / p.beatLength);
            int shift = (int)(((long long)k % p.meter + p.meter) % p.meter);
            p.time += shift * p.beatLength;
            changed();
            toast("The beat at the cursor is now beat 1 of the bar");
        });
        btn("Clear all points", [this] {
            createQuickPopup("Clear", "Delete <cr>all</c> timing points?", "Cancel", "Delete", [this](auto, bool yes) {
                if (!yes) return;
                Session::get().map.points.clear();
                m_selected = -1;
                changed();
            });
        }, "GJ_button_06.png");

        // --- right: find the BPM by itself or by tapping ---
        addSeparator(t, RX - 12);
        row(t, RX, YA);
        btn("Analyze!", [this] {
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
        }, "GJ_button_01.png");
        btn("Tap (T)", [this] { tap(); }, "GJ_button_02.png");
        row(t, RX, YB);
        text("Min");
        m_minBpmInput = input(52, "70", CommonFilter::Float);
        m_minBpmInput->setString(fmt::format("{:.0f}", savedDouble("min-bpm", 70)));
        m_minBpmInput->setCallback([](std::string const& s) {
            if (auto v = numFromString<double>(s); v && *v >= 30) saveSetting("min-bpm", *v);
        });
        fx += 4;
        text("Max");
        m_maxBpmInput = input(52, "230", CommonFilter::Float);
        m_maxBpmInput->setString(fmt::format("{:.0f}", savedDouble("max-bpm", 230)));
        m_maxBpmInput->setCallback([](std::string const& s) {
            if (auto v = numFromString<double>(s); v && *v <= 400) saveSetting("max-bpm", *v);
        });
        row(t, RX, YC);
        m_changesCheck = check("Tempo changes", [this](bool on) {
            saveSetting("detect-changes", on);
            toast(on ? "Tempo changes turned on" : "Tempo changes turned off");
        });
        desc(t,
            "<cr>Red lines</c> on the waveform are timing points. Each one sets the BPM and the beats per bar from its time on.\n"
            "<cy>Quick start</c>\n"
            "1. Load the song (Files tab).\n"
            "2. Press <cg>Analyze!</c>.\n"
            "3. Play it: the beat lines should sit on the peaks of the <co>orange line</c>. Fix the points if they don't.\n"
            "<cy>Left - the selected point</c>\n"
            "- <cy>Point 1/3</c> + arrows - choose a point. Or click its red line (drag to move it, <cy>Shift</c> = onto the nearest hit).\n"
            "- <cg>+ Add</c> (<cy>A</c>) - a new point at the cursor. <cr>Delete</c> (<cy>Del</c>) - remove the selected one.\n"
            "- <cl>Time</c> - its time in ms. <co>-10 / -1 / +1 / +10</c> nudge it.\n"
            "- <cl>All points</c> - the nudge moves the whole timing.\n"
            "- <cl>BPM</c> - its BPM. <co>x2 / /2 / x1.5 / /1.5</c> multiply it.\n"
            "- <cl>Meter</c> - beats per bar.\n"
            "- <cl>Snap to hit</c> - moves the point onto the closest hit.\n"
            "- <cl>Downbeat here</c> - the beat at the cursor becomes beat 1 of the bar.\n"
            "- <cr>Clear all points</c> - deletes every timing point.\n"
            "<cy>Right - finding the BPM</c>\n"
            "- <cg>Analyze!</c> - finds the BPM, tempo changes and the first beat. It <cr>replaces</c> the timing points.\n"
            "- <cl>Tap</c> (<cy>T</c>) - tap along with the music. After 4 taps the selected point gets that BPM.\n"
            "- <cl>Min / Max</c> - the BPM range. Is the result half or double? Narrow it, or use x2 / /2.\n"
            "- <cl>Tempo changes</c> - on: adds points where the tempo drifts (live drums). Off: one BPM for the whole song.\n"
            "<cy>Undo / Redo</c>: top left (<cy>Ctrl+Z</c> / <cy>Ctrl+Y</c>).");
    }

    // ===== 2b. Rhythm patterns =====
    {
        m_rhythmTab = (int)m_tabs.size();
        auto& t = addTab("Rhythm", 0, "ta_piano.png"_spr);
        float const YT = 94, YB = 66; // row 1: the rhythm points + all their settings, row 2: which bar is shown
        m_rhythmLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_rhythmLabel->setScale(.5f);
        m_rhythmLabel->setPosition({ 68, YT });
        t.node->addChild(m_rhythmLabel);
        addArrow(t, { 16, YT }, -1, [this] { selectRhythm(std::max(0, m_rhythmSel - 1)); });
        addArrow(t, { 120, YT }, 1, [this] {
            selectRhythm(std::min((int)Session::get().map.rhythm.size() - 1, m_rhythmSel + 1));
        });
        row(t, 140, YT);
        btn("+ New", [this] { addRhythmPoint(false); }, "GJ_button_01.png");
        btn("Normal grid", [this] { addRhythmPoint(true); });
        {
            // GD's trash button
            auto spr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
            spr->setScale(22.f / spr->getContentSize().height);
            auto b = CCMenuItemExt::createSpriteExtra(spr, [this](auto) { deleteRhythmPoint(); });
            b->setPosition({ fx + spr->getScaledContentSize().width / 2, fy });
            t.menu->addChild(b);
            fx += spr->getScaledContentSize().width + 5;
        }
        {
            auto b = addPill(t.menu, "From", "ta_note.png"_spr, "GJ_button_02.png", { 0, fy }, 0, 20,
                             [this] { rhythmFromAudio(); }, true);
            b->setPositionX(fx + b->getScaledContentSize().width / 2);
        }

        // right end of the same row: length of the pattern and how often it repeats ("<  1 BAR  >")
        auto selected = [this]() -> RhythmPoint* {
            auto& s = Session::get();
            if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return nullptr;
            return &s.map.rhythm[m_rhythmSel];
        };
        m_rhythmLenLabel = addArrowText(t, "1 BAR", { 414, YT }, nullptr, [this, selected](int dir) {
            auto r = selected();
            if (!r) return;
            static int const opts[] = { 1, 2, 4 };
            int i = 0;
            while (i < 3 && opts[i] != r->bars) i++;
            setRhythmBars(opts[((i + dir) % 3 + 3) % 3]);
        }, 28);
        // all = until the next rhythm point / BPM change
        m_rhythmLoopsLabel = addArrowText(t, "LOOP ALL", { 491, YT }, nullptr, [this, selected](int dir) {
            auto r = selected();
            if (!r || r->beats.empty()) return;
            static int const loops[] = { 0, 1, 2, 3, 4, 8 };
            int i = 0;
            while (i < 6 && loops[i] != r->loops) i++;
            r->loops = loops[((i + dir) % 6 + 6) % 6];
            changed();
        }, 30);

        // snapping of the rhythm points (dragging and + New) to the waveform grid
        row(t, 16, YB);
        m_rhythmSnapCheck = check("Snap to grid", [this](bool on) {
            saveSetting("rhythm-snap", on);
            toast(on ? "Rhythm snap to grid turned on" : "Rhythm snap to grid turned off");
        });
        // every square of the shown bar off at once (then click only the hits you want)
        addPill(t.menu, "Clear bar", nullptr, "GJ_button_06.png", { 178, YB }, 76, 18, [this] {
            auto& s = Session::get();
            if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
            int meter = rhythmMeter(m_rhythmSel);
            int beat = 0;
            for (auto& step : s.map.rhythm[m_rhythmSel].beats) {
                int inBar = beat - m_rhythmPage * meter;
                if (inBar >= 0 && inBar < meter) step.hits = step.accents = 0;
                beat += std::max(1, step.span);
            }
            changed();
        }, false, nullptr, .34f);

        // which bar of a longer pattern is shown below
        addArrow(t, { 380, YB }, -1, [this] {
            if (m_rhythmPage > 0) m_rhythmPage--;
            rebuildRhythmGrid();
        });
        m_rhythmPageLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_rhythmPageLabel->setScale(.42f);
        m_rhythmPageLabel->setPosition({ 440, YB });
        t.node->addChild(m_rhythmPageLabel);
        addArrow(t, { 500, YB }, 1, [this] {
            m_rhythmPage++;
            rebuildRhythmGrid();
        });

        // beats of the shown bar: snap button, hit squares and -/+ are built in rebuildRhythmGrid()
        auto beatsLbl = CCLabelBMFont::create("Beats", "goldFont.fnt");
        beatsLbl->setScale(.5f);
        beatsLbl->setPosition({ 34, 32 });
        t.node->addChild(beatsLbl);
        // shown instead of the grid when the selected point has no pattern
        m_rhythmEmptyLabel = CCLabelBMFont::create("No pattern here. Press + New to make one, or select a rhythm point.", "chatFont.fnt");
        m_rhythmEmptyLabel->setScale(.55f);
        m_rhythmEmptyLabel->setColor({ 255, 230, 160 });
        m_rhythmEmptyLabel->setPosition({ 270, 32 });
        t.node->addChild(m_rhythmEmptyLabel);
        m_rhythmGrid = CCMenu::create();
        m_rhythmGrid->setPosition({ 0, 0 });
        t.node->addChild(m_rhythmGrid);
        desc(t,
            "A <cp>rhythm pattern</c> plays its own rhythm inside one BPM (for example 1/4, then fast 1/6), with no extra timing points.\n"
            "<cy>Quick start</c>\n"
            "1. Put the yellow cursor where the rhythm starts.\n"
            "2. Press <cg>+ New</c>.\n"
            "3. Set the snap of each beat and click the squares you want.\n"
            "<cy>Row 1</c>\n"
            "- <cy>Rhythm 1/2</c> + arrows - choose a rhythm point. Or click its purple line on the waveform (drag to move it).\n"
            "- <cg>+ New</c> - a new pattern at the cursor.\n"
            "- <cl>Normal grid</c> - back to the normal grid from the cursor on.\n"
            "- <cr>Trash</c> (<cy>Del</c>) - deletes the selected rhythm point.\n"
            "- <cl>From</c> (note) - fills the pattern from the hits in the song. Check it by ear.\n"
            "- <cl>1 BAR</c> - the length of the pattern: 1, 2 or 4 bars.\n"
            "- <cl>LOOP ALL</c> - how many times it repeats (ALL = until the next rhythm point or BPM change).\n"
            "<cy>Row 2</c>\n"
            "- <cl>Snap to grid</c> - rhythm points jump to the waveform grid. Off = they move freely (on: hold <cy>Alt</c> for free).\n"
            "- <cr>Clear bar</c> - turns off every square of the shown bar.\n"
            "- <cy>Bar 1/2</c> + arrows - which bar of a longer pattern is shown.\n"
            "<cy>Beats</c> (one column per beat)\n"
            "- <cy>1/4</c> above a beat - its snap (1/1 ... 1/16). Use the arrows or click it.\n"
            "- Squares - click: gray (nothing) > <cp>purple</c> (quiet hit) > <cp>pink</c> (loud hit).\n"
            "- <cr>X</c> - no hits on this beat.\n"
            "- <cr>-</c> - joins the beat with the one before (one long beat). <cg>+</c> - splits it again.\n"
            "Inside a pattern the metronome and the guidelines follow the pattern, not the 1/N buttons.");
    }

    // ===== 4. Files (song, osu! timing, level backup) =====
    {
        auto& t = addTab("Files", 318, "ta_folder.png"_spr);
        float const YA = 94, YB = 70, YC = 46, YD = 22; // four rows, the name of each on the left
        float const LX = 104, BW = 128, BH = 20;        // right edge of the names, size of the buttons
        // the names end at the same distance from their buttons; the buttons form equal columns
        auto name = [&](Tab& tab, float y, char const* label) {
            row(tab, 16, y);
            auto l = text(label);
            l->setAnchorPoint({ 1, .5f });
            l->setPositionX(LX);
            fx = LX + 10;
        };
        auto fileBtn = [&](char const* label, char const* iconName, geode::Function<void()> cb,
                           char const* bg = "GJ_button_04.png") {
            auto b = addPill(ft->menu, label, iconName, bg, { fx + BW / 2, fy }, BW, BH, std::move(cb));
            fx += BW + 6;
            return b;
        };
        name(t, YA, "Song file");
        fileBtn("Load audio file...", "ta_load_audio.png"_spr, [this] { pickAudio(); }, "GJ_button_01.png");
        fileBtn("Load level's song", "ta_note_down.png"_spr, [this] {
            auto p = Session::currentLevelSongPath();
            if (p.empty()) Notification::create("This level has no downloaded song", NotificationIcon::Error)->show();
            else handleDroppedFile(p);
        }, "GJ_button_01.png");
        name(t, YB, "In the level");
        fileBtn("Use as level song", "ta_use_song.png"_spr, [this] {
            auto& s = Session::get();
            int id = s.useAudioInLevel();
            if (id <= 0) {
                Notification::create(s.audioPath.empty() ? "Load an audio file first" : "Open this from a level's song selection",
                    NotificationIcon::Error)->show();
                return;
            }
            Notification::create(fmt::format("Level now plays your file (song ID {})", id), NotificationIcon::Success)->show();
        }, "GJ_button_01.png");
        fileBtn("Restore original song", "ta_restore.png"_spr, [] {
            bool ok = Session::get().restoreLevelSong();
            Notification::create(ok ? "Original song restored" : "This level has no local song",
                ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
        }, "GJ_button_06.png");
        name(t, YC, "osu! timing");
        fileBtn("Import from .osu", "ta_import.png"_spr, [this] { pickOsu(); }, "GJ_button_01.png");
        fileBtn("Export to .osu", "ta_export.png"_spr, [this] { exportOsu(); });
        name(t, YD, "Level backup");
        fileBtn("Import backup", "ta_folder.png"_spr, [this] {
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
            setCheck(m_changesCheck, savedBool("detect-changes", true));
            setCheck(m_rhythmSnapCheck, savedBool("rhythm-snap", true));
            setRoundText(m_halfLabel, fmt::format("1/{}", Session::get().guideDivisor()));
            setCheck(m_metroCheck, Session::get().editorMetronome());
            setCheck(m_waveCheck, Session::get().editorWaveform());
            refreshPanel();
            Notification::create("Level settings imported", NotificationIcon::Success)->show();
        }, "GJ_button_01.png");
        fileBtn("Export backup", "ta_save.png"_spr, [] {
            auto& s = Session::get();
            auto name = Session::levelName();
            if (name.empty()) name = "level";
            if (auto p = nativePick(true, L"Geometry Dash Timing Analyzer level\0*.json\0", std::filesystem::path(name + ".gdta.json"))) {
                bool ok = s.exportLevel(*p);
                Notification::create(ok ? "Level settings exported" : "Save failed",
                    ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
            }
        });
        fx += 14;
        fileBtn("Unload everything", "ta_warning.png"_spr, [this] {
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
        desc(t,
            "<cy>Song file</c>\n"
            "- <cg>Load audio file...</c> - mp3 / ogg / wav / flac. Or drag the file onto the game window.\n"
            "- <cg>Load level's song</c> - the level's own song (a Jukebox NONG too).\n"
            "<cy>In the level</c>\n"
            "- <cg>Use as level song</c> - the level plays the loaded file (only on your PC).\n"
            "- <cr>Restore original song</c> - the original song again.\n"
            "<cy>osu! timing</c>\n"
            "- <cg>Import from .osu</c> - takes the timing from an osu! beatmap. Dragging a .osu file onto the game works too.\n"
            "- <cl>Export to .osu</c> - saves your timing as an osu! [TimingPoints] section.\n"
            "<cy>Level backup</c> (.json file)\n"
            "- <cg>Import backup</c> - loads a backup (for example from another PC).\n"
            "- <cl>Export backup</c> - saves this level's timing, rhythm, settings and Song Start.\n"
            "<cr>Unload everything</c> - removes the song, the waveform and all timing points of this level (the saved ones too).");
    }

    // ===== 5. Triggers (pulses / spawns on the beats, put into the level editor) =====
    // added last: the tab index is part of the saved "last-tab" and the Move UI ids
    {
        m_triggerTab = (int)m_tabs.size();
        auto& t = addTab("Triggers", 0, "ta_pulse.png"_spr);
        float const YA = 94, YB = 70, YC = 46, YD = 22;
        loadTriggerSettings();
        auto stepper = [&](char const* value, float half, geode::Function<void(int)> cb) {
            auto l = addArrowText(t, value, { fx + half + 6, fy }, nullptr, std::move(cb), half);
            fx += half * 2 + 20;
            return l;
        };
        auto cursorBtn = [&](geode::Function<void()> cb) {
            auto b = addRound(t, roundIcon("ta_cursor.png"_spr, .62f), CircleBaseColor::Green, { fx + 11, fy }, nullptr,
                              std::move(cb), 22.f);
            fx += 27;
            return b;
        };
        // everything built inside `build` is shown only for one kind of trigger
        auto only = [&](std::vector<CCNode*>& into, auto&& build) {
            unsigned n0 = t.node->getChildrenCount(), m0 = t.menu->getChildrenCount();
            build();
            for (unsigned i = n0; i < t.node->getChildrenCount(); i++)
                into.push_back(static_cast<CCNode*>(t.node->getChildren()->objectAtIndex(i)));
            for (unsigned i = m0; i < t.menu->getChildrenCount(); i++)
                into.push_back(static_cast<CCNode*>(t.menu->getChildren()->objectAtIndex(i)));
        };
        auto fmtSec = [](double ms) { return ms < 0 ? std::string() : fmt::format("{:.3f}", ms / 1000.0); };
        // the squares of the steps and the palette
        m_trigMenu = CCMenu::create();
        m_trigMenu->setPosition({ 0, 0 });
        t.node->addChild(m_trigMenu);

        // row 1: what (the picture of the trigger), how and how often
        row(t, 16, YA);
        text("Make");
        {
            // pulse > spawn > color
            auto flip = [this](int dir) {
                m_trig.kind = (triggers::Kind)((((int)m_trig.kind + dir) % 3 + 3) % 3);
                saveTriggerSettings();
                refreshTriggerTab();
            };
            float x0 = fx;
            addArrow(t, { x0 + 8, fy }, -1, [flip] { flip(-1); });
            m_trigKindIcon = CCSprite::create();
            m_trigKindIcon->setPosition({ x0 + 30, fy });
            t.node->addChild(m_trigKindIcon);
            addArrow(t, { x0 + 52, fy }, 1, [flip] { flip(1); });
            fx = x0 + 66;
        }
        text("As");
        m_trigPlaceLabel = stepper("ON BEATS", 40, [this](int) {
            m_trig.placement = m_trig.placement == triggers::Placement::OnBeats ? triggers::Placement::Loop
                                                                              : triggers::Placement::OnBeats;
            saveTriggerSettings();
            refreshTriggerTab();
        });
        text("Every");
        m_trigEveryLabel = stepper("1/1", 22, [this](int dir) {
            m_trig.every = ((m_trig.every + dir) % triggers::kEveryCount + triggers::kEveryCount) % triggers::kEveryCount;
            saveTriggerSettings();
            refreshTriggerTab();
        });

        // row 2: which part of the song
        row(t, 16, YB);
        text("Start");
        m_trigFromInput = input(54, "start", CommonFilter::Float);
        m_trigFromInput->setString(fmtSec(m_trig.fromMs));
        m_trigFromInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs) return;
            auto v = numFromString<double>(str);
            m_trig.fromMs = v && *v >= 0 ? *v * 1000.0 : -1;
            saveTriggerSettings();
            m_trigTimesAt = -10;
        });
        cursorBtn([this] {
            m_trig.fromMs = std::max(0.0, m_playing ? playPosition() : m_cursor);
            saveTriggerSettings();
            refreshTriggerTab();
            toast(fmt::format("Triggers start at {}", fmtTime(m_trig.fromMs)));
        });
        text("End");
        m_trigToInput = input(54, "end", CommonFilter::Float);
        m_trigToInput->setString(fmtSec(m_trig.toMs));
        m_trigToInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs) return;
            auto v = numFromString<double>(str);
            m_trig.toMs = v && *v >= 0 ? *v * 1000.0 : -1;
            saveTriggerSettings();
            m_trigTimesAt = -10;
        });
        cursorBtn([this] {
            m_trig.toMs = std::max(0.0, m_playing ? playPosition() : m_cursor);
            saveTriggerSettings();
            refreshTriggerTab();
            toast(fmt::format("Triggers end at {}", fmtTime(m_trig.toMs)));
        });

        // row 3: how many steps the cycle has and the steps themselves (click one to edit it below)
        row(t, 16, YC);
        text("Steps");
        m_trigStepsLabel = stepper("1", 20, [this](int dir) {
            m_trig.steps = ((m_trig.steps - 1 + dir) % triggers::kMaxSteps + triggers::kMaxSteps) % triggers::kMaxSteps + 1;
            m_trigSel = std::min(m_trigSel, m_trig.steps - 1);
            saveTriggerSettings();
            refreshTriggerTab();
        });
        m_trigStepsX = fx + 2;

        // row 4: the selected step. Pulse: its color (and what the pulses flash). Spawn: the groups it spawns.
        row(t, 16, YD);
        only(m_pulseOnly, [&] {
            text("Color");
            m_trigPalX = fx + 2;
            fx = m_trigPalX + 100;
            text("On");
            m_trigTargetLabel = stepper("BG", 26, [this](int dir) {
                int n = triggers::targetCount();
                m_trig.target = ((m_trig.target + dir) % n + n) % n;
                saveTriggerSettings();
                refreshTriggerTab();
            });
        });
        only(m_spawnOnly, [&] {
            fx = 16; // the same row again (the pulse part moved fx)
            text("Groups");
            m_trigGroupsInput = input(110, "1,5", CommonFilter::Any);
            m_trigGroupsInput->setMaxCharCount(24);
            m_trigGroupsInput->setCallback([this](std::string const& str) {
                if (m_updatingInputs) return;
                m_trig.step[std::clamp(m_trigSel, 0, triggers::kMaxSteps - 1)].groups = str;
                saveTriggerSettings();
                m_trigDirty = true;
            });
            auto hint = CCLabelBMFont::create("spawned together: 1,5,12", "chatFont.fnt");
            hint->setScale(.42f);
            hint->setAnchorPoint({ 0, .5f });
            hint->setColor({ 200, 200, 200 });
            hint->setPosition({ fx, fy });
            t.node->addChild(hint);
        });

        // right: a ready cycle to start from, how many + the button that puts them into the level
        addPill(t.menu, "Preset", nullptr, "GJ_button_04.png", { 466, 88 }, 112, 20, [this] {
            m_trigPreset = (m_trigPreset + 1) % triggers::presetCount();
            triggers::applyPreset(m_trig, m_trigPreset);
            m_trigSel = 0;
            saveTriggerSettings();
            refreshTriggerTab();
        }, false, &m_trigPresetLabel, .3f);
        m_trigCountLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_trigCountLabel->setScale(.34f);
        m_trigCountLabel->setPosition({ 466, 58 });
        t.node->addChild(m_trigCountLabel);
        addPill(t.menu, "Place triggers", "ta_pulse.png"_spr, "GJ_button_01.png", { 466, 25 }, 112, 26,
                [this] { placeTriggers(); }, false, nullptr, .36f);

        desc(t,
            "Puts <cy>triggers</c> into your level, exactly on the beats.\n"
            "<cy>Quick start</c>\n"
            "1. Choose what to make and how often (row 1).\n"
            "2. Pick a <cl>Preset</c> or set the steps yourself.\n"
            "3. Scroll the editor to the height you want and press <cg>Place triggers</c>.\n"
            "<cy>Row 1</c>\n"
            "- <cl>Make</c> - a <cy>pulse</c>, <cy>spawn</c> or <cy>color</c> trigger (color = a color change right away, no fade).\n"
            "- <cl>As</c> - <cy>ON BEATS</c>: one trigger on every beat. <cy>SPAWN LOOP</c>: one loop that repeats itself (fewer objects), with a stop trigger at the end.\n"
            "- <cl>Every</c> - <cy>BAR</c>, <cy>1/1</c> (every beat) ... <cy>1/8</c>, or <cy>HITS</c> (the hits in the song). Rhythm patterns are followed.\n"
            "<cy>Row 2</c>\n"
            "- <cl>Start / End</c> - the part of the song, in seconds. The <cg>cursor</c> button takes the yellow cursor.\n"
            "- Empty = from the start / to the end. The part is green on the waveform.\n"
            "<cy>Row 3 - Steps</c>\n"
            "- The squares are what the triggers do one after another, then again from the first one.\n"
            "- Example: 1/4 with two steps, pink and blue = pink, blue, pink, blue ...\n"
            "- The arrows set how many steps (1 - 8). Click a square to edit it in row 4.\n"
            "<cy>Row 4 - the selected step</c>\n"
            "- Pulse / color: <cl>Color</c> - the swatch opens the color picker. <cg>+</c> = brighten the channel's own color. <cr>X</c> = nothing on this step.\n"
            "- <cl>On</c> - what changes color: BG, GROUND, OBJECTS or LINE.\n"
            "- Spawn: <cl>Groups</c> - the groups it spawns (\"1,5,12\"). Empty = nothing.\n"
            "<cy>Right</c>\n"
            "- <cl>Preset</c> - ready steps: every beat, two colors, alternate, accent, rainbow.\n"
            "- The text above the button - how many triggers will be placed.\n"
            "- <cg>Place triggers</c> - puts them at the editor's current height. <cy>One Ctrl+Z</c> removes them all.");
    }

    setCheck(m_metroCheck, Session::get().editorMetronome());
    setCheck(m_allCheck, m_shiftAll);
    setCheck(m_changesCheck, savedBool("detect-changes", true));
    setCheck(m_rhythmSnapCheck, savedBool("rhythm-snap", true));
    setRoundText(m_halfLabel, fmt::format("1/{}", Session::get().guideDivisor()));

    // "Level: ... / osu! timing: ..." on the right of the Song line (text set in refreshPanel())
    m_osuLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_osuLabel->setScale(.42f);
    m_osuLabel->setOpacity(170);
    m_osuLabel->setAnchorPoint({ 1, .5f });
    m_osuLabel->setPosition({ POP_W - 16, POP_H - 33 });
    m_mainLayer->addChild(m_osuLabel);

    // help of the current tab (top bar: down in the corner it covered the Unload / Place buttons)
    {
        auto spr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        spr->setScale(.6f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [this](auto) {
            if (m_tab >= 0 && m_tab < (int)m_tabs.size() && !m_tabs[m_tab].help.empty())
                MDPopup::create(fmt::format("Help - {}", m_tabs[m_tab].label->getString()), m_tabs[m_tab].help, "OK")->show();
        });
        btn->setPosition({ 455, POP_H - 15 }); // in the top bar, between the magnifiers and the beat squares
        m_menu->addChild(btn);
        m_helpBtn = btn;
    }
    // undo / redo of everything done in the window (timing, rhythm, song start, 1/N), next to the close button
    {
        auto hist = [&](char const* iconName, float x, geode::Function<void()> cb) {
            auto spr = CircleButtonSprite::create(roundIcon(iconName, .62f), CircleBaseColor::Green, CircleBaseSize::Small);
            spr->setScale(22.f / spr->getContentSize().height);
            auto b = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
            b->setPosition({ x, POP_H - 15 });
            m_menu->addChild(b);
        };
        hist("ta_undo.png"_spr, 42, [this] { undo(); });
        hist("ta_redo.png"_spr, 68, [this] { redo(); });
        // zoom of the waveform (also Ctrl + wheel), right of the title
        auto zoomBtn = [&](char const* name, float x, geode::Function<void()> cb) {
            auto spr = CCSprite::create(name);
            spr->setScale(24.f / spr->getContentSize().height);
            auto b = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) mutable { cb(); });
            b->setPosition({ x, POP_H - 15 });
            m_menu->addChild(b);
        };
        // the waveform grid: how many lines per beat the waveform shows and the cursor / rhythm points snap to
        {
            m_divisor = std::clamp(Mod::get()->getSavedValue<int>("wave-grid", 4), 1, 16);
            m_divLabel = CCLabelBMFont::create(fmt::format("1/{}", m_divisor).c_str(), "bigFont.fnt");
            m_divLabel->setScale(.45f);
            auto spr = CircleButtonSprite::create(m_divLabel, CircleBaseColor::Pink, CircleBaseSize::Small);
            spr->setScale(24.f / spr->getContentSize().height);
            auto b = CCMenuItemExt::createSpriteExtra(spr, [this](auto) {
                static int const divs[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
                int i = 0;
                while (i < 7 && divs[i] != m_divisor) i++;
                m_divisor = divs[(i + 1) % 8];
                Mod::get()->setSavedValue<int>("wave-grid", m_divisor);
                m_divLabel->setString(fmt::format("1/{}", m_divisor).c_str());
                toast(fmt::format("Waveform grid 1/{}", m_divisor));
            });
            b->setPosition({ 372, POP_H - 15 });
            m_menu->addChild(b);
        }
        zoomBtn("ta_zoom_out.png"_spr, 399, [this] {
            zoom(1.5, m_viewStart + WW * m_msPerPx / 2);
            toast("Zoomed out");
        });
        zoomBtn("ta_zoom_in.png"_spr, 426, [this] {
            zoom(1 / 1.5, m_viewStart + WW * m_msPerPx / 2);
            toast("Zoomed in");
        });
    }
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
    (void)Mod::get()->saveData();
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
    double t = savedBool("rhythm-snap", true) ? s.map.snap(m_cursor, m_divisor) : m_cursor;
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
    toast(normal ? "Normal grid from here" : "Rhythm pattern added");
    switchTab(m_rhythmTab);
}

void TimingEditor::deleteRhythmPoint() {
    auto& s = Session::get();
    if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
    s.map.rhythm.erase(s.map.rhythm.begin() + m_rhythmSel);
    if (m_rhythmSel >= (int)s.map.rhythm.size()) m_rhythmSel = (int)s.map.rhythm.size() - 1;
    changed();
    toast("Rhythm point deleted");
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
    if (m_rhythmEmptyLabel) m_rhythmEmptyLabel->setVisible(!valid);
    if (!valid) return;

    auto const& beats = s.map.rhythm[m_rhythmSel].beats;
    float x0 = 64, x1 = POP_W - 26;
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
        // snap of the beat: "<  1/4  >" (clicking the value = next one)
        auto setSnap = [this, bi](int dir) {
            auto& s = Session::get();
            if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
            auto& step = s.map.rhythm[m_rhythmSel].beats[bi];
            // every number of parts 1 ... 16, odd ones too (1/5, 1/7 ...)
            int nd = ((std::clamp(step.divisor, 1, 16) - 1 + dir) % 16 + 16) % 16 + 1;
            // a new snap starts with every part playing (click squares to remove hits)
            step.divisor = nd;
            step.hits = (1u << nd) - 1;
            step.accents = defaultAccents(nd, std::max(1, step.span));
            changed();
        };
        auto snapLbl = CCLabelBMFont::create(fmt::format("1/{}", b.divisor).c_str(), "bigFont.fnt");
        snapLbl->setScale(.4f);
        auto snapItem = CCMenuItemExt::createSpriteExtra(snapLbl, [setSnap](auto) { setSnap(1); });
        snapItem->setPosition({ cx, 46 });
        m_rhythmGrid->addChild(snapItem);
        addArrowTo(m_rhythmGrid, { cx - 20, 46 }, -1, [setSnap] { setSnap(-1); });
        addArrowTo(m_rhythmGrid, { cx + 20, 46 }, 1, [setSnap] { setSnap(1); });
        // "-" = remove this beat, the step before gets longer (not across a bar line)
        if (inBar > 0)
            addButton(m_rhythmGrid, "-", { cx - 40, 46 }, [this, bi] {
                auto& s = Session::get();
                if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size() || bi < 1) return;
                auto& steps = s.map.rhythm[m_rhythmSel].beats;
                steps[bi - 1].span = std::max(1, steps[bi - 1].span) + std::max(1, steps[bi].span);
                steps.erase(steps.begin() + bi);
                changed();
            }, "GJ_button_06.png");
        // "+" = split the last beat off again
        if (span > 1)
            addButton(m_rhythmGrid, "+", { cx + 40, 46 }, [this, bi] {
                auto& s = Session::get();
                if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
                auto& steps = s.map.rhythm[m_rhythmSel].beats;
                if (steps[bi].span < 2) return;
                steps[bi].span--;
                steps.insert(steps.begin() + bi + 1, RhythmBeat{});
                changed();
            }, "GJ_button_01.png");
        // "X" = this beat without any hit (the "+" of a stretched beat is in the way, so it goes one place further)
        if (colW * span >= 100)
            addButton(m_rhythmGrid, "X", { cx + (span > 1 ? 62.f : 40.f), 46 }, [this, bi] {
                auto& s = Session::get();
                if (m_rhythmSel < 0 || m_rhythmSel >= (int)s.map.rhythm.size()) return;
                auto& step = s.map.rhythm[m_rhythmSel].beats[bi];
                step.hits = step.accents = 0;
                changed();
            }, "GJ_button_06.png");
        float cw = w / b.divisor;
        for (int j = 0; j < b.divisor; j++) {
            bool on = b.hits & (1u << j), accent = b.accents & (1u << j);
            // pink = accented hit (louder), purple = quiet hit, gray = no hit
            ccColor3B col = on ? (accent ? ccColor3B{ 255, 150, 235 } : ccColor3B{ 170, 80, 255 }) : ccColor3B{ 70, 70, 85 };
            // drawn 4x larger and scaled down so the rounded corners stay small
            auto cell = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
            cell->setContentSize({ std::max(8.f, (cw - 2) * 4), 24 * 4 });
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
            item->setPosition({ left + 5 + cw * (j + .5f), 21 });
            m_rhythmGrid->addChild(item);
        }
    }
}

// ---------------- undo / redo ----------------

static std::string historyKeyNow() {
    auto& s = Session::get();
    return utils::string::pathToString(s.audioPath) + "|" + Session::levelKey();
}

std::string TimingEditor::Snapshot::key() const {
    return map.serialize() + fmt::format("|{:.4f}|{}", songOffset, guideDivisor);
}

TimingEditor::Snapshot TimingEditor::snapshot() const {
    auto& s = Session::get();
    Snapshot snap;
    snap.map = s.map;
    if (auto ls = Session::levelSettings()) snap.songOffset = ls->m_songOffset;
    snap.guideDivisor = s.guideDivisor();
    return snap;
}

void TimingEditor::restore(Snapshot const& snap) {
    auto& s = Session::get();
    s.map = snap.map;
    if (auto ls = Session::levelSettings(); ls && snap.songOffset >= 0 && std::abs(ls->m_songOffset - snap.songOffset) > 1e-4)
        s.setSongStartOffset(snap.songOffset * 1000.0);
    if (snap.guideDivisor != s.guideDivisor()) {
        s.setGuideDivisor(snap.guideDivisor);
        setRoundText(m_halfLabel, fmt::format("1/{}", snap.guideDivisor));
    }
}

void TimingEditor::resetHistory() {
    m_undo.clear();
    m_redo.clear();
    m_lastMap = snapshot();
    m_historyKey = historyKeyNow();
}

void TimingEditor::recordHistory() {
    auto& s = Session::get();
    // another song / level -> its own history
    if (m_historyKey != historyKeyNow()) return resetHistory();
    auto now = snapshot();
    if (now.key() == m_lastMap.key()) return;
    // typing a value or clicking +1 several times in a row is one step (adding / deleting never merges)
    auto const& last = m_lastMap.map;
    bool merge = m_clock - m_lastRecord < 0.6 && !m_undo.empty() &&
                 last.points.size() == s.map.points.size() && m_undo.back().map.points.size() == s.map.points.size() &&
                 last.rhythm.size() == s.map.rhythm.size() && m_undo.back().map.rhythm.size() == s.map.rhythm.size() &&
                 m_lastMap.guideDivisor == now.guideDivisor;
    if (!merge) {
        m_undo.push_back(m_lastMap);
        if (m_undo.size() > 200) m_undo.erase(m_undo.begin());
    }
    m_redo.clear();
    m_lastMap = std::move(now);
    m_lastRecord = m_clock;
}

void TimingEditor::undo() {
    auto& s = Session::get();
    recordHistory();
    if (m_undo.empty()) {
        Notification::create("Nothing to undo", NotificationIcon::Info)->show();
        return;
    }
    m_redo.push_back(snapshot());
    restore(m_undo.back());
    m_undo.pop_back();
    toast("Undone");
    m_lastMap = snapshot();
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
    m_undo.push_back(snapshot());
    restore(m_redo.back());
    m_redo.pop_back();
    toast("Redone");
    m_lastMap = snapshot();
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
    opt::set<int64_t>("metronome-volume", v);
    m_metroVolLabel->setString(fmt::format("Metronome: {}%", v).c_str());
}

void TimingEditor::refreshPanel() {
    auto& s = Session::get();
    // what is loaded: the song (and where it comes from) + whether the timing comes from an .osu file
    auto osu = s.osuTimingName();
    m_fileLabel->setString(fmt::format("Song: {}", s.songDescription()).c_str());
    limitNodeWidth(m_fileLabel, 270, .5f, .2f);
    if (m_osuLabel) {
        auto level = Session::levelName();
        m_osuLabel->setString(fmt::format("{}  /  osu! timing: {}",
            level.empty() ? std::string("Drop an audio or .osu file here") : fmt::format("Level: {}", level),
            osu.empty() ? "no" : fmt::format("yes ({})", osu)).c_str());
        limitNodeWidth(m_osuLabel, 225, .42f, .2f);
    }

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
            setRoundText(m_rhythmLenLabel, r.beats.empty() ? std::string("-")
                : fmt::format("{} BAR{}", r.bars, r.bars > 1 ? "S" : ""));
            setRoundText(m_rhythmLoopsLabel, r.beats.empty() ? std::string("-")
                : r.loops == 0 ? std::string("LOOP ALL") : fmt::format("LOOP x{}", r.loops));
        } else {
            m_rhythmLabel->setString("No rhythm");
            setRoundText(m_rhythmLenLabel, "-");
            setRoundText(m_rhythmLoopsLabel, "-");
        }
    }
    // the grid itself is rebuilt from update() (its buttons call this, they must not delete themselves)
}

// ---------------- triggers tab ----------------

// per level: color channels and groups belong to one level
void TimingEditor::loadTriggerSettings() {
    auto& t = m_trig;
    auto& s = Session::get();
    t.kind = (triggers::Kind)std::clamp((int)savedDouble("trig-kind", 0), 0, 2);
    t.placement = savedDouble("trig-loop", 0) == 1 ? triggers::Placement::Loop : triggers::Placement::OnBeats;
    t.every = std::clamp((int)savedDouble("trig-every", 1), 0, triggers::kEveryCount - 1);
    t.fromMs = savedDouble("trig-from", -1);
    t.toMs = savedDouble("trig-to", -1);
    t.target = std::clamp((int)savedDouble("trig-target", 0), 0, triggers::targetCount() - 1);
    t.steps = std::clamp((int)savedDouble("trig-steps", 1), 1, triggers::kMaxSteps);
    m_trigPreset = std::clamp((int)savedDouble("trig-preset", 0), 0, triggers::presetCount() - 1);
    // "0,3,9" and "1|1,5||2"
    auto colors = utils::string::split(s.setting("trig-colors", "").asString().unwrapOr(""), ",");
    auto groups = utils::string::split(s.setting("trig-groups", "").asString().unwrapOr(""), "|");
    for (int i = 0; i < triggers::kMaxSteps; i++) {
        t.step[i].color = 0;
        t.step[i].groups = "1";
        if (i < (int)colors.size())
        {
            // "1:FF5AC8" (older versions saved just "3")
            auto const& c = colors[i];
            auto colon = c.find(':');
            if (auto v = numFromString<int>(c.substr(0, colon)))
                t.step[i].color = std::clamp(*v, 0, triggers::paletteCount() - 1);
            if (colon != std::string::npos)
                if (auto v = numFromString<unsigned>(c.substr(colon + 1), 16)) t.step[i].rgb = *v & 0xFFFFFF;
        }
        if (i < (int)groups.size()) t.step[i].groups = groups[i];
    }
}

void TimingEditor::saveTriggerSettings() {
    auto const& t = m_trig;
    saveSetting("trig-kind", (int)t.kind);
    saveSetting("trig-loop", t.placement == triggers::Placement::Loop ? 1 : 0);
    saveSetting("trig-every", t.every);
    saveSetting("trig-from", t.fromMs);
    saveSetting("trig-to", t.toMs);
    saveSetting("trig-target", t.target);
    saveSetting("trig-steps", t.steps);
    saveSetting("trig-preset", m_trigPreset);
    std::string colors, groups;
    for (int i = 0; i < triggers::kMaxSteps; i++) {
        colors += fmt::format("{}:{:06X},", t.step[i].color, t.step[i].rgb & 0xFFFFFF);
        std::string g;
        for (char c : t.step[i].groups)
            if ((c >= '0' && c <= '9') || c == ',') g += c;
        groups += g + "|";
    }
    saveSetting("trig-colors", colors);
    saveSetting("trig-groups", groups);
}

void TimingEditor::refreshTriggerTab() {
    if (m_triggerTab < 0 || m_triggerTab >= (int)m_tabs.size() || !m_trigPlaceLabel) return;
    bool pulse = m_trig.kind != triggers::Kind::Spawn, loop = m_trig.placement == triggers::Placement::Loop;
    bool active = m_tab == m_triggerTab && !m_layoutEdit;
    // the settings of the other kind are hidden (and their fields can not be clicked)
    auto show = [&](std::vector<CCNode*> const& nodes, bool on) {
        for (auto n : nodes) {
            n->setVisible(on);
            if (auto in = typeinfo_cast<TextInput*>(n)) in->setEnabled(on && active);
        }
    };
    show(m_pulseOnly, pulse);
    show(m_spawnOnly, !pulse);
    // the picture of the trigger (GD's own toolbar button)
    if (m_trigKindIcon) {
        if (auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
            m_trig.kind == triggers::Kind::Pulse ? "edit_ePulseBtn_001.png"
            : m_trig.kind == triggers::Kind::Spawn ? "edit_eSpawnBtn_001.png" : "edit_eTintCol01Btn_001.png")) {
            m_trigKindIcon->setDisplayFrame(frame);
            m_trigKindIcon->setScale(30.f / std::max(1.f, m_trigKindIcon->getContentSize().height));
        }
    }
    setRoundText(m_trigPlaceLabel, loop ? "SPAWN LOOP" : "ON BEATS");
    setRoundText(m_trigEveryLabel, triggers::everyName(m_trig.every));
    setRoundText(m_trigStepsLabel, fmt::format("{}", m_trig.steps));
    setRoundText(m_trigTargetLabel, triggers::targetName(m_trig.target));
    if (m_trigPresetLabel) {
        m_trigPresetLabel->setString(fmt::format("Preset: {}", triggers::presetName(m_trigPreset)).c_str());
        m_trigPresetLabel->limitLabelWidth(100, .3f, .12f);
    }
    auto sec = [](double ms) { return ms < 0 ? std::string() : fmt::format("{:.3f}", ms / 1000.0); };
    m_updatingInputs = true;
    if (m_trigFromInput && !m_trigFromInput->getInputNode()->m_selected) m_trigFromInput->setString(sec(m_trig.fromMs));
    if (m_trigToInput && !m_trigToInput->getInputNode()->m_selected) m_trigToInput->setString(sec(m_trig.toMs));
    m_updatingInputs = false;
    m_trigTimesAt = -10; // the count / preview is recalculated in update()
    m_trigDirty = true;  // so are the squares
}

// the squares of the steps (row 3) and the color palette of the selected step (row 4); rebuilt from update() so a
// button never deletes itself
void TimingEditor::rebuildTriggerSteps() {
    m_trigDirty = false;
    if (!m_trigMenu) return;
    m_trigMenu->removeAllChildren();
    bool pulse = m_trig.kind != triggers::Kind::Spawn;
    bool colorKind = m_trig.kind == triggers::Kind::Color;
    int K = std::clamp(m_trig.steps, 1, triggers::kMaxSteps);
    m_trigSel = std::clamp(m_trigSel, 0, K - 1);

    // a colored square (drawn 4x larger and scaled down so the corners stay small), outlined when it is selected
    auto square = [&](float w, float h, ccColor3B col, bool selected, std::string const& label) {
        auto node = CCNode::create();
        node->setContentSize({ w + 4, h + 4 });
        node->setAnchorPoint({ .5f, .5f });
        if (selected) {
            auto ring = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
            ring->setContentSize({ (w + 4) * 4, (h + 4) * 4 });
            ring->setScale(.25f);
            ring->setPosition({ (w + 4) / 2, (h + 4) / 2 });
            node->addChild(ring);
        }
        auto cell = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        cell->setContentSize({ w * 4, h * 4 });
        cell->setScale(.25f);
        cell->setColor(col);
        cell->setPosition({ (w + 4) / 2, (h + 4) / 2 });
        node->addChild(cell);
        if (!label.empty()) {
            auto l = CCLabelBMFont::create(label.c_str(), "bigFont.fnt");
            l->setScale(.3f);
            l->limitLabelWidth(w - 2, .3f, .12f);
            l->setPosition({ (w + 4) / 2, (h + 4) / 2 + 1 });
            node->addChild(l, 2);
        }
        return node;
    };

    for (int i = 0; i < K; i++) {
        ccColor3B col{ 70, 70, 85 };
        std::string label;
        if (pulse) {
            if (m_trig.active(i)) {
                unsigned char r, g, b;
                triggers::stepRGB(m_trig.step[i], r, g, b);
                col = m_trig.step[i].color == 0 ? ccColor3B{ 255, 245, 190 } : ccColor3B{ r, g, b };
                if (m_trig.step[i].color == 0) label = "+";
            } else label = "X";
        } else {
            auto groups = triggers::parseGroups(m_trig.step[i].groups);
            if (!groups.empty()) {
                col = { 90, 220, 110 };
                label = fmt::format("{}", groups[0]);
            } else label = "-";
        }
        auto item = CCMenuItemExt::createSpriteExtra(square(22, 20, col, i == m_trigSel, label), [this, i](auto) {
            m_trigSel = i;
            m_trigDirty = true;
        });
        item->setPosition({ m_trigStepsX + 26.f * i + 13, 46 });
        m_trigMenu->addChild(item);
    }

    if (pulse) {
        // the color of the selected step: a picked color (the swatch opens GD's color picker), the channel's own
        // color brightened (+, pulse only) or nothing (X)
        auto& cur = m_trig.step[m_trigSel];
        unsigned char r, g, b;
        triggers::stepRGB(cur, r, g, b);
        bool custom = cur.color >= 1 && !triggers::paletteIsOff(cur.color);
        auto swatch = CCMenuItemExt::createSpriteExtra(square(30, 16, { r, g, b }, custom, ""), [this](auto) {
            auto& st = m_trig.step[std::clamp(m_trigSel, 0, triggers::kMaxSteps - 1)];
            unsigned char r, g, b;
            triggers::stepRGB(st, r, g, b);
            auto popup = ColorPickPopup::create(ccColor4B{ r, g, b, 255 });
            int sel = m_trigSel;
            popup->setCallback([this, sel](ccColor4B const& c) {
                auto& st = m_trig.step[std::clamp(sel, 0, triggers::kMaxSteps - 1)];
                st.color = 1;
                st.rgb = (unsigned)c.r << 16 | (unsigned)c.g << 8 | c.b;
                saveTriggerSettings();
                m_trigDirty = true;
                m_trigTimesAt = -10;
            });
            popup->show();
        });
        swatch->setPosition({ m_trigPalX + 18, 22 });
        m_trigMenu->addChild(swatch);
        if (!colorKind) {
            auto bright = CCMenuItemExt::createSpriteExtra(square(18, 16, { 255, 245, 190 }, cur.color == 0, "+"), [this](auto) {
                m_trig.step[m_trigSel].color = 0;
                saveTriggerSettings();
                m_trigDirty = true;
                m_trigTimesAt = -10;
            });
            bright->setPosition({ m_trigPalX + 52, 22 });
            m_trigMenu->addChild(bright);
        }
        auto none = CCMenuItemExt::createSpriteExtra(
            square(18, 16, { 70, 70, 85 }, triggers::paletteIsOff(cur.color), "X"), [this](auto) {
                m_trig.step[m_trigSel].color = triggers::paletteCount() - 1;
                saveTriggerSettings();
                m_trigDirty = true;
                m_trigTimesAt = -10;
            });
        none->setPosition({ m_trigPalX + 78, 22 });
        m_trigMenu->addChild(none);
    } else if (m_trigGroupsInput && !m_trigGroupsInput->getInputNode()->m_selected) {
        m_updatingInputs = true;
        m_trigGroupsInput->setString(m_trig.step[m_trigSel].groups);
        m_updatingInputs = false;
    }
    m_trigMenu->setEnabled(m_tab == m_triggerTab && !m_layoutEdit);
}

void TimingEditor::placeTriggers() {
    auto& s = Session::get();
    if (!LevelEditorLayer::get()) {
        Notification::create("Open this from the level editor", NotificationIcon::Error)->show();
        return;
    }
    bool loop = m_trig.placement == triggers::Placement::Loop;
    if (triggers::everyDivisor(m_trig.every) >= 0 && s.map.empty()) {
        Notification::create("Add a timing point (BPM) first", NotificationIcon::Error)->show();
        return;
    }
    auto ts = triggers::times(s.map, s.envelope.get(), m_trig, songLength());
    if (ts.empty()) {
        Notification::create(triggers::everyDivisor(m_trig.every) < 0 ? "No hits found here (load the song first)"
                                                                       : "No beats in this part", NotificationIcon::Error)->show();
        return;
    }
    char const* kind = m_trig.kind == triggers::Kind::Pulse ? "pulse" : m_trig.kind == triggers::Kind::Spawn ? "spawn" : "color";
    auto what = loop ? fmt::format("a <cy>{} spawn loop</c> for <cg>{}</c> hits", kind, ts.size())
                     : fmt::format("<cg>{}</c> {} triggers", triggers::objectCount(m_trig, ts.size()), kind);
    auto settings = m_trig;
    createQuickPopup("Place triggers",
        fmt::format("Put {} into the level?\nOne <cy>Ctrl+Z</c> in the editor removes all of them.", what),
        "Cancel", "Place", [settings](auto, bool yes) {
            if (!yes) return;
            auto& s = Session::get();
            std::string err;
            double len = s.audio ? s.audio->lengthMs : (s.map.empty() ? 60000.0 : s.map.points.back().time + 30000.0);
            int n = triggers::place(s.map, s.envelope.get(), settings, len, &err);
            if (n <= 0) Notification::create(err.empty() ? "Nothing placed" : err, NotificationIcon::Error)->show();
            else Notification::create(fmt::format("Placed {} trigger{}", n, n == 1 ? "" : "s"), NotificationIcon::Success)->show();
        });
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
    if (m_layoutEdit) {
        auto kb = CCKeyboardDispatcher::get();
        float st = kb->getShiftKeyPressed() ? 5.f : 1.f;
        CCPoint d{ 0, 0 };
        switch (key) {
            case KEY_Left: d.x = -st; break;
            case KEY_Right: d.x = st; break;
            case KEY_Up: d.y = st; break;
            case KEY_Down: d.y = -st; break;
            case KEY_Delete:
                if (kb->getShiftKeyPressed()) {
                    for (auto& [n, base] : m_uiBase) if (inCurrentTab(n)) n->setPosition(base);
                } else if (m_editSel) {
                    m_editSel->setPosition(m_uiBase[m_editSel]);
                }
                saveUiLayout();
                return;
            default: return Popup::keyDown(key, ts);
        }
        if (m_editSel) {
            m_editSel->setPosition(m_editSel->getPosition() + d);
            saveUiLayout();
        }
        return;
    }
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

// ---------------- Move UI (layout editing) ----------------

// rectangle of a control in window space (tabs sit at 0,0); nodes without a size use their children
static CCRect uiRect(CCNode* n) {
    auto r = n->boundingBox();
    if (r.size.width >= 2 && r.size.height >= 2) return r;
    bool any = false;
    CCRect u;
    auto tr = n->nodeToParentTransform();
    if (auto kids = n->getChildren()) {
        for (auto c : CCArrayExt<CCNode*>(kids)) {
            auto cr = CCRectApplyAffineTransform(c->boundingBox(), tr);
            if (cr.size.width < 1 && cr.size.height < 1) continue;
            if (!any) { u = cr; any = true; continue; }
            float x0 = std::min(u.getMinX(), cr.getMinX()), y0 = std::min(u.getMinY(), cr.getMinY());
            float x1 = std::max(u.getMaxX(), cr.getMaxX()), y1 = std::max(u.getMaxY(), cr.getMaxY());
            u = CCRect(x0, y0, x1 - x0, y1 - y0);
        }
    }
    if (any) return u;
    auto pos = n->getPosition();
    return CCRect(pos.x - 20, pos.y - 7, 40, 14);
}

void TimingEditor::applyUiLayout() {
    m_uiBase.clear();
    m_uiIds.clear();
    // saved as "id=dx,dy;id=dx,dy;..."
    std::map<std::string, CCPoint> saved;
    auto str = Mod::get()->getSavedValue<std::string>("ui-layout-2", "");
    for (auto const& part : utils::string::split(str, ";")) {
        auto eq = part.find('='), comma = part.find(',');
        if (eq == std::string::npos || comma == std::string::npos || comma < eq) continue;
        auto dx = numFromString<float>(part.substr(eq + 1, comma - eq - 1));
        auto dy = numFromString<float>(part.substr(comma + 1));
        if (dx && dy) saved[part.substr(0, eq)] = CCPoint{ *dx, *dy };
    }
    for (int i = 0; i < (int)m_tabs.size(); i++) {
        auto& t = m_tabs[i];
        auto add = [&](CCNode* n, std::string id) {
            m_uiBase[n] = n->getPosition();
            if (auto it = saved.find(id); it != saved.end()) n->setPosition(n->getPosition() + it->second);
            m_uiIds[n] = std::move(id);
        };
        int j = 0;
        if (auto kids = t.node->getChildren())
            for (auto n : CCArrayExt<CCNode*>(kids)) {
                // the tab's menus (buttons are added one by one below; the rhythm grid is rebuilt all the time)
                if (!typeinfo_cast<CCMenu*>(n)) add(n, fmt::format("t{}n{}", i, j));
                j++;
            }
        j = 0;
        if (auto kids = t.menu->getChildren())
            for (auto n : CCArrayExt<CCNode*>(kids)) add(n, fmt::format("t{}m{}", i, j++));
    }
}

void TimingEditor::saveUiLayout() {
    std::string out;
    for (auto const& [n, id] : m_uiIds) {
        auto d = n->getPosition() - m_uiBase[n];
        if (std::abs(d.x) < .5f && std::abs(d.y) < .5f) continue;
        out += fmt::format("{}={:.0f},{:.0f};", id, d.x, d.y);
    }
    Mod::get()->setSavedValue<std::string>("ui-layout-2", out);
}

void TimingEditor::toggleLayoutEdit() {
    m_layoutEdit = !m_layoutEdit;
    m_editSel = nullptr;
    if (m_editBtnLabel) m_editBtnLabel->setString(m_layoutEdit ? "Done" : "Move UI");
    if (m_editHint) m_editHint->setVisible(m_layoutEdit);
    if (m_rhythmGrid) m_rhythmGrid->setEnabled(!m_layoutEdit);
    switchTab(m_tab);
    if (!m_layoutEdit) saveUiLayout();
}

bool TimingEditor::inCurrentTab(CCNode* n) const {
    if (m_tab < 0 || m_tab >= (int)m_tabs.size()) return false;
    auto parent = n->getParent();
    return parent == m_tabs[m_tab].node || parent == m_tabs[m_tab].menu;
}

// the smallest control of the shown tab under the point
CCNode* TimingEditor::pickUiNode(CCPoint p) const {
    CCNode* best = nullptr;
    float bestArea = 1e9f;
    for (auto const& [n, id] : m_uiIds) {
        if (!inCurrentTab(n) || !n->isVisible()) continue;
        auto r = uiRect(n);
        r.origin = r.origin - CCPoint{ 2, 2 };
        r.size = r.size + CCSize{ 4, 4 };
        if (!r.containsPoint(p)) continue;
        float area = r.size.width * r.size.height;
        if (area < bestArea) { bestArea = area; best = n; }
    }
    return best;
}

void TimingEditor::drawLayoutEdit() {
    if (!m_editDraw) return;
    m_editDraw->clear();
    if (!m_layoutEdit) return;
    // dim the waveform so the hint is readable
    m_editDraw->drawRect(CCRect(WX, WY, WW, WH), rgba(0, 0, 0, .75f), 0, rgba(0, 0, 0, 0));
    for (auto const& [n, id] : m_uiIds) {
        if (!inCurrentTab(n) || !n->isVisible()) continue;
        bool sel = n == m_editSel;
        m_editDraw->drawRect(uiRect(n), rgba(1, 1, 1, sel ? .12f : 0), sel ? 1.f : .5f,
            sel ? rgba(1, 1, 1, 1) : rgba(.4f, .9f, 1, .55f));
    }
}

// ---------------- touch / mouse ----------------

bool TimingEditor::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    auto& s = Session::get();
    if (m_layoutEdit) {
        m_editSel = pickUiNode(p);
        if (m_editSel) {
            m_drag = Drag::Layout;
            m_editGrab = p;
            m_editStart = m_editSel->getPosition();
            return true;
        }
    }
    if (p.x >= OX && p.x <= OX + OW && p.y >= OY - 2 && p.y <= OY + OH + 2) {
        m_drag = Drag::Overview;
        ccTouchMoved(touch, event);
        return true;
    }
    if (p.x >= WX && p.x <= WX + WW && p.y >= WY && p.y <= WY + WH) {
        m_dragStartX = p.x;
        m_dragStartView = m_viewStart;
        m_drag = Drag::Pending;
        // timing points (red, triangle at the top) and rhythm points (purple, triangle at the bottom)
        int best = -1, bestR = -1;
        float bestDist = 7, bestDistR = m_tab == m_rhythmTab ? 10.f : 7.f;
        for (int i = 0; i < (int)s.map.points.size(); i++) {
            float d = std::abs(xAtTime(s.map.points[i].time) - p.x);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        for (int i = 0; i < (int)s.map.rhythm.size(); i++) {
            float d = std::abs(xAtTime(s.map.rhythm[i].time) - p.x);
            if (d < bestDistR) { bestDistR = d; bestR = i; }
        }
        // both under the mouse (a rhythm point made on a timing point): the Rhythm tab grabs the rhythm point,
        // elsewhere the lower half of the waveform (where its purple triangle is) grabs it
        bool rhythm = bestR >= 0 && (best < 0 || m_tab == m_rhythmTab || p.y < WY + WH * .5f);
        if (rhythm) {
            if (bestR != m_rhythmSel) m_rhythmPage = 0;
            m_rhythmSel = bestR;
            m_drag = Drag::Rhythm;
            m_dragStartTime = s.map.rhythm[bestR].time;
            if (m_rhythmTab >= 0 && m_tab != m_rhythmTab) switchTab(m_rhythmTab);
            refreshPanel();
        } else if (best >= 0) {
            m_selected = best;
            m_drag = Drag::Marker;
            m_dragStartTime = s.map.points[best].time;
            refreshPanel();
        }
        return true;
    }
    return Popup::ccTouchBegan(touch, event);
}

void TimingEditor::ccTouchMoved(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    auto& s = Session::get();
    switch (m_drag) {
        case Drag::Layout:
            if (m_editSel) {
                auto np = m_editStart + (p - m_editGrab);
                m_editSel->setPosition({ std::round(np.x), std::round(np.y) });
            }
            break;
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
                // rhythm points snap to the Grid of the Timing Points tab (1/4 ...) unless "Snap to grid" is off
                // (Alt = free for this drag)
                bool snap = savedBool("rhythm-snap", true) && !CCKeyboardDispatcher::get()->getAltKeyPressed();
                if (snap && !s.map.empty()) t = s.map.snap(t, m_divisor);
                else t = std::round(t);
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
    } else if (m_drag == Drag::Layout) {
        saveUiLayout();
    } else if (m_drag == Drag::None) {
        Popup::ccTouchEnded(touch, event);
    }
    m_drag = Drag::None;
}

void TimingEditor::ccTouchCancelled(CCTouch* touch, CCEvent* event) {
    if (m_drag == Drag::Marker || m_drag == Drag::Rhythm) changed();
    if (m_drag == Drag::Layout) saveUiLayout();
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

void TimingEditor::releaseRateDsps() {
    for (auto dsp : m_rateDsps) {
        if (m_channel) m_channel->removeDSP(dsp);
        dsp->release();
    }
    m_rateDsps.clear();
}

// the channel plays at m_rate of its normal frequency; pitch shifters (up to 2x each) bring the pitch back up
void TimingEditor::applyRate() {
    releaseRateDsps();
    if (!m_channel || !m_sound) return;
    float freq = 0;
    int priority = 0;
    if (m_sound->getDefaults(&freq, &priority) != FMOD_OK || freq <= 0) return;
    m_channel->setFrequency(freq * m_rate);
    auto sys = FMODAudioEngine::get()->m_system;
    for (float need = 1.f / m_rate; need > 1.001f;) {
        float step = std::min(2.f, need);
        FMOD::DSP* dsp = nullptr;
        if (sys->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &dsp) != FMOD_OK || !dsp) break;
        dsp->setParameterFloat(FMOD_DSP_PITCHSHIFT_PITCH, step);
        dsp->setParameterFloat(FMOD_DSP_PITCHSHIFT_FFTSIZE, 2048.f);
        m_channel->addDSP(0, dsp);
        m_rateDsps.push_back(dsp);
        need /= step;
    }
}

void TimingEditor::stopPlayback() {
    releaseRateDsps();
    if (m_channel) m_channel->stop();
    m_channel = nullptr;
    if (m_sound) m_sound->release();
    m_sound = nullptr;
    m_soundPath.clear();
    m_playing = false;
    updatePlayIcon();
}

void TimingEditor::seek(double ms) {
    m_cursor = std::max(0.0, ms);
    if (m_channel) m_channel->setPosition((unsigned)m_cursor, FMOD_TIMEUNIT_MS);
    m_tracker.reset();
}

void TimingEditor::updatePlayIcon() {
    if (!m_playIcon) return;
    if (auto frame = CCSpriteFrameCache::get()->spriteFrameByName(m_playing ? "GJ_stopMusicBtn_001.png" : "GJ_playMusicBtn_001.png"))
        m_playIcon->setDisplayFrame(frame);
}

void TimingEditor::togglePlay() {
    auto& s = Session::get();
    if (s.audioPath.empty()) return;
    if (m_playing) {
        m_cursor = playPosition();
        if (m_channel) m_channel->setPaused(true);
        m_playing = false;
        updatePlayIcon();
        return;
    }
    auto sys = FMODAudioEngine::get()->m_system;
    // a channel that finished or was stolen by FMOD is invalid - make a new one
    if (m_channel) {
        bool isPlaying = false;
        if (m_channel->isPlaying(&isPlaying) != FMOD_OK || !isPlaying) {
            releaseRateDsps();
            m_channel = nullptr;
        }
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
        releaseRateDsps();
        sys->playSound(m_sound, nullptr, true, &m_channel);
        if (!m_channel) return;
        m_channel->setVolume(FMODAudioEngine::get()->m_musicVolume);
        m_channel->setPosition((unsigned)std::max(0.0, m_cursor), FMOD_TIMEUNIT_MS);
    }
    applyRate();
    m_channel->setPaused(false);
    m_tracker.reset();
    m_playing = true;
    updatePlayIcon();
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
    toast(fmt::format("Timing point added at {}", fmtTime(p.time)));
}

void TimingEditor::deletePoint() {
    auto& s = Session::get();
    if (m_selected < 0 || m_selected >= (int)s.map.points.size()) return;
    s.map.points.erase(s.map.points.begin() + m_selected);
    m_selected = std::min(m_selected, (int)s.map.points.size() - 1);
    changed();
    toast("Timing point deleted");
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
    drawLayoutEdit();
    m_clock += dt;
    auto& s = Session::get();
    if (m_offsetLabel) {
        auto ls = Session::levelSettings();
        m_offsetLabel->setString(ls ? "" : "Open this from a level");
        // keep the field in sync with the cursor button / editor (not while typing in it)
        if (ls && m_offsetInput && !m_offsetInput->getInputNode()->m_selected) {
            auto text = fmt::format("{:.3f}", ls->m_songOffset);
            if (m_offsetInput->getString() != text) {
                m_updatingInputs = true;
                m_offsetInput->setString(text);
                m_updatingInputs = false;
            }
        }
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
            if (m_followPlay && (rel > 0.75 || rel < 0)) m_viewStart = pos - WW * m_msPerPx * 0.25;
        }
    }
    if (s.busy) {
        m_statusLabel->setString(fmt::format("{}... {:.0f}%", s.busyText, s.progress.load() * 100).c_str());
    } else {
        std::string st = m_statusLabel->getString();
        if (st.find("%") != std::string::npos) m_statusLabel->setString("");
    }
    // the editor's 1/N button changes the guidelines too
    setRoundText(m_halfLabel, fmt::format("1/{}", s.guideDivisor()));

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
    // the second row of tabs starts one cell later, so this text has the room up to Files
    limitNodeWidth(m_gridLabel, kTabLeft + kTabW - WX - 4, .5f, .2f);
    if (m_tab == m_triggerTab && m_trigCountLabel && m_clock - m_trigTimesAt > .5) {
        m_trigTimesAt = m_clock;
        m_trigTimes = triggers::times(s.map, s.envelope.get(), m_trig, songLength());
        bool loop = m_trig.placement == triggers::Placement::Loop;
        std::string count;
        if (!LevelEditorLayer::get()) count = "Open from the editor";
        else if (loop && triggers::everyDivisor(m_trig.every) < 0) count = "Loop: not with HITS";
        else if (loop) {
            // one loop per BPM / rhythm pattern in the part
            int loops = triggers::loopCount(s.map, m_trig, m_trigTimes);
            count = fmt::format("{} loop{}, {} hits", loops, loops == 1 ? "" : "s", m_trigTimes.size());
        }
        else {
            int n = triggers::objectCount(m_trig, m_trigTimes.size());
            count = fmt::format("{} trigger{}", n, n == 1 ? "" : "s");
        }
        m_trigCountLabel->setString(count.c_str());
        m_trigCountLabel->limitLabelWidth(120, .34f, .15f);
    }
    if (m_trigDirty && m_tab == m_triggerTab) rebuildTriggerSteps();
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
    // Triggers tab: the part is green, a short green tick for every trigger
    if (m_tab == m_triggerTab && m_triggerTab >= 0) {
        auto [from, to] = triggers::range(m_trig, songLength());
        float x0 = std::max(WX, xAtTime(from)), x1 = std::min(WX + WW, xAtTime(to));
        if (x1 > x0) m_draw->drawRect(CCRect(x0, WY, x1 - x0, WH), rgba(.3f, 1, .4f, .08f), .5f, rgba(.3f, 1, .4f, .5f));
        auto it = std::lower_bound(m_trigTimes.begin(), m_trigTimes.end(), m_viewStart);
        for (; it != m_trigTimes.end() && *it <= viewEnd; ++it) {
            float x = xAtTime(*it);
            m_draw->drawSegment({ x, WY + WH - 12 }, { x, WY + WH }, .7f, rgba(.4f, 1, .5f, .95f));
        }
    }
    // beat start found by the analysis
    if (s.firstBeatMs >= 0) {
        float x = xAtTime(s.firstBeatMs);
        if (x >= WX && x <= WX + WW) m_draw->drawSegment({ x, WY }, { x, WY + 10 }, 1, rgba(0.3f, 1, 0.3f, 1));
    }
    // where the level's song starts (Song Start): only shown, it is changed with the Song Start controls
    bool showStart = false;
    if (auto ls = Session::levelSettings()) {
        float x = xAtTime(ls->m_songOffset * 1000.0);
        if (x >= WX && x <= WX + WW) {
            auto col = rgba(.25f, 1, .8f, .95f);
            m_draw->drawSegment({ x, WY }, { x, WY + WH }, .9f, col);
            CCPoint flag[3] = { { x, WY + WH - 14 }, { x + 9, WY + WH - 18 }, { x, WY + WH - 22 } };
            m_draw->drawPolygon(flag, 3, col, 0, col);
            if (m_startLabel) {
                m_startLabel->setPosition({ std::min(x + 11, WX + WW - 60), WY + WH - 14 });
                showStart = true;
            }
        }
    }
    if (m_startLabel) m_startLabel->setVisible(showStart);
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
    if (m_tab == m_triggerTab && m_triggerTab >= 0) {
        auto [from, to] = triggers::range(m_trig, len);
        float o0 = float(OX + std::clamp(from / len, 0.0, 1.0) * OW), o1 = float(OX + std::clamp(to / len, 0.0, 1.0) * OW);
        if (o1 > o0) m_draw->drawRect(CCRect(o0, OY, o1 - o0, OH), rgba(.3f, 1, .4f, .25f), 0, rgba(0, 0, 0, 0));
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

    // top right: one square per beat of the bar, the current one lights up (pink = first beat, purple = the rest)
    // and fades until the next beat while playing
    if (int pi = s.map.indexAt(m_cursor); pi >= 0) {
        auto const& p = s.map.points[pi];
        int meter = std::clamp(p.meter, 1, 16);
        double beats = (m_cursor - p.time) / p.beatLength;
        long long b = (long long)std::floor(beats + 1e-6);
        double phase = std::clamp(beats - b, 0.0, 1.0);
        int cur = (int)(((b % meter) + meter) % meter);
        float gap = 3, size = std::min(11.f, (150 - gap * (meter - 1)) / meter);
        float right = POP_W - 16, y = POP_H - 15;
        float glow = m_playing ? (float)std::pow(1 - phase, 2) : 1.f;
        for (int i = 0; i < meter; i++) {
            float x = right - (meter - i) * size - (meter - 1 - i) * gap;
            bool first = i == 0, on = i == cur;
            ccColor4F base = first ? rgba(1, .45f, .85f) : rgba(.6f, .3f, 1);
            float k = on ? .35f + .65f * glow : .18f;
            ccColor4F fill = rgba(base.r * k, base.g * k, base.b * k, .95f);
            if (on && glow > .05f) {
                float g = 2.5f * glow;
                m_draw->drawRect(CCRect(x - g, y - size / 2 - g, size + 2 * g, size + 2 * g),
                                 rgba(base.r, base.g, base.b, .35f * glow), 0, rgba(0, 0, 0, 0));
            }
            m_draw->drawRect(CCRect(x, y - size / 2, size, size), fill, .6f, rgba(0, 0, 0, .8f));
        }
    }

    double bpm = s.map.bpmAt(m_cursor);
    m_infoLabel->setString(fmt::format("{} / {}  BPM {}",
        fmtTime(m_cursor), fmtTime(s.audio ? s.audio->lengthMs : 0),
        bpm > 0 ? fmt::format("{:.2f}", bpm) : std::string("-")).c_str());
    // stays left of the tab bar
    limitNodeWidth(m_infoLabel, 118, .5f, .2f);
}
