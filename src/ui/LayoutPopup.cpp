#include "LayoutPopup.hpp"

#include <Geode/binding/ButtonSprite.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
constexpr float W = 290.f, H = 100.f;
} // namespace

void LayoutPopup::open() {
    auto ret = new LayoutPopup();
    if (ret->init()) {
        ret->autorelease();
        ret->show();
        return;
    }
    delete ret;
}

bool LayoutPopup::init() {
    if (!Popup::init(W, H)) return false;
    this->setTitle("Editor buttons", "goldFont.fnt", .6f, 12.f);
    // no dimming: the editor behind is the preview
    this->setOpacity(0);
    // slides up from the bottom edge and stays there, so the free middle of the screen stays visible
    m_noElasticity = true;
    float restY = H / 2 + 2;
    m_mainLayer->setPositionY(-H / 2);
    m_mainLayer->runAction(CCEaseExponentialOut::create(CCMoveTo::create(.25f,
        { m_mainLayer->getPositionX(), restY })));

    auto hint = CCLabelBMFont::create("Drag anywhere outside this window to move the buttons", "chatFont.fnt");
    hint->setScale(.45f);
    hint->setPosition({ W / 2, H - 31 });
    m_mainLayer->addChild(hint);

    auto menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    m_mainLayer->addChild(menu);

    auto addButton = [&](char const* text, CCPoint pos, char const* bg, auto cb, CCLabelBMFont** out = nullptr) {
        auto spr = ButtonSprite::create(text, "bigFont.fnt", bg, .7f);
        spr->setScale(.42f);
        if (out) *out = spr->m_label;
        auto btn = CCMenuItemExt::createSpriteExtra(spr, cb);
        btn->setPosition(pos);
        menu->addChild(btn);
    };

    float y = 52;
    addButton("Layout: 2x2", { 50, y }, "GJ_button_04.png", [this](auto) {
        auto mod = Mod::get();
        auto cur = mod->getSettingValue<std::string>("buttons-layout");
        mod->setSettingValue<std::string>("buttons-layout", cur == "row" ? "2x2" : cur == "2x2" ? "column" : "row");
        refresh();
    }, &m_layoutLabel);
    auto changeSize = [this](double d) {
        auto mod = Mod::get();
        double v = std::clamp(mod->getSettingValue<double>("buttons-scale") + d, 0.5, 1.5);
        mod->setSettingValue<double>("buttons-scale", std::round(v * 20) / 20);
        refresh();
    };
    addButton("-", { 112, y }, "GJ_button_01.png", [changeSize](auto) { changeSize(-0.05); });
    m_sizeLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_sizeLabel->setScale(.35f);
    m_sizeLabel->setPosition({ 140, y });
    m_mainLayer->addChild(m_sizeLabel);
    addButton("+", { 168, y }, "GJ_button_01.png", [changeSize](auto) { changeSize(0.05); });
    // second row
    float y2 = 22;
    addButton("Hide in playtest: ON", { 85, y2 }, "GJ_button_04.png", [this](auto) {
        auto mod = Mod::get();
        mod->setSettingValue<bool>("hide-buttons-playtest", !mod->getSettingValue<bool>("hide-buttons-playtest"));
        refresh();
    }, &m_hideLabel);
    addButton("Reset", { 195, y2 }, "GJ_button_06.png", [this](auto) {
        auto mod = Mod::get();
        mod->setSettingValue<int64_t>("buttons-x", 0);
        mod->setSettingValue<int64_t>("buttons-y", 0);
        mod->setSettingValue<double>("buttons-scale", 1.0);
        mod->setSettingValue<std::string>("buttons-layout", "row");
        refresh();
    });
    addButton("Done", { 252, y2 }, "GJ_button_02.png", [this](auto) { this->onClose(nullptr); });

    refresh();
    return true;
}

void LayoutPopup::refresh() {
    auto mod = Mod::get();
    if (m_layoutLabel)
        m_layoutLabel->setString(fmt::format("Layout: {}", mod->getSettingValue<std::string>("buttons-layout")).c_str());
    if (m_hideLabel)
        m_hideLabel->setString(mod->getSettingValue<bool>("hide-buttons-playtest") ? "Hide in playtest: ON"
                                                                                     : "Hide in playtest: OFF");
    if (m_sizeLabel)
        m_sizeLabel->setString(fmt::format("{:.0f}%", mod->getSettingValue<double>("buttons-scale") * 100).c_str());
}

bool LayoutPopup::ccTouchBegan(CCTouch* touch, CCEvent* event) {
    auto p = m_mainLayer->convertTouchToNodeSpace(touch);
    auto size = m_mainLayer->getContentSize();
    // touches on the window itself behave normally
    if (p.x >= 0 && p.y >= 0 && p.x <= size.width && p.y <= size.height) return Popup::ccTouchBegan(touch, event);
    m_dragging = true;
    m_dragStart = touch->getLocation();
    m_startX = Mod::get()->getSettingValue<int64_t>("buttons-x");
    m_startY = Mod::get()->getSettingValue<int64_t>("buttons-y");
    return true;
}

void LayoutPopup::ccTouchMoved(CCTouch* touch, CCEvent* event) {
    if (!m_dragging) return Popup::ccTouchMoved(touch, event);
    auto d = touch->getLocation() - m_dragStart;
    // the editor re-reads these every frame, so the buttons follow the finger
    Mod::get()->setSettingValue<int64_t>("buttons-x", std::clamp<int64_t>(m_startX + (int64_t)std::lround(d.x), -500, 500));
    Mod::get()->setSettingValue<int64_t>("buttons-y", std::clamp<int64_t>(m_startY + (int64_t)std::lround(d.y), -300, 300));
}

void LayoutPopup::ccTouchEnded(CCTouch* touch, CCEvent* event) {
    if (!m_dragging) return Popup::ccTouchEnded(touch, event);
    m_dragging = false;
}

void LayoutPopup::ccTouchCancelled(CCTouch* touch, CCEvent* event) {
    if (!m_dragging) return Popup::ccTouchCancelled(touch, event);
    m_dragging = false;
}
