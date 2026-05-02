#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// ─── Налаштування ────────────────────────────────────────────────────────────
static constexpr float LOOK_AHEAD    = 300.0f;
static constexpr float DANGER_HEIGHT = 90.0f;
static constexpr float JUMP_COOLDOWN = 0.2f;
static constexpr float HOLD_DURATION = 0.08f;

// ─── Глобальний стан ─────────────────────────────────────────────────────────
static bool  g_botEnabled     = false;
static bool  g_botActive      = false;
static bool  g_holding        = false;
static float g_holdTimer      = 0.0f;
static float g_lastJumpTime   = -999.0f;

// ─── ID об'єктів ─────────────────────────────────────────────────────────────
static bool isDangerous(int id) {
    switch (id) {
        case 8: case 39: case 103: case 291:
        case 1616: case 1715: case 1614:
        case 1705: case 1706: case 1707:
            return true;
        default:
            return false;
    }
}

static bool isOrb(int id) {
    switch (id) {
        case 36: case 84: case 1594: case 1595:
        case 1022: case 1099: case 1696:
            return true;
        default:
            return false;
    }
}

static bool isPad(int id) {
    switch (id) {
        case 35: case 67: case 1333: case 1332:
            return true;
        default:
            return false;
    }
}

// ─── Логіка стрибка ──────────────────────────────────────────────────────────
static bool shouldBotJump(PlayLayer* pl) {
    if (!pl || !pl->m_player1) return false;

    float px = pl->m_player1->getPositionX();
    float py = pl->m_player1->getPositionY();

    for (auto* obj : CCArrayExt<GameObject*>(pl->m_objects)) {
        if (!obj) continue;

        float ox = obj->getPositionX();
        if (ox < px + 5.0f || ox > px + LOOK_AHEAD) continue;

        float oy = obj->getPositionY();
        if (std::abs(oy - py) > DANGER_HEIGHT) continue;

        int id = obj->m_objectID;

        if (isDangerous(id) || isPad(id)) return true;
        if (isOrb(id) && (ox - px) < 100.0f) return true;
    }
    return false;
}

// ─── PlayLayer ───────────────────────────────────────────────────────────────
struct MyPlayLayer : Modify<MyPlayLayer, PlayLayer> {

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_botActive    = true;
        g_holding      = false;
        g_holdTimer    = 0.0f;
        g_lastJumpTime = -999.0f;
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (!g_botEnabled || !g_botActive || !m_player1) return;

        float now = m_gameState.m_levelTime;

        if (g_holding) {
            g_holdTimer += dt;
            if (g_holdTimer >= HOLD_DURATION) {
                m_player1->releaseButton(PlayerButton::Jump);
                g_holding   = false;
                g_holdTimer = 0.0f;
            }
        } else {
            if ((now - g_lastJumpTime) >= JUMP_COOLDOWN && shouldBotJump(this)) {
                m_player1->pushButton(PlayerButton::Jump);
                g_holding      = true;
                g_holdTimer    = 0.0f;
                g_lastJumpTime = now;
            }
        }
    }

    void levelComplete() {
        if (g_botEnabled) {
            int saved = m_level->m_normalPercent.value();
            PlayLayer::levelComplete();
            m_level->m_normalPercent = saved;
            Notification::create(
                "Бот пройшов рівень — прогрес не зараховано!",
                NotificationIcon::Warning
            )->show();
        } else {
            PlayLayer::levelComplete();
        }
    }

    void onQuit() {
        g_botActive = false;
        g_holding   = false;
        if (m_player1) m_player1->releaseButton(PlayerButton::Jump);
        PlayLayer::onQuit();
    }
};

// ─── PauseLayer ──────────────────────────────────────────────────────────────
struct MyPauseLayer : Modify<MyPauseLayer, PauseLayer> {

    void customSetup() {
        PauseLayer::customSetup();

        auto* spr = ButtonSprite::create(
            g_botEnabled ? "Bot: ON" : "Bot: OFF",
            "bigFont.fnt", "GJ_button_02.png", 0.7f
        );
        spr->setScale(0.8f);

        auto* btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(MyPauseLayer::onToggleBot)
        );
        btn->setID("bot-toggle-btn");

        if (auto* menu = this->getChildByID("left-button-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else if (auto* menu = this->getChildByID("center-button-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
    }

    void refreshBotButton(const char* menuId) {
        auto* menu = this->getChildByID(menuId);
        if (!menu) return;

        auto* btn = static_cast<CCMenuItemSpriteExtra*>(
            menu->getChildByID("bot-toggle-btn")
        );
        if (!btn) return;

        auto* spr = ButtonSprite::create(
            g_botEnabled ? "Bot: ON" : "Bot: OFF",
            "bigFont.fnt", "GJ_button_02.png", 0.7f
        );
        spr->setScale(0.8f);
        btn->setNormalImage(spr);
        btn->setContentSize(spr->getContentSize());
        menu->updateLayout();
    }

    void onToggleBot(CCObject*) {
        g_botEnabled = !g_botEnabled;

        if (!g_botEnabled) {
            if (auto* pl = PlayLayer::get()) {
                if (pl->m_player1)
                    pl->m_player1->releaseButton(PlayerButton::Jump);
            }
            g_holding = false;
        }

        refreshBotButton("left-button-menu");
        refreshBotButton("center-button-menu");

        Notification::create(
            g_botEnabled ? "Бот увімкнено" : "Бот вимкнено",
            g_botEnabled ? NotificationIcon::Success : NotificationIcon::Info
        )->show();
    }
};
