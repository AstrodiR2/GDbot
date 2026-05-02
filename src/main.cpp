#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/include/cocos2d.h>

using namespace geode::prelude;

// ─────────────────────────────────────────
//  Глобальное состояние бота
// ─────────────────────────────────────────
static bool g_botEnabled = false;   // бот вкл/выкл
static bool g_botActive  = false;   // идёт игра прямо сейчас

// Насколько далеко вперёд смотрит бот (в единицах игры)
static constexpr float LOOK_AHEAD   = 100.0f;
// Зона «опасности» по вертикали
static constexpr float DANGER_HEIGHT = 40.0f;
// Минимальный интервал между прыжками (секунды)
static constexpr float JUMP_COOLDOWN = 0.15f;

static float g_lastJumpTime = 0.0f;

// ─────────────────────────────────────────
//  Утилита: получить PlayLayer
// ─────────────────────────────────────────
static PlayLayer* getPlayLayer() {
    return PlayLayer::get();
}

// ─────────────────────────────────────────
//  Проверка: объект опасен?
//  Типы: kGameObjectTypeSpike(8), kGameObjectTypeSolid(1),
//        kGameObjectTypeGravityPad(10) и т.д.
// ─────────────────────────────────────────
static bool isDangerous(GameObject* obj) {
    if (!obj) return false;
    auto type = obj->m_objectType;
    // Шипы
    if (type == GameObjectType::Spike) return true;
    // Твёрдые блоки (стены впереди)
    if (type == GameObjectType::Solid) return true;
    return false;
}

static bool isOrb(GameObject* obj) {
    if (!obj) return false;
    auto type = obj->m_objectType;
    return (type == GameObjectType::YellowJumpPad  ||
            type == GameObjectType::PinkJumpPad    ||
            type == GameObjectType::YellowJumpRing ||
            type == GameObjectType::PinkJumpRing   ||
            type == GameObjectType::RedJumpRing    ||
            type == GameObjectType::GreenJumpRing  ||
            type == GameObjectType::BlackJumpRing);
}

// ─────────────────────────────────────────
//  Основная логика бота — вызывается каждый кадр
// ─────────────────────────────────────────
static void botThink(PlayLayer* pl, float dt) {
    if (!pl) return;

    auto* player = pl->m_player1;
    if (!player) return;

    float now = pl->m_gameState.m_levelTime;
    if (now - g_lastJumpTime < JUMP_COOLDOWN) return;

    float px = player->getPositionX();
    float py = player->getPositionY();

    bool shouldJump = false;

    // Перебираем все объекты уровня
    auto& objects = pl->m_objects;
    for (int i = 0; i < objects->count(); i++) {
        auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (!obj || !obj->isVisible()) continue;

        float ox = obj->getPositionX();
        float oy = obj->getPositionY();

        // Смотрим только вперёд
        if (ox < px + 5.0f || ox > px + LOOK_AHEAD) continue;

        // Проверяем высоту: объект на уровне игрока ± DANGER_HEIGHT
        if (std::abs(oy - py) > DANGER_HEIGHT) continue;

        if (isDangerous(obj)) {
            shouldJump = true;
            break;
        }

        if (isOrb(obj)) {
            // Для орбов прыгаем чуть позже (когда ближе)
            if (ox - px < 50.0f) {
                shouldJump = true;
                break;
            }
        }
    }

    if (shouldJump) {
        player->pushButton(PlayerButton::Jump);
        player->releaseButton(PlayerButton::Jump);
        g_lastJumpTime = now;
    }
}

// ─────────────────────────────────────────
//  Хук PlayLayer: старт/конец уровня
// ─────────────────────────────────────────
struct MyPlayLayer : Modify<MyPlayLayer, PlayLayer> {

    // Уровень запустился
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_botActive = true;
        g_lastJumpTime = 0.0f;
        return true;
    }

    // Каждый кадр
    void update(float dt) {
        PlayLayer::update(dt);
        if (g_botEnabled && g_botActive) {
            botThink(this, dt);
        }
    }

    // Смерть игрока
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
    }

    // Уровень пройден — если бот включён, блокируем прогресс
    void levelComplete() {
        if (g_botEnabled) {
            // Обнуляем прогресс чтобы не засчитался
            auto* level = m_level;
            if (level) {
                // Сохраняем оригинальный процент
                int savedPercent = level->m_normalPercent.value();
                // Вызываем оригинальный метод
                PlayLayer::levelComplete();
                // Восстанавливаем процент обратно
                level->m_normalPercent = savedPercent;
            }
            // Показываем уведомление
            Notification::create(
                "Уровень пройден ботом — прогресс не засчитан!",
                NotificationIcon::Warning
            )->show();
        } else {
            PlayLayer::levelComplete();
        }
    }

    // Выход с уровня
    void onQuit() {
        g_botActive = false;
        PlayLayer::onQuit();
    }
};

// ─────────────────────────────────────────
//  Хук PauseLayer: кнопка вкл/выкл бота
// ─────────────────────────────────────────
struct MyPauseLayer : Modify<MyPauseLayer, PauseLayer> {

    void customSetup() {
        PauseLayer::customSetup();

        // Создаём кнопку
        auto* label = CCLabelBMFont::create(
            g_botEnabled ? "Bot: ON" : "Bot: OFF",
            "bigFont.fnt"
        );
        label->setScale(0.5f);
        label->setColor(g_botEnabled ? ccColor3B{0, 255, 100} : ccColor3B{255, 80, 80});

        auto* btn = CCMenuItemLabel::create(label, this, menu_selector(MyPauseLayer::onToggleBot));

        // Находим меню паузы и добавляем кнопку
        if (auto* menu = this->getChildByID("right-button-menu")) {
            btn->setID("bot-toggle-btn");
            menu->addChild(btn);
            menu->updateLayout();
        }
    }

    void onToggleBot(CCObject*) {
        g_botEnabled = !g_botEnabled;

        // Обновляем текст кнопки
        if (auto* menu = this->getChildByID("right-button-menu")) {
            if (auto* btn = static_cast<CCMenuItemLabel*>(menu->getChildByID("bot-toggle-btn"))) {
                if (auto* lbl = dynamic_cast<CCLabelBMFont*>(btn->getLabel())) {
                    lbl->setString(g_botEnabled ? "Bot: ON" : "Bot: OFF");
                    lbl->setColor(g_botEnabled ? ccColor3B{0, 255, 100} : ccColor3B{255, 80, 80});
                }
            }
        }

        Notification::create(
            g_botEnabled ? "Бот включён" : "Бот выключен",
            g_botEnabled ? NotificationIcon::Success : NotificationIcon::Info
        )->show();
    }
};
