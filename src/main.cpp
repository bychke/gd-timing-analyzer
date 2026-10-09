// Geometry Dash Timing Analyzer - osu!-style timing editor for the Geometry Dash level editor.
#include "Session.hpp"
#include "Options.hpp"
#include "audio/Metronome.hpp"
#include "ui/LayoutPopup.hpp"
#include "ui/TimingEditor.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/DrawGridLayer.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/SongInfoObject.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>
#include <Geode/modify/CustomSongLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/MusicDownloadManager.hpp>

using namespace geode::prelude;

// "Timing" button in the Custom Song Selection window
class $modify(TimingCustomSongLayer, CustomSongLayer) {
    bool init(CustomSongDelegate* delegate) {
        if (!CustomSongLayer::init(delegate)) return false;

        auto spr = ButtonSprite::create("Timing", "bigFont.fnt", "GJ_button_02.png", .7f);
        spr->setScale(.55f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [](auto) { TimingEditor::open(); });
        btn->setID("timing-editor-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("timing-editor-menu"_spr);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition(win / 2 + CCPoint{ 197, 72 });
        menu->addChild(btn);
        m_mainLayer->addChild(menu, 10);
        handleTouchPriority(this);
        Session::get().fixOldSongId();
        return true;
    }
};

// Mouse wheel inside the timing editor
class $modify(CCMouseDispatcher) {
    bool dispatchScrollMSG(float y, float x) {
        if (TimingEditor::s_current) return TimingEditor::s_current->onScroll(-y, x);
        return CCMouseDispatcher::dispatchScrollMSG(y, x);
    }
};

// ---------------- local songs: a song ID can point to any file on this PC ----------------
class $modify(MusicDownloadManager) {
    static void onModify(auto& self) {
        // run before other song mods (NONG managers) so our local files win
        for (auto name : { "MusicDownloadManager::pathForSong", "MusicDownloadManager::isSongDownloaded",
                           "MusicDownloadManager::getSongInfoObject" })
            (void)self.setHookPriority(name, -10000);
    }

    gd::string pathForSong(int id) {
        if (auto p = Session::get().songOverride(id)) return utils::string::pathToString(*p);
        return MusicDownloadManager::pathForSong(id);
    }

    bool isSongDownloaded(int id) {
        if (Session::get().songOverride(id)) return true;
        return MusicDownloadManager::isSongDownloaded(id);
    }

    SongInfoObject* getSongInfoObject(int id) {
        auto p = Session::get().songOverride(id);
        if (!p) return MusicDownloadManager::getSongInfoObject(id);
        // show the local file's name in the song widget
        static std::map<int, Ref<SongInfoObject>> cache;
        // objects handed to the game are never freed (song widgets may still point at an older one)
        static std::vector<Ref<SongInfoObject>> keepAlive;
        auto& obj = cache[id];
        auto name = utils::string::pathToString(p->stem());
        if (!obj || obj->m_songName != name) {
            if (obj) keepAlive.push_back(obj);
            // fully initialized object (the short create leaves fields the song widget reads)
            obj = SongInfoObject::create(id, name, "Local file", 0, 0.f, "", "", "", "", 0, "", false, 0, 0);
        }
        return obj;
    }
};

// ---------------- level editor: metronome button + grid position ----------------
class $modify(TimingEditorUI, EditorUI) {
    struct Fields {
        metronome::Tracker tracker;
        CCLabelBMFont* gridLabel = nullptr;
        CCLabelBMFont* centerLabel = nullptr;
        CCPoint labelBase;
        CCMenuItemToggler* toggle = nullptr;
        CCMenuItemToggler* waveToggle = nullptr;
        CCLabelBMFont* divLabel = nullptr;
        // BPM, WAVE, TIME, 1/N (placed by the buttons-* settings)
        CCMenu* menu = nullptr;
        std::array<CCNode*, 4> buttons{};
        CCPoint menuBase;
        float step = 0;
        // background waveform
        CCDrawNode* waveNode = nullptr;
        AudioData const* waveAudio = nullptr;
        float waveNorm = 1.f;
        std::array<float, 6> waveKey{};
        int waveFrames = 0;
        CCDrawNode* guideNode = nullptr;
        std::array<float, 7> guideKey{};
        int guideFrames = 0;
        bool guideInit = false;
    };

    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        // use the saved timing of this level's song
        auto& s = Session::get();
        // a different level starts clean: its own song, its own timing and settings
        s.enterLevel();

