#pragma once
#include <Geode/Geode.hpp>

// Small see-through window over the editor (from the pause menu): drag anywhere to move the
// BPM / WAVE / TIME / 1/N buttons, change their size, arrangement and playtest visibility - live.
class LayoutPopup : public geode::Popup {
public:
    static void open();

protected:
    bool init();
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
    cocos2d::CCLabelBMFont* m_hideLabel = nullptr;
    void refresh();
};
