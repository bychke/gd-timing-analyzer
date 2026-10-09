#include "LayoutPopup.hpp"
#include "../Options.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/ColorPickPopup.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/EditorUI.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
constexpr float W = 340.f, H = 112.f;
} // namespace

void LayoutPopup::open(int tab, bool returnToPause) {
    auto ret = new LayoutPopup();
    ret->m_startTab = tab;
    ret->m_returnToPause = returnToPause;
    if (ret->init()) {
        ret->autorelease();
        ret->show();
        return;
    }
    delete ret;
}

bool LayoutPopup::init() {
    if (!Popup::init(W, H)) return false;
    this->setTitle("Timing Analyzer - UI Settings", "goldFont.fnt", .55f, 12.f);
    // no dimming: the editor behind is the preview
    this->setOpacity(0);
    // slides up from the bottom edge and stays there, so the free middle of the screen stays visible
    m_noElasticity = true;
    float restY = H / 2 + 2;
    m_mainLayer->setPositionY(-H / 2);
    m_mainLayer->runAction(CCEaseExponentialOut::create(CCMoveTo::create(.25f,
        { m_mainLayer->getPositionX(), restY })));

    // tabs: editor buttons / beat texts / guideline colors, each a page with its own menu
    auto makePage = [&] {
        auto page = CCNode::create();
        page->setContentSize({ W, H });
        m_mainLayer->addChild(page);
        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });
        page->addChild(menu);
        m_pages.push_back({ page, menu });
        return std::make_pair(page, menu);
    };
    // the scale has to be set before the menu item is made: the item takes its hitbox from the sprite's size
    auto addButtonTo = [](CCMenu* menu, char const* text, CCPoint pos, char const* bg, auto cb,
                          CCLabelBMFont** out = nullptr, float scale = .42f) {
        auto spr = ButtonSprite::create(text, "bigFont.fnt", bg, .7f);
        spr->setScale(scale);
        if (out) *out = spr->m_label;
        auto btn = CCMenuItemExt::createSpriteExtra(spr, cb);
        btn->setPosition(pos);
        menu->addChild(btn);
        return spr;
    };
    // right under the tab buttons
    auto addHint = [](CCNode* page, char const* text) {
        auto hint = CCLabelBMFont::create(text, "chatFont.fnt");
        hint->setScale(.42f);
        hint->setPosition({ W / 2, 60 });
        page->addChild(hint);
    };
    auto addCheck = [&](CCNode* page, CCMenu* menu, char const* text, char const* key, CCPoint pos) {
        auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(.5f, [key](CCMenuItemToggler* t) {
            // called before the toggler flips
            opt::set<bool>(key, !t->isToggled());
        });
        toggle->toggle(opt::get<bool>(key));
        toggle->setPosition(pos);
        menu->addChild(toggle);
        m_checks.push_back({ toggle, key });
        auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
        lbl->setScale(.3f);
        lbl->setAnchorPoint({ 0, .5f });
        lbl->setPosition(pos + CCPoint{ 13, 0 });
        page->addChild(lbl);
    };

    // top row: tabs on the left, Reset (for the current tab) and Done on the right
    float const TOP = H - 34;
    auto tabMenu = CCMenu::create();
    tabMenu->setPosition({ 0, 0 });
    m_mainLayer->addChild(tabMenu);
    char const* tabNames[] = { "Buttons", "Text", "Guideline Colors" };
    float tx = 14, gap = 6;
    for (int i = 0; i < 3; i++) {
        m_tabs[i] = addButtonTo(tabMenu, tabNames[i], { 0, TOP }, "GJ_button_04.png", [this, i](auto) { showTab(i); },
                                nullptr, .36f);
        float w = m_tabs[i]->getScaledContentSize().width;
        m_tabs[i]->getParent()->setPositionX(tx + w / 2);
        tx += w + gap;
    }
    addButtonTo(tabMenu, "Reset", { W - 78, TOP }, "GJ_button_06.png", [this](auto) { resetTab(); }, nullptr, .36f);
    addButtonTo(tabMenu, "Done", { W - 30, TOP }, "GJ_button_02.png", [this](auto) { this->onClose(nullptr); }, nullptr,
                .36f);

    // ===== tab 1: editor buttons =====
    {
        auto [page, menu] = makePage();
        addHint(page, "Drag anywhere outside this window to move the buttons");
        // which buttons are shown (the others move together), next to the layout / size row below
        auto showLbl = CCLabelBMFont::create("Show:", "goldFont.fnt");
        showLbl->setScale(.42f);
        showLbl->setAnchorPoint({ 0, .5f });
        showLbl->setPosition({ 12, 42 });
        page->addChild(showLbl);
        addCheck(page, menu, "BPM", "show-btn-bpm", { 64, 42 });
        addCheck(page, menu, "Wave", "show-btn-wave", { 112, 42 });
        addCheck(page, menu, "Timing window", "show-btn-time", { 166, 42 });
        addCheck(page, menu, "1/N lines", "show-btn-div", { 266, 42 });
        float y = 22;
        addButtonTo(menu, "Layout: 2x2", { 50, y }, "GJ_button_04.png", [this](auto) {
            auto mod = Mod::get();
            auto cur = opt::get<std::string>("buttons-layout");
            opt::set<std::string>("buttons-layout", cur == "row" ? "2x2" : cur == "2x2" ? "column" : "row");
            refresh();
        }, &m_layoutLabel);
        auto changeSize = [this](double d) {
            auto mod = Mod::get();
            double v = std::clamp(opt::get<double>("buttons-scale") + d, 0.5, 1.5);
            opt::set<double>("buttons-scale", std::round(v * 20) / 20);
            refresh();
        };
        addButtonTo(menu, "-", { 112, y }, "GJ_button_01.png", [changeSize](auto) { changeSize(-0.05); });
        m_sizeLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_sizeLabel->setScale(.35f);
        m_sizeLabel->setPosition({ 140, y });
        page->addChild(m_sizeLabel);
        addButtonTo(menu, "+", { 168, y }, "GJ_button_01.png", [changeSize](auto) { changeSize(0.05); });
        addCheck(page, menu, "Hide in playtest", "hide-buttons-playtest", { 215, y });
    }

    // ===== tab 2: beat texts at the top of the editor =====
    {
        auto [page, menu] = makePage();
        addHint(page, "Drag anywhere outside this window to move the text");
        addCheck(page, menu, "Show beat text", "show-object-beat", { 30, 22 });
        addCheck(page, menu, "Colored center text", "center-text-colors", { 170, 22 });
    }

    // ===== tab 3: waveform + guideline colors (osu! beat snap colors), tap a swatch to change it =====
    {
        auto [page, menu] = makePage();
        // thickness of the guidelines: - 0.4 +
        auto thLbl = CCLabelBMFont::create("Guideline thickness", "chatFont.fnt");
        thLbl->setScale(.42f);
        thLbl->setAnchorPoint({ 1, .5f });
        thLbl->setPosition({ 150, 48 });
        page->addChild(thLbl);
        auto changeThickness = [this](int d) {
            auto mod = Mod::get();
            double v = std::clamp(opt::get<double>("guideline-thickness") + d * 0.1, 0.1, 3.0);
            opt::set<double>("guideline-thickness", std::round(v * 10) / 10);
            refresh();
        };
        addButtonTo(menu, "-", { 166, 48 }, "GJ_button_01.png", [changeThickness](auto) { changeThickness(-1); }, nullptr, .36f);
        m_thicknessLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_thicknessLabel->setScale(.3f);
        m_thicknessLabel->setPosition({ 192, 48 });
        page->addChild(m_thicknessLabel);
        addButtonTo(menu, "+", { 218, 48 }, "GJ_button_01.png", [changeThickness](auto) { changeThickness(1); }, nullptr, .36f);
        struct Swatch { char const* id; char const* label; };
        static constexpr Swatch swatches[] = {
            { "editor-waveform-color", "wave" }, { "guide-color-1", "1/1" }, { "guide-color-2", "1/2" },
            { "guide-color-3", "1/3" }, { "guide-color-4", "1/4" }, { "guide-color-6", "1/6" },
            { "guide-color-8", "1/8" }, { "guide-color-12", "1/12" }, { "guide-color-16", "other" } };
        float x = 47;
        for (auto const& sw : swatches) {
            // plain white quad tinted with the color (GD's square sprites are dark, tinting them stays black)
            auto spr = CCLayerColor::create({ 255, 255, 255, 255 }, 22, 22);
            // CCLayerColor ignores the anchor point by default, which shifts it off its button
            spr->ignoreAnchorPointForPosition(false);
            auto id = std::string(sw.id);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, id](auto) {
                auto popup = ColorPickPopup::create(opt::get<ccColor4B>(id));
                popup->setCallback([this, id](ccColor4B const& c) {
                    opt::set<ccColor4B>(id, c);
                    refresh();
                });
                popup->show();
            });
            btn->setPosition({ x, 26 });
            menu->addChild(btn);
            m_swatches.push_back({ id, spr });
            auto lbl = CCLabelBMFont::create(sw.label, "chatFont.fnt");
            lbl->setScale(.35f);
            lbl->setPosition({ x, 9 });
            page->addChild(lbl);
            x += 30;
        }
    }

    // pulsing frame around what dragging moves (the editor buttons or the beat texts)
    m_highlight = CCDrawNode::create();
    this->addChild(m_highlight, -1);
    this->scheduleUpdate();

    showTab(m_startTab);
    refresh();
    return true;
}

