#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

static bool g_botEnabled = false;
static bool g_botActive  = false;
static constexpr float LOOK_AHEAD    = 150.0f;
static constexpr float DANGER_HEIGHT = 50.0f;
static constexpr float JUMP_COOLDOWN = 0.1f;
static float g_lastJumpTime = 0.0f;

static bool isDangerous(GameObject* obj) {
    if (!obj) return false;
    int id = obj->m_objectID;
    return (id == 8 || id == 39 || id == 103 || id == 291 ||
            id == 1616 || id == 1715 || id == 1614);
}

static bool isOrb(GameObject* obj) {
    if (!obj) return false;
    int id = obj->m_objectID;
    return (id == 36 || id == 84 || id == 1594 ||
            id == 1595 || id == 1022 || id == 1099);
}

static bool isPad(GameObject* obj) {
    if (!obj) return false;
    int id = obj->m_objectID;
    return (id == 35 || id == 67 || id == 1333 || id == 1332);
}

static void botThink(PlayLayer* pl) {
    if (!pl) return;
    auto* player = pl->m_player1;
    if (!player) return;

    float now = pl->m_gameState.m_levelTime;
    if (now - g_lastJumpTime < JUMP_COOLDOWN) return;

    float px = player->getPositionX();
    float py = player->getPositionY();
    bool shouldJump = false;

    // Перебираем секции уровня напрямую
    for (auto* section : pl->m_sectionObjects) {
        if (!section) continue;
        for (auto* obj : *section) {
            if (!obj || !obj->isVisible()) continue;

            float ox = obj->getPositionX();
            float oy = obj->getPositionY();

            if (ox < px + 5.0f || ox > px + LOOK_AHEAD) continue;
            if (std::abs(oy - py) > DANGER_HEIGHT) continue;

            if (isDangerous(obj) || isPad(obj)) {
                shouldJump = true;
                break;
            }
            if (isOrb(obj) && (ox - px) < 60.0f) {
                shouldJump = true;
                break;
            }
        }
        if (shouldJump) break;
    }

    if (shouldJump) {
        player->pushButton(PlayerButton::Jump);
        player->releaseButton(PlayerButton::Jump);
        g_lastJumpTime = now;
    }
}

struct MyPlayLayer : Modify<MyPlayLayer, PlayLayer> {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_botActive = true;
        g_lastJumpTime = 0.0f;
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (g_botEnabled && g_botActive) {
            botThink(this);
        }
    }

    void levelComplete() {
        if (g_botEnabled) {
            int savedPercent = m_level->m_normalPercent.value();
            PlayLayer::levelComplete();
            m_level->m_normalPercent = savedPercent;
            Notification::create(
                "Бот прошёл уровень — прогресс не засчитан!",
                NotificationIcon::Warning
            )->show();
        } else {
            PlayLayer::levelComplete();
        }
    }

    void onQuit() {
        g_botActive = false;
        PlayLayer::onQuit();
    }
};

struct MyPauseLayer : Modify<MyPauseLayer, PauseLayer> {
    void customSetup() {
        PauseLayer::customSetup();

        auto* spr = ButtonSprite::create(
            g_botEnabled ? "Bot: ON" : "Bot: OFF",
            "bigFont.fnt",
            "GJ_button_02.png",
            0.7f
        );
        spr->setScale(0.8f);

        auto* btn = CCMenuItemSpriteExtra::create(
            spr, this,
            menu_selector(MyPauseLayer::onToggleBot)
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

    void onToggleBot(CCObject*) {
        g_botEnabled = !g_botEnabled;

        auto updateMenu = [&](const char* menuId) {
            if (auto* menu = this->getChildByID(menuId)) {
                if (auto* btn = static_cast<CCMenuItemSpriteExtra*>(
                        menu->getChildByID("bot-toggle-btn"))) {
                    auto* newSpr = ButtonSprite::create(
                        g_botEnabled ? "Bot: ON" : "Bot: OFF",
                        "bigFont.fnt",
                        "GJ_button_02.png",
                        0.7f
                    );
                    newSpr->setScale(0.8f);
                    btn->setNormalImage(newSpr);
                    btn->setContentSize(newSpr->getContentSize());
                    menu->updateLayout();
                }
            }
        };

        updateMenu("left-button-menu");
        updateMenu("center-button-menu");

        Notification::create(
            g_botEnabled ? "Бот включён" : "Бот выключен",
            g_botEnabled ? NotificationIcon::Success : NotificationIcon::Info
        )->show();
    }
};