        if (m_playbackBtn && m_playbackBtn->getParent()) {
            float target = m_playbackBtn->getScaledContentSize().height * 0.8f;
            auto makeSpr = [target](char const* text, bool on, CircleBaseColor color) {
                auto lbl = CCLabelBMFont::create(fmt::format("{}\n{}", text, on ? "ON" : "OFF").c_str(), "bigFont.fnt");
                lbl->setAlignment(kCCTextAlignmentCenter);
                lbl->setScale(.45f);
                auto spr = CircleButtonSprite::create(lbl, on ? color : CircleBaseColor::Gray, CircleBaseSize::Small);
                spr->setScale(target / spr->getContentSize().height);
                return spr;
            };
            auto onSpr = makeSpr("BPM", true, CircleBaseColor::Green);
            auto offSpr = makeSpr("BPM", false, CircleBaseColor::Green);

            auto toggle = CCMenuItemExt::createToggler(onSpr, offSpr, [](CCMenuItemToggler* t) {
                // the callback runs before the toggler flips
                Session::get().setEditorMetronome(!t->isToggled());
            });
            toggle->toggle(s.editorMetronome());
            toggle->setID("metronome-toggle"_spr);
            m_fields->toggle = toggle;

            auto world = m_playbackBtn->getParent()->convertToWorldSpace(m_playbackBtn->getPosition());
            auto local = this->convertToNodeSpace(world);
            float gap = m_playbackBtn->getScaledContentSize().width * 0.5f + target * 0.6f + 6;

            float step = target + 4;
            // WAVE ON/OFF: semi-transparent waveform of the song behind the level
            auto waveToggle = CCMenuItemExt::createToggler(makeSpr("WAVE", true, CircleBaseColor::Cyan),
                makeSpr("WAVE", false, CircleBaseColor::Cyan), [](CCMenuItemToggler* t) {
                    Session::get().setEditorWaveform(!t->isToggled());
                });
            waveToggle->toggle(s.editorWaveform());
            waveToggle->setID("waveform-toggle"_spr);
            m_fields->waveToggle = waveToggle;

            // TIME: opens the timing editor at the current moment of the level (and comes back on close)
            auto timeSpr = CircleButtonSprite::createWithSprite("note_wave.png"_spr, 1.f, CircleBaseColor::Blue,
                CircleBaseSize::Small);
            timeSpr->setScale(target / timeSpr->getContentSize().height);
            auto wave = CCMenuItemExt::createSpriteExtra(timeSpr, [](auto) { TimingEditor::openFromEditor(); });
            wave->setID("timing-editor-button"_spr);

            // 1/N: guideline divisor of the whole level (cycles and redraws the guidelines right away)
            auto divLbl = CCLabelBMFont::create(fmt::format("1/{}", s.guideDivisor()).c_str(), "bigFont.fnt");
            divLbl->setScale(.45f);
            auto divSpr = CircleButtonSprite::create(divLbl, CircleBaseColor::Pink, CircleBaseSize::Small);
            divSpr->setScale(target / divSpr->getContentSize().height);
            auto divBtn = CCMenuItemExt::createSpriteExtra(divSpr, [divLbl](auto) {
                auto& s = Session::get();
                static int const divs[] = { 1, 2, 3, 4, 6, 8 };
                int cur = s.guideDivisor(), next = divs[0];
                for (int d : divs)
                    if (d > cur) { next = d; break; }
                s.setGuideDivisor(next);
                divLbl->setString(fmt::format("1/{}", next).c_str());
                s.applyGuidelines();
            });
            divBtn->setID("guide-divisor-button"_spr);
            m_fields->divLabel = divLbl;

            auto menu = CCMenu::create();
            menu->setID("metronome-menu"_spr);
            menu->setPosition(local + CCPoint{ gap, 0 });
            menu->addChild(toggle);
            menu->addChild(waveToggle);
            menu->addChild(wave);
            menu->addChild(divBtn);
            this->addChild(menu, 10);
            m_fields->menu = menu;
            m_fields->buttons = { toggle, waveToggle, wave, divBtn };
            m_fields->menuBase = local + CCPoint{ gap, 0 };
            m_fields->step = step;
            applyButtonLayout();

            // centered under the top bar, away from the buttons of GD and other mods
            auto win = CCDirector::get()->getWinSize();
            auto label = CCLabelBMFont::create("", "chatFont.fnt");
            label->setID("grid-position"_spr);
            label->setScale(.5f);
            label->setAnchorPoint({ .5f, .5f });
            label->setPosition({ win.width / 2, win.height - 68 });
            this->addChild(label, 10);
            m_fields->gridLabel = label;

            // second line: where the middle of the screen is (the line that activates triggers while scrolling)
            auto center = CCLabelBMFont::create("", "chatFont.fnt");
            center->setID("center-position"_spr);
            center->setScale(.5f);
            center->setPosition({ win.width / 2, win.height - 56 });
            this->addChild(center, 10);
            m_fields->centerLabel = center;
            m_fields->labelBase = CCPoint{ win.width / 2, win.height - 56 };
        }