void LayoutPopup::onClose(CCObject* sender) {
    Popup::onClose(sender);
    if (!m_returnToPause) return;
    // opened from the pause menu: back to it
    queueInMainThread([] {
        auto lel = LevelEditorLayer::get();
        if (lel && lel->m_editorUI) lel->m_editorUI->onPause(nullptr);
    });
}

void LayoutPopup::resetTab() {
    auto mod = Mod::get();
    if (m_tab == 0) {
        opt::set<int64_t>("buttons-x", 0);
        opt::set<int64_t>("buttons-y", 0);
        opt::set<double>("buttons-scale", 1.0);
        opt::set<std::string>("buttons-layout", "row");
        for (auto key : { "show-btn-bpm", "show-btn-wave", "show-btn-time", "show-btn-div" }) opt::reset(key);
    } else if (m_tab == 1) {
        opt::set<int64_t>("text-x", 0);
        opt::set<int64_t>("text-y", 0);
    } else {
        for (auto& [id, spr] : m_swatches)
            opt::reset(id);
        opt::reset("guideline-thickness");
    }
    refresh();
}

void LayoutPopup::update(float dt) {
    m_time += dt;
    m_highlight->clear();
    auto ui = LevelEditorLayer::get() ? LevelEditorLayer::get()->m_editorUI : nullptr;
    if (!ui || m_tab > 1) return;
    auto id = [](char const* name) { return fmt::format("{}/{}", Mod::get()->getID(), name); };
    std::vector<CCNode*> nodes;
    if (m_tab == 0) {
        if (auto menu = ui->getChildByIDRecursive(id("metronome-menu")))
            for (auto child : CCArrayExt<CCNode*>(menu->getChildren())) nodes.push_back(child);
    } else {
        for (auto name : { "center-position", "grid-position" })
            if (auto n = ui->getChildByIDRecursive(id(name))) nodes.push_back(n);
    }
    bool any = false;
    CCRect box;
    for (auto n : nodes) {
        if (!n->isVisible() || !n->getParent()) continue;
        auto bb = n->boundingBox();
        auto a = this->convertToNodeSpace(n->getParent()->convertToWorldSpace(bb.origin));
        auto b = this->convertToNodeSpace(n->getParent()->convertToWorldSpace({ bb.getMaxX(), bb.getMaxY() }));
        CCRect r{ std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x), std::abs(b.y - a.y) };
        if (r.size.width <= 0 && r.size.height <= 0) continue;
        if (!any) box = r;
        else {
            float x0 = std::min(box.getMinX(), r.getMinX()), y0 = std::min(box.getMinY(), r.getMinY());
            float x1 = std::max(box.getMaxX(), r.getMaxX()), y1 = std::max(box.getMaxY(), r.getMaxY());
            box = { x0, y0, x1 - x0, y1 - y0 };
        }
        any = true;
    }
    if (!any) return;
    float pad = 4, pulse = .5f + .5f * std::sin(m_time * 9.f);
    CCPoint quad[4] = { { box.getMinX() - pad, box.getMinY() - pad }, { box.getMaxX() + pad, box.getMinY() - pad },
                        { box.getMaxX() + pad, box.getMaxY() + pad }, { box.getMinX() - pad, box.getMaxY() + pad } };
    // outline only (the filled polygon came out as a solid block); the pulse is the line's brightness
    ccColor4F c{ 1, 1, 1, .2f + .8f * pulse };
    for (int i = 0; i < 4; i++) m_highlight->drawSegment(quad[i], quad[(i + 1) % 4], 1.f, c);
}

