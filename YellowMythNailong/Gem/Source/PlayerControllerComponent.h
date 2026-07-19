#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include "CombatNotificationBus.h"

namespace YellowMythNailong
{
    class PlayerControllerComponent
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public AzFramework::InputChannelEventListener
        , public CombatNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(PlayerControllerComponent, "{A1B2C3D4-E5F6-7890-1234-567890ABCDEF}");

        static void Reflect(AZ::ReflectContext* context);

        PlayerControllerComponent();
        ~PlayerControllerComponent() override = default;

        void SetCameraEntityId(AZ::EntityId id) { m_cameraEntityId = id; }

        float GetHealth() const { return m_health; }
        float GetMaxHealth() const { return m_maxHealth; }

    protected:
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        // CombatNotificationBus
        void OnPlayerDamaged(float damage) override;
        void OnPlayerDied() override;
        void OnRestartGame() override;

    private:
        void Move(const AZ::Vector3& direction, float deltaTime);
        void PerformDodge();
        void PerformAttack();
        void UpdateCamera();
        void TryFindCameraEntity();

        void UpdateMovementFromKeyboard();

        float m_moveSpeed = 5.0f;
        float m_dodgeSpeed = 18.0f;
        float m_dodgeDuration = 0.25f;
        float m_attackCooldown = 0.5f;
        float m_attackRadius = 4.0f;
        float m_attackDamage = 25.0f;
        float m_maxHealth = 100.0f;

        float m_health = 100.0f;
        AZ::Vector3 m_inputDirection = AZ::Vector3::CreateZero();
        bool m_isDodging = false;
        float m_dodgeTimer = 0.0f;
        AZ::Vector3 m_dodgeDirection = AZ::Vector3::CreateZero();
        float m_attackTimer = 0.0f;
        bool m_attackRequested = false;

        // Raw key state tracking
        bool m_keyW = false;
        bool m_keyA = false;
        bool m_keyS = false;
        bool m_keyD = false;
        bool m_keySpace = false;

        // Third-person camera settings
        float m_cameraDistance = 6.0f;
        float m_cameraHeight = 2.5f;
        float m_cameraLookAtHeight = 1.5f;

        bool m_broadcastGameStart = true;

        AZ::EntityId m_cameraEntityId;
    };
} // namespace YellowMythNailong