        this->schedule(schedule_selector(TimingEditorUI::onTimingTick));
        return true;
    }

    // Position / size / arrangement of the buttons from the mod settings (re-read every frame, so changes show up live)
    void applyButtonLayout() {
        auto& f = m_fields;
        if (!f->menu) return;
        auto mod = Mod::get();
        f->menu->setPosition(f->menuBase + CCPoint{ (float)opt::get<int64_t>("buttons-x"),
                                                    (float)opt::get<int64_t>("buttons-y") });
        f->menu->setScale((float)opt::get<double>("buttons-scale"));
        bool playtest = m_editorLayer && m_editorLayer->m_playbackMode != PlaybackMode::Not;
        f->menu->setVisible(!(playtest && opt::get<bool>("hide-buttons-playtest")));
        auto layout = opt::get<std::string>("buttons-layout");
        float s = f->step;
        // hidden buttons (Editor Settings) leave no gap: the shown ones move together
        static char const* const showKeys[] = { "show-btn-bpm", "show-btn-wave", "show-btn-time", "show-btn-div" };
        int n = 0;
        for (int i = 0; i < 4; i++) {
            if (!f->buttons[i]) continue;
            bool show = opt::get<bool>(showKeys[i]);
            f->buttons[i]->setVisible(show);
            if (!show) continue;
            CCPoint pos = layout == "column" ? CCPoint{ 0, -s * n }
                        : layout == "2x2"    ? CCPoint{ s * (n % 2), -s * (n / 2) }
                                             : CCPoint{ s * n, 0 };
            f->buttons[i]->setPosition(pos);
            n++;
        }
    }