void LayoutPopup::showTab(int tab) {
    m_tab = tab;
    for (int i = 0; i < (int)m_pages.size(); i++) {
        m_pages[i].first->setVisible(i == tab);
        m_pages[i].second->setEnabled(i == tab);
        // selected tab: bright green; the others dimmed. (Swapping the background image resized the sprite
        // and moved its picture off the clickable area, so only the tint changes.)
        if (m_tabs[i]) {
            m_tabs[i]->setCascadeColorEnabled(true);
            m_tabs[i]->setColor(i == tab ? ccColor3B{ 140, 255, 90 } : ccColor3B{ 120, 120, 120 });
        }
    }
}

void LayoutPopup::refresh() {
    for (auto& [toggle, key] : m_checks)
        if (toggle->isToggled() != opt::get<bool>(key)) toggle->toggle(opt::get<bool>(key));
    auto mod = Mod::get();
    if (m_thicknessLabel)
        m_thicknessLabel->setString(fmt::format("{:.1f}", opt::get<double>("guideline-thickness")).c_str());
    for (auto& [id, spr] : m_swatches) {
        auto c = opt::get<ccColor4B>(id);
        spr->setColor({ c.r, c.g, c.b });
    }
    if (m_layoutLabel)
        m_layoutLabel->setString(fmt::format("Layout: {}", opt::get<std::string>("buttons-layout")).c_str());
    if (m_sizeLabel)
        m_sizeLabel->setString(fmt::format("{:.0f}%", opt::get<double>("buttons-scale") * 100).c_str());
}

