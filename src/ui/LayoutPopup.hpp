#pragma once
#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>

// Small see-through window over the editor (from the pause menu): drag anywhere to move the
// BPM / WAVE / TIME / 1/N buttons, change their size, arrangement and playtest visibility, and the
// waveform / guideline colors - live.
class LayoutPopup : public geode::Popup {
public:
    // tab: 0 buttons, 1 text, 2 guideline colors. returnToPause: Done goes back to the pause menu + settings
    static void open(int tab = 0, bool returnToPause = false);

protected:
    bool init();
    void onClose(cocos2d::CCObject* sender) override;
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;

private:
    bool m_dragging = false;
    cocos2d::CCPoint m_dragStart;
    int64_t m_startX = 0;
    int64_t m_startY = 0;
    cocos2d::CCLabelBMFont* m_layoutLabel = nullptr;
    cocos2d::CCLabelBMFont* m_sizeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_thicknessLabel = nullptr;
    std::vector<std::pair<std::string, cocos2d::CCLayerColor*>> m_swatches;
    std::vector<std::pair<CCMenuItemToggler*, std::string>> m_checks; // checkbox + its option (Reset updates them)
    std::vector<std::pair<cocos2d::CCNode*, cocos2d::CCMenu*>> m_pages;
    ButtonSprite* m_tabs[3] = {};
    int m_tab = 0;
    int m_startTab = 0;
    bool m_returnToPause = false;
    char const* m_dragKeyX = "buttons-x";
    char const* m_dragKeyY = "buttons-y";
    cocos2d::CCDrawNode* m_highlight = nullptr;
    float m_time = 0;
    void update(float dt) override;
    void showTab(int tab);
    void resetTab();
    void refresh();
};
