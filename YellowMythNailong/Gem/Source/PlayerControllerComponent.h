#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Random.h>
#include <AzCore/std/containers/vector.h>
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
        float GetSpitCooldownRemaining() const { return m_spitCooldown; }
        float GetSpitCooldownMax() const { return m_spitCooldownMax; }
        int GetFocus() const { return m_focus; }
        int GetMaxFocus() const { return m_maxFocus; }

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
        void OnBossDamaged(float damage) override;
        void OnPlayerDied() override;
        void OnRestartGame() override;

    private:
        void Move(const AZ::Vector3& direction, float deltaTime);
        void PerformDodge();
        void PerformAttack();
        void PerformHeavyAttack();
        void FireSpit();
        void UpdateSpit(float deltaTime);
        void ResolveRockCollision();
        void UpdateCamera(float deltaTime);
        void TryFindCameraEntity();
        void TryFindPartEntities();
        void CacheRockPositions();
        void UpdateVisuals(float deltaTime);
        void UpdatePartAnimations(float deltaTime);

        void UpdateMovementFromKeyboard();

        float m_moveSpeed = 5.0f;
        float m_dodgeSpeed = 18.0f;
        float m_dodgeDuration = 0.35f;
        float m_attackCooldown = 0.5f;
        float m_attackRadius = 6.5f;
        float m_attackDamage = 40.0f;
        float m_comboFinisherDamage = 70.0f;
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

        // Game feel state.
        float m_animTime = 0.0f;
        float m_baseScale = 1.2f;
        float m_shakeAmplitude = 0.0f;    // camera shake, decays exponentially
        float m_hitstopTimer = 0.0f;      // brief freeze when a hit connects
        float m_pulseTimer = 0.0f;        // grow pulse on attack
        float m_flinchTimer = 0.0f;       // shrink flinch when hurt
        float m_lungeTimer = 0.0f;        // forward dash during an attack
        AZ::Vector3 m_lungeDirection = AZ::Vector3::CreateAxisY();
        float m_lungeSpeed = 0.0f;
        int m_comboCount = 0;             // 3-hit chain: 40 / 40 / 70 damage
        float m_comboTimer = 0.0f;
        float m_deathTimer = 0.0f;        // sink into the ground after defeat

        // Focus economy: landed light hits build points, the F heavy spends them all.
        int m_focus = 0;
        int m_maxFocus = 3;
        float m_heavyDamageBase = 70.0f;
        float m_heavyDamagePerFocus = 40.0f;

        // Presentation extras.
        AZ::EntityId m_visualPartId;      // NailongVisual child (roll / tilt target)
        AZ::EntityId m_eyePartIds[4];     // EyeL / EyeR / PupilL / PupilR (blink target)
        AZ::EntityId m_bossEntityId;      // for hit knockback direction
        float m_blinkTimer = 2.5f;        // time until the next blink
        float m_blinkActiveTimer = 0.0f;  // > 0 while the eyes are closed
        float m_knockbackTimer = 0.0f;
        AZ::Vector3 m_knockbackDirection = AZ::Vector3::CreateAxisY();
        bool m_finisherSpinActive = false;  // full-body spin during the combo finisher

        // Directional body language: lean into movement, face the camera when idle.
        AZ::Vector3 m_lastMoveDirection = AZ::Vector3::CreateAxisY();
        float m_idleTimer = 0.0f;
        float m_visualYaw = 0.0f;
        float m_leanPitch = 0.0f;
        float m_leanRoll = 0.0f;

        AZ::Vector3 m_cameraSmoothedPos = AZ::Vector3::CreateZero();

        // Static boulder positions for camera anti-occlusion and player collision.
        AZStd::vector<AZ::Vector3> m_rockPositions;
        bool m_rocksCached = false;

        // Milk spit (Q): a slow white projectile that arcs toward the boss.
        bool m_spitActive = false;
        AZ::Vector3 m_spitPosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_spitDirection = AZ::Vector3::CreateAxisY();
        float m_spitTimer = 0.0f;
        float m_spitCooldown = 0.0f;
        float m_spitDebugTimer = 0.0f;
        float m_spitCooldownMax = 3.0f;
        bool m_cameraSmoothedInit = false;
        AZ::SimpleLcgRandom m_rng;
    };
} // namespace YellowMythNailong
