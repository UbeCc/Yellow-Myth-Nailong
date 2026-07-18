#include <AzCore/Math/Vector3.h>
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include "CombatNotificationBus.h"

namespace YellowMythNailong
{
    class CombatManagerComponent
        : public AZ::Component
        , public CombatNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(CombatManagerComponent, "{C3D4E5F6-A7B8-9012-3456-789012CDEF01}");

        static void Reflect(AZ::ReflectContext* context);

        CombatManagerComponent();
        ~CombatManagerComponent() override = default;

        void SetPlayerEntityId(AZ::EntityId id) { m_playerEntityId = id; }
        void SetBossEntityId(AZ::EntityId id) { m_bossEntityId = id; }

    protected:
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        void OnPlayerAttack(const AZ::Vector3& position, float radius, float damage) override;
        void OnBossAttack(const AZ::Vector3& position, float radius, float damage) override;
        void OnPlayerDied() override;
        void OnBossDied() override;

    private:
        void TryFindEntities();

    private:
        AZ::EntityId m_playerEntityId;
        AZ::EntityId m_bossEntityId;
        bool m_gameOver = false;
    };
} // namespace YellowMythNailong
