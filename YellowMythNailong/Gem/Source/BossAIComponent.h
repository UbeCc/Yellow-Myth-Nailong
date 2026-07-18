#include <AzCore/Math/Vector3.h>
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
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

    protected:
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void OnPlayerAttack(const AZ::Vector3& position, float radius, float damage) override;
        void OnBossDamaged(float damage) override;
        void OnBossDied() override;

    private:
        enum class State
        {
            Idle,
            Chase,
            Attack,
            Stagger
        };

        void UpdateAI(float deltaTime);
        void PerformAttack();
        void TryFindPlayerEntity();

        float m_moveSpeed = 3.0f;
        float m_attackRange = 3.0f;
        float m_attackCooldown = 2.0f;
        float m_attackDamage = 15.0f;
        float m_attackRadius = 3.5f;
        float m_maxHealth = 300.0f;
        float m_staggerDuration = 0.5f;
        float m_agroRange = 25.0f;

        float m_health = 300.0f;
        float m_attackTimer = 0.0f;
        State m_state = State::Idle;
        float m_staggerTimer = 0.0f;
        AZ::EntityId m_playerEntityId;
    };
} // namespace YellowMythNailong