    // Draws the song's waveform behind the objects, aligned with the level (speed portals + song offset)
    void updateBackgroundWaveform() {
        auto& s = Session::get();
        auto lel = m_editorLayer;
        auto grid = lel ? lel->m_drawGridLayer : nullptr;
        auto parent = grid ? grid->getParent() : nullptr;
        if (!parent || !lel->m_levelSettings) return;

        auto& f = m_fields;
        if (!s.editorWaveform()) {
            if (f->waveNode) f->waveNode->setVisible(false);
            return;
        }
        // decode the level's song if needed
        auto song = Session::currentLevelSongPath();
        // only decode when nothing is loaded yet - never replace the song the Timing window works on
        if (!s.unloaded && !s.busy && !TimingEditor::s_current && !song.empty() &&
            (s.audioPath.empty() || (s.audioPath == song && !s.audio)))
            s.loadAudio(song);
        auto audio = s.audio.get();
        if (!audio || audio->peakMax.empty() || s.audioPath != song) {
            if (f->waveNode) f->waveNode->setVisible(false);
            return;
        }

        if (!f->waveNode) {
            f->waveNode = CCDrawNode::create();
            f->waveNode->setID("background-waveform"_spr);
            // right above the grid, under the objects
            parent->addChild(f->waveNode, grid->getZOrder());
        }
        auto node = f->waveNode;
        node->setVisible(true);

        if (f->waveAudio != audio) {
            f->waveAudio = audio;
            float amp = 0.f;
            for (size_t i = 0; i < audio->peakMax.size(); i += 16)
                amp = std::max({ amp, audio->peakMax[i], -audio->peakMin[i] });
            f->waveNorm = amp > 0 ? amp : 1.f;
            f->waveKey = {};
        }

        auto win = CCDirector::get()->getWinSize();
        auto bl = parent->convertToNodeSpace({ 0, 0 });
        auto tr = parent->convertToNodeSpace({ win.width, win.height });
        float offset = lel->m_levelSettings->m_songOffset;
        std::array<float, 6> key{ bl.x, bl.y, tr.x, tr.y, offset, (float)lel->m_objects->count() };
        // redraw only when the view changes (and now and then for edited speed portals)
        if (key == f->waveKey && ++f->waveFrames < 60) return;
        f->waveKey = key;
        f->waveFrames = 0;
        node->clear();

        float cy = (bl.y + tr.y) / 2;
        float t0 = std::max(0.f, lel->timeForPos({ bl.x, cy }, 0, 0, false, 0));
        float t1 = lel->timeForPos({ tr.x, cy }, 0, 0, false, 0);
        if (t1 <= t0) return;
        float h = (tr.y - bl.y) * 0.3f;
        constexpr int N = 360;
        auto wc = opt::get<ccColor4B>("editor-waveform-color");
        ccColor4F fill{ wc.r / 255.f, wc.g / 255.f, wc.b / 255.f, wc.a / 255.f };
        float prevX = 0;
        for (int i = 0; i <= N; i++) {
            float t = t0 + (t1 - t0) * i / N;
            float x = lel->posForTime(t).x;
            if (i > 0 && x > prevX) {
                double a = (t0 + (t1 - t0) * (i - 1) / N + offset) * 1000.0;
                double b = (t + offset) * 1000.0;
                if (b > 0 && a < audio->lengthMs) {
                    auto [mn, mx] = audio->range(std::max(0.0, a), b);
                    float amp = std::min(1.f, std::max(std::abs(mn), std::abs(mx)) / f->waveNorm) * h;
                    if (amp > 0.5f) {
                        CCPoint quad[4] = { { prevX, cy - amp }, { x, cy - amp }, { x, cy + amp }, { prevX, cy + amp } };
                        node->drawPolygon(quad, 4, fill, 0, fill);
                    }
                }
            }
            prevX = x;
        }
    }

