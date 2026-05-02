#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

static bool g_botEnabled = false;
static bool g_botActive  = false;
static constexpr float LOOK_AHEAD    = 100.0f;
static constexpr float DANGER_HEIGHT = 40.0f;
static constexpr float JUMP_COOLDOWN = 0.15f;
static float g_lastJumpTime = 0.0f;

static bool isDangerous(GameObject* obj) {
    if (!obj) return false;
    int id = obj->m_objectID;
    // Шипы (ID объектов шипов в GD)
    return (id == 8 || id == 39 || id == 103 || id == 291);
}

static bool isOrb(GameObject* obj) {
    if (!obj) return false;
    int id = obj->m_objectID;
    // Жёлтый орб=36, розовый=84, красный=1594,
    // синий=1595, зелёный=1022, чёрный=1099
    return (id == 36 || id == 84 || id == 1594 ||
            id == 1595 || id == 1022 || id == 1099);
}

static bool isPad(GameObject* obj) {
    if (!obj) return false;
    int id = obj->m_objectID;
    // Жёлтый пад=35, розовый=67, красный=1333, синий=1332
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

    auto& objects = pl->m_objects;
    for (int i = 0; i < (int)objects->count(); i++) {
        auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (!obj || !obj->isVisible()) continue;

        float ox = obj->getPositionX();
        float oy = obj->getPositionY();

        if (ox < px + 5.0f || ox > px + LOOK_AHEAD) continue;
        if (std::abs(oy - py) > DANGER_HEIGHT) continue;

        if (isDangerous(obj)) {
            shouldJump = true;
            break;
        }
        if (isPad(obj)) {
            shouldJump = true;
            break;
        }
        if (isOrb(obj) && ox - px < 50.0f) {
            shouldJump = true;
            break;
        }
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

        auto* label = CCLabelBMFont::create(
            g_botEnabled ? "Bot: ON" : "Bot: OFF",
            "bigFont.fnt"
        );
        label->setScale(0.5f);
        label->setColor(g_botEnabled
            ? ccColor3B{0, 255, 100}
            : ccColor3B{255, 80, 80});

        auto* btn = CCMenuItemLabel::create(
            label, this,
            menu_selector(MyPauseLayer::onToggleBot)
        );

        if (auto* menu = this->getChildByID("right-button-menu")) {
            btn->setID("bot-toggle-btn");
            menu->addChild(btn);
            menu->updateLayout();
        }
    }

    void onToggleBot(CCObject*) {
        g_botEnabled = !g_botEnabled;

        if (auto* menu = this->getChildByID("right-button-menu")) {
            if (auto* btn = static_cast<CCMenuItemLabel*>(
                    menu->getChildByID("bot-toggle-btn"))) {
                if (auto* lbl = dynamic_cast<CCLabelBMFont*>(btn->getLabel())) {
                    lbl->setString(g_botEnabled ? "Bot: ON" : "Bot: OFF");
                    lbl->setColor(g_botEnabled
                        ? ccColor3B{0, 255, 100}
                        : ccColor3B{255, 80, 80});
                }
            }
        }

        Notification::create(
            g_botEnabled ? "Бот включён" : "Бот выключен",
            g_botEnabled
                ? NotificationIcon::Success
                : NotificationIcon::Info
        )->show();
    }
};