bool LayoutPopup::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    auto size = m_mainLayer->getContentSize();
    // touches on the window itself behave normally
    if (p.x >= 0 && p.y >= 0 && p.x <= size.width && p.y <= size.height) return Popup::ccTouchBegan(touch, event);
    m_dragging = true;
    m_dragStart = touch->getLocation();
    // the Text tab moves the beat texts, the other tabs the buttons
    m_dragKeyX = m_tab == 1 ? "text-x" : "buttons-x";
    m_dragKeyY = m_tab == 1 ? "text-y" : "buttons-y";
    m_startX = opt::get<int64_t>(m_dragKeyX);
    m_startY = opt::get<int64_t>(m_dragKeyY);
    return true;
}

void LayoutPopup::ccTouchMoved(CCTouch* touch, CCEvent* event) {
    if (!m_dragging) return Popup::ccTouchMoved(touch, event);
    auto d = touch->getLocation() - m_dragStart;
    // the editor re-reads these every frame, so the buttons follow the finger
    opt::set<int64_t>(m_dragKeyX, std::clamp<int64_t>(m_startX + (int64_t)std::lround(d.x), -500, 500));
    opt::set<int64_t>(m_dragKeyY, std::clamp<int64_t>(m_startY + (int64_t)std::lround(d.y), -300, 300));
}

void LayoutPopup::ccTouchEnded(CCTouch* touch, CCEvent* event) {
    if (!m_dragging) return Popup::ccTouchEnded(touch, event);
    m_dragging = false;
}

void LayoutPopup::ccTouchCancelled(CCTouch* touch, CCEvent* event) {
    if (!m_dragging) return Popup::ccTouchCancelled(touch, event);
    m_dragging = false;
}