    // Guidelines in the waveform's colors (GD's own guidelines only have a few fixed colors)
    void updateCustomGuidelines() {
        auto& s = Session::get();
        auto lel = m_editorLayer;
        auto grid = lel ? lel->m_drawGridLayer : nullptr;
        auto parent = grid ? grid->getParent() : nullptr;
        if (!parent || !lel->m_levelSettings) return;
        auto& f = m_fields;
        // custom lines aren't saved in the level: with "Automatic guidelines" draw them once when the editor opens
        if (!f->guideInit && !s.busy && s.timingIsForCurrentLevel()) {
            f->guideInit = true;
            s.autoGuidelines();
        }
        if (!s.customGuidesOn || s.map.empty()) {
            if (f->guideNode) f->guideNode->setVisible(false);
            return;
        }
        if (!f->guideNode) {
            f->guideNode = CCDrawNode::create();
            f->guideNode->setID("custom-guidelines"_spr);
            parent->addChild(f->guideNode, grid->getZOrder());
        }
        auto node = f->guideNode;
        node->setVisible(true);

        auto win = CCDirector::get()->getWinSize();
        auto bl = parent->convertToNodeSpace({ 0, 0 });
        auto tr = parent->convertToNodeSpace({ win.width, win.height });
        std::array<float, 7> key{ bl.x, bl.y, tr.x, tr.y, lel->m_levelSettings->m_songOffset,
                                  (float)s.guidesRev, (float)s.guideDivisor() };
        if (key == f->guideKey && ++f->guideFrames < 60) return;
        f->guideKey = key;
        f->guideFrames = 0;
        node->clear();

        float cy = (bl.y + tr.y) / 2;
        float offset = lel->m_levelSettings->m_songOffset;
        double t0 = (std::max(0.f, lel->timeForPos({ bl.x, cy }, 0, 0, false, 0)) + offset) * 1000.0;
        double t1 = (lel->timeForPos({ tr.x, cy }, 0, 0, false, 0) + offset) * 1000.0;
        if (t1 <= t0) return;
        auto mod = Mod::get();
        float radius = (float)opt::get<double>("guideline-thickness") / std::max(0.05f, parent->getScale());
        auto col = [&](char const* id) {
            auto v = opt::get<ccColor4B>(id);
            return ccColor4F{ v.r / 255.f, v.g / 255.f, v.b / 255.f, v.a / 255.f };
        };
        // osu! beat snap colors: the smallest snap (1/1 ... 1/16) the position lands on
        constexpr int SNAPS[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
        ccColor4F colors[8];
        for (int i = 0; i < 8; i++) colors[i] = col(fmt::format("guide-color-{}", SNAPS[i]).c_str());
        s.map.forEachClick(t0, t1, s.guideDivisor(), [&](double t, TickKind k) {
            ccColor4F c = colors[7];
            int idx = s.map.indexAt(t);
            if (k != TickKind::Sub || idx < 0) c = colors[0];
            else {
                auto const& p = s.map.points[idx];
                double pos = std::fmod((t - p.time) / p.beatLength + 1000.0, 1.0);
                for (int i = 0; i < 8; i++) {
                    double v = pos * SNAPS[i];
                    if (std::abs(v - std::round(v)) < 2e-3 * SNAPS[i]) { c = colors[i]; break; }
                }
            }
            float x = lel->posForTime((float)(t / 1000.0 - offset)).x;
            node->drawSegment({ x, bl.y }, { x, tr.y }, radius, c);
        });
    }

    void updateCenterLabel(bool playing) {
        auto label = m_fields->centerLabel;
        if (!label) return;
        // both texts follow the offset from the settings (draggable in the Text tab, re-read every frame)
        auto mod = Mod::get();
        CCPoint off{ (float)opt::get<int64_t>("text-x"), (float)opt::get<int64_t>("text-y") };
        label->setPosition(m_fields->labelBase + off);
        if (m_fields->gridLabel) m_fields->gridLabel->setPosition(m_fields->labelBase + off - CCPoint{ 0, 12 });
        auto& s = Session::get();
        auto lel = m_editorLayer;
        bool show = opt::get<bool>("show-object-beat") && !playing && !s.map.empty() &&
                    lel && lel->m_objectLayer;
        label->setVisible(show);
        if (!show) return;
        auto win = CCDirector::get()->getWinSize();
        auto t = Session::songTimeAtLevelPos(lel->m_objectLayer->convertToNodeSpace({ win.width / 2, win.height / 2 }));
        if (!t) return;
        auto g = s.map.locate(*t, 4.0);
        auto text = "Screen center: " + g.describe();
        if (text != label->getString()) label->setString(text.c_str());
        if (!opt::get<bool>("center-text-colors")) label->setColor({ 255, 255, 255 });
        else label->setColor(g.den == 0 ? ccColor3B{ 200, 200, 255 } : ccColor3B{ 120, 220, 255 });
    }

    void onTimingTick(float) {
        applyButtonLayout();
        updateBackgroundWaveform();
        updateCustomGuidelines();
        auto& s = Session::get();
        auto engine = FMODAudioEngine::get();
        bool playing = engine->isMusicPlaying(0);

        if (playing) {
            double pos = engine->getMusicTimeMS(0);
            if (s.editorMetronome()) m_fields->tracker.update(pos, s.map);
            else m_fields->tracker.reset();
        } else {
            m_fields->tracker.reset();
        }

        auto label = m_fields->gridLabel;
        if (!label) return;
        label->setVisible(opt::get<bool>("show-object-beat"));
        updateCenterLabel(playing);
        if (m_fields->toggle && m_fields->toggle->isToggled() != s.editorMetronome())
            m_fields->toggle->toggle(s.editorMetronome());
        if (m_fields->waveToggle && m_fields->waveToggle->isToggled() != s.editorWaveform())
            m_fields->waveToggle->toggle(s.editorWaveform());
        // the divisor can also be changed in the Timing window
        if (m_fields->divLabel) {
            auto text = fmt::format("1/{}", s.guideDivisor());
            if (text != m_fields->divLabel->getString()) m_fields->divLabel->setString(text.c_str());
        }

        if (s.map.empty()) {
            label->setString("No timing - open Custom Song > Timing");
            label->setColor({ 180, 180, 180 });
            return;
        }
        if (playing) {
            auto g = s.map.locate(engine->getMusicTimeMS(0), 2.0);
            label->setString(fmt::format("Music: Bar {}  Beat {}", g.bar, g.beat).c_str());
            label->setColor({ 255, 255, 255 });
            return;
        }
        GameObject* obj = m_selectedObject;
        if (!obj && m_selectedObjects && m_selectedObjects->count() > 0)
            obj = static_cast<GameObject*>(m_selectedObjects->objectAtIndex(0));
        if (!obj) {
            label->setString("Select an object to see its beat");
            label->setColor({ 180, 180, 180 });
            return;
        }
        auto t = Session::songTimeAtLevelPos(obj->getPosition());
        if (!t) return;
        // one editor unit is ~3 ms at normal speed, so allow a bit of tolerance
        auto g = s.map.locate(*t, 4.0);
        label->setString(("Object: " + g.describe()).c_str());
        label->setColor(g.den == 0 ? ccColor3B{ 255, 120, 120 } : ccColor3B{ 140, 255, 140 });
    }
};

// ---------------- editor pause menu: button that opens the editor settings preview ----------------
class $modify(TimingEditorPauseLayer, EditorPauseLayer) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorPauseLayer::init(lel)) return false;

        auto spr = CircleButtonSprite::createWithSprite("note_wave.png"_spr, 1.f, CircleBaseColor::Green,
            CircleBaseSize::Medium);
        // all Timing Analyzer settings in a window over the pause menu
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [](auto) {
            // every setting of the mod in one window (the live editor preview is in TIME > Editor Settings)
            openSettingsPopup(Mod::get(), false);
        });
        btn->setID("editor-buttons-layout"_spr);

        // next to Help (and BetterEdit's button) at the bottom, or an own menu without node IDs
        if (auto menu = this->getChildByIDRecursive("guidelines-menu")) {
            if (auto first = menu->getChildren() ? static_cast<CCNode*>(menu->getChildren()->objectAtIndex(0)) : nullptr)
                spr->setScale(first->getScaledContentSize().height / spr->getContentSize().height);
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto win = CCDirector::get()->getWinSize();
            auto own = CCMenu::create();
            own->setID("layout-menu"_spr);
            own->setPosition({ win.width / 2 + 90, 30 });
            spr->setScale(.8f);
            own->addChild(btn);
            this->addChild(own, 10);
        }
        return true;
    }
};

