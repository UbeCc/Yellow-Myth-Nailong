#include <AzCore/Math/Vector3.h>
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Random.h>
#include "CombatNotificationBus.h"

namespace YellowMythNailong
{
    class BossAIComponent
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public CombatNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(BossAIComponent, "{B2C3D4E5-F6A7-8901-2345-678901BCDEF0}");

        static void Reflect(AZ::ReflectContext* context);

        BossAIComponent();
        ~BossAIComponent() override = default;

        void SetPlayerEntityId(AZ::EntityId id) { m_playerEntityId = id; }

        float GetHealth() const { return m_health; }
        float GetMaxHealth() const { return m_maxHealth; }
        bool IsEnraged() const { return m_enraged; }
        bool IsTelegraphing() const { return m_state == State::Telegraph; }
        bool IsCharging() const { return m_state == State::Charge; }
        AZ::Vector3 GetTelegraphTarget() const { return m_chargeTarget; }
        float GetTelegraphRadius() const { return m_chargeRadius; }

    protected:
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void OnBossDamaged(float damage) override;
        void OnBossDied() override;
        void OnPlayerDied() override;
        void OnRestartGame() override;

    private:
        enum class State
        {
            Idle,
            Chase,
            Attack,
            Stagger,
            Telegraph,
            Charge
        };

        void UpdateAI(float deltaTime);
        void PerformAttack();
        void TryFindPlayerEntity();
        void UpdateVisuals(float deltaTime);

        float m_moveSpeed = 3.0f;
        float m_attackRange = 3.0f;
        float m_attackCooldown = 2.0f;
        float m_attackDamage = 10.0f;
        float m_attackRadius = 3.5f;
        float m_maxHealth = 500.0f;
        float m_staggerDuration = 0.5f;
        float m_agroRange = 25.0f;

        // Phase two (enrage) tuning.
        float m_enragedMoveSpeed = 5.0f;
        float m_enragedAttackCooldown = 1.3f;
        float m_enragedAttackDamage = 14.0f;
        float m_chargeDamage = 18.0f;
        float m_chargeRadius = 5.0f;
        float m_chargeSpeed = 20.0f;
        float m_chargeCooldown = 6.0f;
        float m_telegraphDuration = 0.8f;

        float m_health = 500.0f;
        float m_attackTimer = 0.0f;
        State m_state = State::Idle;
        float m_staggerTimer = 0.0f;
        AZ::EntityId m_playerEntityId;

        // Game feel / presentation state.
        float m_animTime = 0.0f;
        float m_baseScale = 0.02f;
        float m_pulseTimer = 0.0f;      // grow pulse when hit
        float m_lungeTimer = 0.0f;      // forward dash during an attack
        AZ::Vector3 m_lungeDirection = AZ::Vector3::CreateAxisY();
        float m_deathTimer = 0.0f;      // sink + shrink after defeat
        bool m_engaged = false;
        bool m_enraged = false;
        bool m_playerDead = false;

        // Charge attack state.
        float m_chargeTimer = 0.0f;
        float m_telegraphTimer = 0.0f;
        AZ::Vector3 m_chargeTarget = AZ::Vector3::CreateZero();

        AZ::SimpleLcgRandom m_rng;
    };
} // namespace YellowMythNailong
