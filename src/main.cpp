#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Базовая скорость прыжка куба в GD
static constexpr double BASE_JUMP_VELOCITY = 11.180032;

class $modify(AirJumpPlayer, PlayerObject) {
    struct Fields {
        int airJumpsUsed = 0;      // сколько воздушных прыжков уже потрачено
        float graceTime = 0.f;     // короткая пауза, чтобы не сбросить счётчик сразу после прыжка
        bool holdingJump = false;  // зажат ли экран сейчас
        float holdTimeLeft = 0.f;  // сколько ещё можно "лететь" в режиме Hold
        float holdPower = 0.f;     // сила полёта в режиме Hold
    };

    bool airJumpAllowed() {
        auto mod = Mod::get();
        if (!PlayLayer::get()) return false;
        if (!mod->getSettingValue<bool>("enabled")) return false;
        if (mod->getSettingValue<bool>("platformer-only") && !m_isPlatformer) return false;
        // Воздушный прыжок только для куба и робота
        if (m_isShip || m_isBird || m_isBall || m_isDart || m_isSwing || m_isSpider) return false;
        return !m_isDead;
    }

    bool pushButton(PlayerButton button) {
        if (button != PlayerButton::Jump) {
            return PlayerObject::pushButton(button);
        }

        m_fields->holdingJump = true;

        // На земле или мод выключен - обычное поведение игры
        if (m_isOnGround || !airJumpAllowed()) {
            return PlayerObject::pushButton(button);
        }

        auto mod = Mod::get();
        int maxJumps = static_cast<int>(mod->getSettingValue<int64_t>("air-jumps"));
        if (m_fields->airJumpsUsed >= maxJumps) {
            return PlayerObject::pushButton(button);
        }

        m_fields->airJumpsUsed++;
        m_fields->graceTime = 0.1f;

        double power = mod->getSettingValue<double>("jump-power");
        double before = m_yVelocity;

        // Обманываем игру: "игрок на земле", чтобы она сама сделала прыжок
        m_isOnGround = true;
        bool result = PlayerObject::pushButton(button);

        if (m_yVelocity != before) {
            // Игра прыгнула сама - только масштабируем силу
            m_yVelocity *= power;
            m_isOnGround = false;
        } else if (!m_isRobot) {
            // Запасной вариант: задаём скорость прыжка вручную
            double v = BASE_JUMP_VELOCITY * power;
            m_yVelocity = m_isUpsideDown ? -v : v;
            m_isOnGround = false;
        }

        if (mod->getSettingValue<std::string>("mode") == "Hold") {
            m_fields->holdTimeLeft = static_cast<float>(mod->getSettingValue<double>("hold-time"));
            m_fields->holdPower = static_cast<float>(mod->getSettingValue<double>("hold-power"));
        }

        return result;
    }

    bool releaseButton(PlayerButton button) {
        if (button == PlayerButton::Jump) {
            m_fields->holdingJump = false;
            m_fields->holdTimeLeft = 0.f;
        }
        return PlayerObject::releaseButton(button);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        // Сброс счётчика при приземлении
        if (m_fields->graceTime > 0.f) {
            m_fields->graceTime -= dt;
        } else if (m_isOnGround) {
            m_fields->airJumpsUsed = 0;
            m_fields->holdTimeLeft = 0.f;
        }

        // Режим Hold: пока держишь экран - поднимаемся вверх
        if (m_fields->holdingJump && m_fields->holdTimeLeft > 0.f && !m_isOnGround && !m_isDead) {
            m_fields->holdTimeLeft -= dt;
            double target = BASE_JUMP_VELOCITY * m_fields->holdPower;
            double dir = m_isUpsideDown ? -1.0 : 1.0;
            if (m_yVelocity * dir < target) {
                m_yVelocity = target * dir;
            }
        }
    }
};