// ---------------- drag & drop of files (Windows) ----------------
#ifdef GEODE_IS_WINDOWS
#include <shellapi.h>

static WNDPROC g_origProc = nullptr;

static void onFileDropped(std::filesystem::path path) {
    if (TimingEditor::s_current) {
        TimingEditor::s_current->handleDroppedFile(path);
        return;
    }
    if (!LevelEditorLayer::get()) return; // editor only
    TimingEditor::open();
    if (TimingEditor::s_current) TimingEditor::s_current->handleDroppedFile(path);
}

static LRESULT CALLBACK dropWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DROPFILES) {
        auto drop = (HDROP)wp;
        UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::filesystem::path> files;
        for (UINT i = 0; i < n; i++) {
            UINT len = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring w(len, L'\0');
            DragQueryFileW(drop, i, w.data(), len + 1);
            files.emplace_back(w);
        }
        DragFinish(drop);
        // audio first, then .osu (so loading the audio does not overwrite the imported timing)
        std::stable_sort(files.begin(), files.end(), [](auto const& a, auto const& b) {
            return (a.extension() == ".osu") < (b.extension() == ".osu");
        });
        queueInMainThread([files] { for (auto const& f : files) onFileDropped(f); });
        return 0;
    }
    return CallWindowProcW(g_origProc, hwnd, msg, wp, lp);
}

$on_mod(Loaded) {
    queueInMainThread([] {
        HWND hwnd = WindowFromDC(wglGetCurrentDC());
        if (!hwnd) {
            log::warn("GD window not found - drag & drop disabled");
            return;
        }
        DragAcceptFiles(hwnd, TRUE);
        g_origProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)dropWndProc);
    });
}
#endif
