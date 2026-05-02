#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// ─── Налаштування бота ────────────────────────────────────────────────────────
static constexpr float LOOK_AHEAD    = 300.0f;  // px вперед
static constexpr float DANGER_HEIGHT = 90.0f;   // px по вертикалі
static constexpr float JUMP_COOLDOWN = 0.2f;    // сек між стрибками
static constexpr float HOLD_DURATION = 0.08f;   // скільки тримати кнопку

// ─── Глобальний стан ──────────────────────────────────────────────────────────
static bool  g_botEnabled     = false;
static bool  g_botActive      = false;
static bool  g_holding        = false;   // зараз «тримаємо» кнопку
static float g_holdTimer      = 0.0f;
static float g_lastJumpTime   = -999.0f;

// ─── Таблиці ID об'єктів ─────────────────────────────────────────────────────
static bool isDangerous(int id) {
    // шипи, пили, небезпечні блоки
    switch (id) {
        case 8: case 39: case 103: case 291:
        case 1616: case 1715: case 1614:
        case 1705: case 1706: case 1707: // більше небезпечних
            return true;
        default: return false;
    }
}

static bool isOrb(int id) {
    switch (id) {
        case 36: case 84: case 1594: case 1595:
        case 1022: case 1099: case 1696:
            return true;
        default: return false;
    }
}

static bool isPad(int id) {
    switch (id) {
        case 35: case 67: case 1333: case 1332:
            return true;
        default: return false;
    }
}

// ─── Логіка вибору стрибка ───────────────────────────────────────────────────
static bool shouldBotJump(PlayLayer* pl) {
    if (!pl || !pl->m_player1) return false;

    auto* player = pl->m_player1;
    float px = player->getPositionX();
    float py = player->getPositionY();

    // Перебираємо m_objects (офіційний API Geode)
    for (auto* obj : CCArrayExt<GameObject*>(pl->m_objects)) {
        if (!obj) continue;

        // Перевіряємо лише видимі об'єкти в зоні попереду
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

// ─── PlayLayer hook ──────────────────────────────────────────────────────────
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
            // Відпускаємо після HOLD_DURATION
            g_holdTimer += dt;
            if (g_holdTimer >= HOLD_DURATION) {
                m_player1->releaseButton(PlayerButton::Jump);
                g_holding   = false;
                g_holdTimer = 0.0f;
            }
        } else {
            // Перевіряємо чи треба стрибати
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

// ─── PlayerObject hook — щоб логувати чи push взагалі доходить ──────────────
// (можна видалити після налагодження)
struct MyPlayerObject : Modify<MyPlayerObject, PlayerObject> {
    void pushButton(PlayerButton btn) {
        PlayerObject::pushButton(btn);
        // Підраховується в CPS лічильнику гри автоматично
    }
};

// ─── PauseLayer hook ─────────────────────────────────────────────────────────
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

        auto tryAdd = [&](const char* menuId) {
            if (auto* menu = this->getChildByID(menuId)) {
                menu->addChild(btn);
                menu->updateLayout();
                return true;
            }
            return false;
        };

        if (!tryAdd("left-button-menu"))
            tryAdd("center-button-menu");
    }

    void onToggleBot(CCObject*) {
        g_botEnabled = !g_botEnabled;

        // Якщо вимикаємо — відпускаємо кнопку
        if (!g_botEnabled) {
            if (auto* pl = PlayLayer::get()) {
                if (pl->m_player1)
                    pl->m_player1->releaseButton(PlayerButton::Jump);
            }
            g_holding = false;
        }

        auto refresh = [&](const char* menuId) {
            if (auto* menu = this->getChildByID(menuId)) {
                if (auto* btn = static_cast<CCMenuItemSpriteExtra*>(
                        menu->getChildByID("bot-toggle-btn"))) {
                    auto* spr = ButtonSprite::create(
                        g_botEnabled ? "Bot: ON" : "Bot: OFF",
                        "bigFont.fnt", "GJ_button_02.png", 0.7f
                    );
                    spr->setScale(0.8f);
                    btn->setNormalImage(spr);
                    btn->setContentSize(spr->getContentSize());
                    menu->updateLayout();
