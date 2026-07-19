#include "PlayerControllerComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/ILogger.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <Atom/RPI.Public/ViewportContext.h>

namespace YellowMythNailong
{
    PlayerControllerComponent::PlayerControllerComponent()
        : AzFramework::InputChannelEventListener(false)
    {
    }

    void PlayerControllerComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<PlayerControllerComponent, AZ::Component>()
                ->Version(3)
                ->Field("MoveSpeed", &PlayerControllerComponent::m_moveSpeed)
                ->Field("DodgeSpeed", &PlayerControllerComponent::m_dodgeSpeed)
                ->Field("DodgeDuration", &PlayerControllerComponent::m_dodgeDuration)
                ->Field("AttackCooldown", &PlayerControllerComponent::m_attackCooldown)
                ->Field("AttackRadius", &PlayerControllerComponent::m_attackRadius)
                ->Field("AttackDamage", &PlayerControllerComponent::m_attackDamage)
                ->Field("MaxHealth", &PlayerControllerComponent::m_maxHealth)
                ->Field("CameraEntityId", &PlayerControllerComponent::m_cameraEntityId)
                ->Field("CameraDistance", &PlayerControllerComponent::m_cameraDistance)
                ->Field("CameraHeight", &PlayerControllerComponent::m_cameraHeight)
                ->Field("CameraLookAtHeight", &PlayerControllerComponent::m_cameraLookAtHeight)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<PlayerControllerComponent>("Player Controller", "Controls the Nailong player character")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Yellow Myth")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_moveSpeed, "Move Speed", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_dodgeSpeed, "Dodge Speed", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_dodgeDuration, "Dodge Duration", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_attackCooldown, "Attack Cooldown", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_attackRadius, "Attack Radius", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_attackDamage, "Attack Damage", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_maxHealth, "Max Health", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_cameraEntityId, "Camera Entity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_cameraDistance, "Camera Distance", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_cameraHeight, "Camera Height", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &PlayerControllerComponent::m_cameraLookAtHeight, "Camera Look-At Height", "")
                    ;
            }
        }
    }

    void PlayerControllerComponent::Init()
    {
    }

    void PlayerControllerComponent::Activate()
    {
        AZLOG_INFO("PlayerControllerComponent activated on entity %s", GetEntityId().ToString().c_str());
        AZ::TickBus::Handler::BusConnect();
        CombatNotificationBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();

        // TransformComponent ignores the prefab "Scale" field at runtime; set the
        // Nailong character to a playable size (the chubby model is ~1.7 m tall at 1.0).
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale);

        m_health = m_maxHealth;
        m_keyW = m_keyA = m_keyS = m_keyD = m_keySpace = false;

        TryFindCameraEntity();
    }

    void PlayerControllerComponent::Deactivate()
    {
        AzFramework::InputChannelEventListener::Disconnect();
        CombatNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    bool PlayerControllerComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const AZStd::string channelName = inputChannel.GetInputChannelId().GetName();
        const AzFramework::InputChannel::State state = inputChannel.GetState();
        const bool pressed = (state == AzFramework::InputChannel::State::Began);
        const bool released = (state == AzFramework::InputChannel::State::Ended);

        // The first move/dodge/attack dismisses the title screen.
        if (pressed && m_broadcastGameStart)
        {
            m_broadcastGameStart = false;
            CombatNotificationBus::Broadcast(&CombatNotifications::OnGameStart);
        }

        if (channelName == "keyboard_key_alphanumeric_W")
        {
            m_keyW = !released;
        }
        else if (channelName == "keyboard_key_alphanumeric_A")
        {
            m_keyA = !released;
        }
        else if (channelName == "keyboard_key_alphanumeric_S")
        {
            m_keyS = !released;
        }
        else if (channelName == "keyboard_key_alphanumeric_D")
        {
            m_keyD = !released;
        }
        else if (channelName == "keyboard_key_edit_space")
        {
            if (pressed && !m_keySpace)
            {
                PerformDodge();
            }
            m_keySpace = !released;
        }
        else if (channelName == "keyboard_key_edit_enter" || channelName == "mouse_button_left")
        {
            if (pressed)
            {
                m_attackRequested = true;
            }
        }
        else if (channelName == "keyboard_key_alphanumeric_R")
        {
            if (pressed)
            {
                CombatNotificationBus::Broadcast(&CombatNotifications::OnRestartGame);
            }
        }
        else if (channelName == "keyboard_key_alphanumeric_Q")
        {
            if (pressed)
            {
                FireSpit();
            }
        }
        else
        {
            return false;
        }
        return true;
    }

    void PlayerControllerComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        m_animTime += deltaTime;
        TryFindPartEntities();

        // Death animation: the chubby hero deflates into the ground, then holds.
        if (m_health <= 0.0f)
        {
            if (m_deathTimer > 0.0f)
            {
                m_deathTimer -= deltaTime;
                const float t = AZ::GetClamp(1.0f - m_deathTimer / 1.2f, 0.0f, 1.0f);
                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
                pos.SetZ(-1.4f * t);
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, pos);
                AZ::TransformBus::Event(
                    GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale * (1.0f - 0.4f * t));
            }
            UpdatePartAnimations(deltaTime);
            UpdateCamera(deltaTime);
            return;
        }

        if (m_attackTimer > 0.0f)
        {
            m_attackTimer -= deltaTime;
        }
        if (m_comboTimer > 0.0f)
        {
            m_comboTimer -= deltaTime;
            if (m_comboTimer <= 0.0f)
            {
                m_comboCount = 0;
            }
        }

        // Hitstop: freeze the world for a few frames when a hit connects.
        if (m_hitstopTimer > 0.0f)
        {
            m_hitstopTimer -= deltaTime;
            UpdateCamera(deltaTime);
            return;
        }

        UpdateMovementFromKeyboard();

        if (m_knockbackTimer > 0.0f)
        {
            // Getting smacked shoves the little hero backwards.
            m_knockbackTimer -= deltaTime;
            Move(m_knockbackDirection * (8.0f / m_moveSpeed), deltaTime);
        }
        else if (m_isDodging)
        {
            m_dodgeTimer -= deltaTime;
            if (m_dodgeTimer <= 0.0f)
            {
                m_isDodging = false;
            }
            else
            {
                Move(m_dodgeDirection, deltaTime);
            }
        }
        else if (m_lungeTimer > 0.0f)
        {
            m_lungeTimer -= deltaTime;
            Move(m_lungeDirection * (m_lungeSpeed / m_moveSpeed), deltaTime);
        }
        else
        {
            if (m_inputDirection.GetLengthSq() > 0.001f)
            {
                m_inputDirection.Normalize();
                m_lastMoveDirection = m_inputDirection;
                m_idleTimer = 0.0f;
                Move(m_inputDirection, deltaTime);
            }
            else
            {
                m_idleTimer += deltaTime;
            }

            if (m_attackRequested && m_attackTimer <= 0.0f)
            {
                PerformAttack();
                m_attackTimer = m_attackCooldown;
                m_attackRequested = false;
            }
        }

        // Attacks requested mid-dodge/knockback stay buffered for the next free frame.
        m_inputDirection = AZ::Vector3::CreateZero();

        UpdateSpit(deltaTime);
        ResolveRockCollision();
        UpdateVisuals(deltaTime);
        UpdatePartAnimations(deltaTime);
        TryFindCameraEntity();
        UpdateCamera(deltaTime);
    }

    void PlayerControllerComponent::UpdateMovementFromKeyboard()
    {
        m_inputDirection = AZ::Vector3::CreateZero();
        if (m_keyW)
        {
            m_inputDirection.SetY(m_inputDirection.GetY() + 1.0f);
        }
        if (m_keyS)
        {
            m_inputDirection.SetY(m_inputDirection.GetY() - 1.0f);
        }
        if (m_keyD)
        {
            m_inputDirection.SetX(m_inputDirection.GetX() + 1.0f);
        }
        if (m_keyA)
        {
            m_inputDirection.SetX(m_inputDirection.GetX() - 1.0f);
        }
    }

    void PlayerControllerComponent::OnPlayerDamaged(float damage)
    {
        if (m_isDodging)
        {
            return; // invulnerable while dodging
        }
        m_health -= damage;
        m_shakeAmplitude = AZ::GetMin(m_shakeAmplitude + 0.35f, 0.7f);
        m_flinchTimer = 0.12f;

        // Knock back away from the boss.
        if (m_bossEntityId.IsValid())
        {
            AZ::Vector3 bossPos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(bossPos, m_bossEntityId, &AZ::TransformInterface::GetWorldTranslation);
            AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(playerPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
            AZ::Vector3 away = playerPos - bossPos;
            away.SetZ(0.0f);
            if (away.GetLengthSq() > 0.001f)
            {
                m_knockbackDirection = away.GetNormalized();
                m_knockbackTimer = 0.18f;
            }
        }

        AZLOG_INFO("Player health: %.1f", m_health);
        if (m_health <= 0.0f)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerDied);
        }
    }

    void PlayerControllerComponent::OnBossDamaged(float /*damage*/)
    {
        // The player's own hit connected: freeze frame + a touch of shake.
        m_hitstopTimer = 0.06f;
        m_shakeAmplitude = AZ::GetMin(m_shakeAmplitude + 0.12f, 0.4f);
    }

    void PlayerControllerComponent::OnPlayerDied()
    {
        AZLOG_INFO("Player died!");
        m_deathTimer = 1.2f;
    }

    void PlayerControllerComponent::OnRestartGame()
    {
        // Reset the player to full health at the spawn point so a new round can start.
        m_health = m_maxHealth;
        m_isDodging = false;
        m_dodgeTimer = 0.0f;
        m_attackTimer = 0.0f;
        m_attackRequested = false;
        m_keyW = m_keyA = m_keyS = m_keyD = m_keySpace = false;
        m_hitstopTimer = 0.0f;
        m_pulseTimer = 0.0f;
        m_flinchTimer = 0.0f;
        m_lungeTimer = 0.0f;
        m_comboCount = 0;
        m_comboTimer = 0.0f;
        m_deathTimer = 0.0f;
        m_shakeAmplitude = 0.0f;
        m_knockbackTimer = 0.0f;
        m_blinkActiveTimer = 0.0f;
        m_blinkTimer = 2.5f;
        m_finisherSpinActive = false;
        m_cameraSmoothedInit = false;
        m_spitActive = false;
        m_spitCooldown = 0.0f;
        if (m_visualPartId.IsValid())
        {
            AZ::TransformBus::Event(m_visualPartId, &AZ::TransformInterface::SetLocalRotationQuaternion, AZ::Quaternion::CreateIdentity());
        }
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale);
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, AZ::Vector3::CreateZero());
    }

    void PlayerControllerComponent::Move(const AZ::Vector3& direction, float deltaTime)
    {
        AZ::Vector3 currentTranslation = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(currentTranslation, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        float speed = m_isDodging ? m_dodgeSpeed : m_moveSpeed;
        AZ::Vector3 newPos = currentTranslation + direction * speed * deltaTime;
        newPos.SetZ(0.0f); // keep on ground

        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, newPos);

        if (!m_isDodging && direction.GetLengthSq() > 0.001f)
        {
            AZ::Quaternion targetRotation = AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisY(), direction.GetNormalized());
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldRotationQuaternion, targetRotation);
        }
    }

    void PlayerControllerComponent::FireSpit()
    {
        // Q: hock up a milk loogie at the boss. Slow, white, gross, effective.
        if (m_spitActive || m_spitCooldown > 0.0f || m_health <= 0.0f || m_isDodging)
        {
            return;
        }

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        AZ::Vector3 targetDir = AZ::Vector3::CreateZero();
        bool hasTarget = false;
        if (m_bossEntityId.IsValid())
        {
            AZ::Vector3 bossPos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(bossPos, m_bossEntityId, &AZ::TransformInterface::GetWorldTranslation);
            targetDir = bossPos - playerPos;
            targetDir.SetZ(0.0f);
            hasTarget = targetDir.GetLengthSq() > 0.5f;
        }
        if (!hasTarget)
        {
            AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
            AZ::TransformBus::EventResult(rotation, GetEntityId(), &AZ::TransformInterface::GetWorldRotationQuaternion);
            targetDir = rotation.TransformVector(AZ::Vector3::CreateAxisY());
            targetDir.SetZ(0.0f);
        }
        if (targetDir.GetLengthSq() < 0.001f)
        {
            targetDir = AZ::Vector3::CreateAxisY();
        }
        targetDir.Normalize();

        m_spitActive = true;
        m_spitTimer = 2.0f;
        m_spitPosition = playerPos + AZ::Vector3(0.0f, 0.0f, 1.0f);
        m_spitDirection = targetDir;
        m_spitCooldown = m_spitCooldownMax;
        AZLOG_INFO("Nailong spits!");
    }

    void PlayerControllerComponent::UpdateSpit(float deltaTime)
    {
        if (m_spitCooldown > 0.0f)
        {
            m_spitCooldown -= deltaTime;
        }
        if (!m_spitActive)
        {
            return;
        }

        m_spitTimer -= deltaTime;

        // Gently home in on the boss so the loogie actually connects.
        if (m_bossEntityId.IsValid())
        {
            AZ::Vector3 bossPos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(bossPos, m_bossEntityId, &AZ::TransformInterface::GetWorldTranslation);
            AZ::Vector3 toBoss = bossPos - m_spitPosition;
            toBoss.SetZ(0.0f);
            if (toBoss.GetLengthSq() > 0.001f)
            {
                m_spitDebugTimer -= deltaTime;
                if (m_spitDebugTimer <= 0.0f)
                {
                    m_spitDebugTimer = 0.3f;
                    AZLOG_INFO("Spit at (%.1f,%.1f), boss %.1fm away", m_spitPosition.GetX(), m_spitPosition.GetY(), toBoss.GetLength());
                }
                toBoss.Normalize();
                m_spitDirection = (m_spitDirection + toBoss * 4.0f * deltaTime).GetNormalized();
            }
        }

        m_spitPosition += m_spitDirection * 18.0f * deltaTime;
        CombatNotificationBus::Broadcast(&CombatNotifications::OnProjectileTick, m_spitPosition, false);

        if (m_bossEntityId.IsValid())
        {
            AZ::Vector3 bossPos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(bossPos, m_bossEntityId, &AZ::TransformInterface::GetWorldTranslation);
            AZ::Vector3 delta = bossPos - m_spitPosition;
            delta.SetZ(0.0f);
            if (delta.GetLengthSq() <= 2.5f * 2.5f)
            {
                CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerAttack, m_spitPosition, 3.0f, 30.0f);
                m_spitActive = false;
                return;
            }
        }
        if (m_spitTimer <= 0.0f)
        {
            m_spitActive = false;
        }
    }

    void PlayerControllerComponent::ResolveRockCollision()
    {
        // The boulders are solid: shove the little hero out of their spheres.
        CacheRockPositions();
        AZ::Vector3 pos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
        for (const AZ::Vector3& rock : m_rockPositions)
        {
            const float minDist = 2.0f;
            AZ::Vector3 delta = pos - rock;
            delta.SetZ(0.0f);
            const float dist = delta.GetLength();
            if (dist < minDist && dist > 0.001f)
            {
                pos = rock + delta * (minDist / dist);
                pos.SetZ(0.0f);
            }
        }
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, pos);
    }

    void PlayerControllerComponent::PerformDodge()
    {
        if (m_isDodging)
        {
            return;
        }
        m_isDodging = true;
        m_dodgeTimer = m_dodgeDuration;
        m_dodgeDirection = m_inputDirection.GetLengthSq() > 0.001f ? m_inputDirection.GetNormalized() : AZ::Vector3::CreateAxisY();
        CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerDodged);
        AZLOG_INFO("Nailong dodges!");
    }

    void PlayerControllerComponent::PerformAttack()
    {
        AZ::Vector3 pos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        // 3-hit combo: the third strike hits much harder and lunges further.
        const bool finisher = (m_comboCount == 2);
        const float damage = finisher ? m_comboFinisherDamage : m_attackDamage;
        AZLOG_INFO("Nailong attacks! (combo %d)", m_comboCount + 1);
        CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerAttack, pos, m_attackRadius, damage);

        if (finisher)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerComboStrike);
        }
        m_comboCount = (m_comboCount + 1) % 3;
        m_comboTimer = 1.2f;

        // Lunge toward the current facing for a meaty swing.
        AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
        AZ::TransformBus::EventResult(rotation, GetEntityId(), &AZ::TransformInterface::GetWorldRotationQuaternion);
        m_lungeDirection = rotation.TransformVector(AZ::Vector3::CreateAxisY());
        m_lungeDirection.SetZ(0.0f);
        if (m_lungeDirection.GetLengthSq() < 0.001f)
        {
            m_lungeDirection = AZ::Vector3::CreateAxisY();
        }
        m_lungeDirection.Normalize();
        m_lungeSpeed = finisher ? 10.0f : 7.0f;
        m_lungeTimer = finisher ? 0.32f : 0.12f;
        m_pulseTimer = 0.15f;
        m_finisherSpinActive = finisher;
    }

    void PlayerControllerComponent::UpdateVisuals(float deltaTime)
    {
        if (m_pulseTimer > 0.0f)
        {
            m_pulseTimer -= deltaTime;
        }
        if (m_flinchTimer > 0.0f)
        {
            m_flinchTimer -= deltaTime;
        }

        // Idle breathing + attack pulse + hurt flinch, all as one uniform scale.
        float scale = m_baseScale;
        scale *= 1.0f + 0.025f * sinf(m_animTime * 2.2f);
        if (m_pulseTimer > 0.0f)
        {
            scale *= 1.0f + 0.18f * (m_pulseTimer / 0.15f);
        }
        if (m_flinchTimer > 0.0f)
        {
            scale *= 1.0f - 0.15f * (m_flinchTimer / 0.12f);
        }
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, scale);
    }

    void PlayerControllerComponent::TryFindPartEntities()
    {
        if (m_visualPartId.IsValid() && m_eyePartIds[0].IsValid() && m_bossEntityId.IsValid())
        {
            return;
        }

        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }

        appRequests->EnumerateEntities([this](AZ::Entity* entity)
        {
            if (!entity)
            {
                return true;
            }
            const AZStd::string& name = entity->GetName();
            if (name == "NailongVisual")
            {
                m_visualPartId = entity->GetId();
            }
            else if (name == "NailongEyeL")
            {
                m_eyePartIds[0] = entity->GetId();
            }
            else if (name == "NailongEyeR")
            {
                m_eyePartIds[1] = entity->GetId();
            }
            else if (name == "NailongPupilL")
            {
                m_eyePartIds[2] = entity->GetId();
            }
            else if (name == "NailongPupilR")
            {
                m_eyePartIds[3] = entity->GetId();
            }
            else if (name == "DarkBoss")
            {
                m_bossEntityId = entity->GetId();
            }
            return true;
        });
    }

    void PlayerControllerComponent::UpdatePartAnimations(float deltaTime)
    {
        // Body roll while dodging, a forward lean while lunging at the foe,
        // and a full spin while the combo finisher is out.
        float pitch = 0.0f;
        float spin = 0.0f;
        if (m_isDodging)
        {
            const float progress = 1.0f - AZ::GetClamp(m_dodgeTimer / m_dodgeDuration, 0.0f, 1.0f);
            pitch = AZ::Constants::TwoPi * progress;
        }
        else if (m_finisherSpinActive)
        {
            const float progress = 1.0f - AZ::GetClamp(m_lungeTimer / 0.32f, 0.0f, 1.0f);
            spin = AZ::Constants::TwoPi * progress;
            if (m_lungeTimer <= 0.0f)
            {
                m_finisherSpinActive = false;
            }
        }
        else if (m_lungeTimer > 0.0f)
        {
            pitch = 0.30f * (m_lungeTimer / 0.12f);
        }
        if (m_visualPartId.IsValid())
        {
            // Lean into the current movement direction (player-local frame).
            AZ::Quaternion playerRot = AZ::Quaternion::CreateIdentity();
            AZ::TransformBus::EventResult(playerRot, GetEntityId(), &AZ::TransformInterface::GetWorldRotationQuaternion);
            const AZ::Vector3 localMove = playerRot.GetConjugate().TransformVector(m_lastMoveDirection);
            const bool moving = (m_idleTimer < 0.1f);
            const float targetPitch = moving ? 0.14f * localMove.GetY() : 0.0f;
            const float targetRoll = moving ? 0.12f * localMove.GetX() : 0.0f;
            const float leanBlend = AZ::GetMin(1.0f, 10.0f * deltaTime);
            m_leanPitch += (targetPitch - m_leanPitch) * leanBlend;
            m_leanRoll += (targetRoll - m_leanRoll) * leanBlend;

            // Stand still for a breath and Nailong turns to look at you.
            const float yawTarget = (m_idleTimer > 1.5f && !m_isDodging && m_lungeTimer <= 0.0f) ? AZ::Constants::Pi : 0.0f;
            const float yawDelta = yawTarget - m_visualYaw;
            const float yawStep = AZ::GetClamp(yawDelta, -2.5f * deltaTime, 2.5f * deltaTime);
            m_visualYaw += yawStep;

            const AZ::Quaternion rotation =
                AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisZ(), m_visualYaw + spin) *
                AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisY(), m_leanRoll) *
                AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisX(), pitch + m_leanPitch);
            AZ::TransformBus::Event(m_visualPartId, &AZ::TransformInterface::SetLocalRotationQuaternion, rotation);
        }

        // Idle blinking keeps the little guy alive between swings.
        if (m_blinkActiveTimer > 0.0f)
        {
            m_blinkActiveTimer -= deltaTime;
            if (m_blinkActiveTimer <= 0.0f)
            {
                for (const AZ::EntityId& eyeId : m_eyePartIds)
                {
                    if (eyeId.IsValid())
                    {
                        AZ::TransformBus::Event(eyeId, &AZ::TransformInterface::SetLocalUniformScale, 1.0f);
                    }
                }
            }
        }
        else
        {
            m_blinkTimer -= deltaTime;
            if (m_blinkTimer <= 0.0f)
            {
                m_blinkActiveTimer = 0.12f;
                m_blinkTimer = 2.5f + m_rng.GetRandomFloat() * 2.0f;
                for (const AZ::EntityId& eyeId : m_eyePartIds)
                {
                    if (eyeId.IsValid())
                    {
                        AZ::TransformBus::Event(eyeId, &AZ::TransformInterface::SetLocalUniformScale, 0.12f);
                    }
                }
            }
        }
    }

    void PlayerControllerComponent::TryFindCameraEntity()
    {
        if (m_cameraEntityId.IsValid())
        {
            return;
        }

        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }

        appRequests->EnumerateEntities([this](AZ::Entity* entity)
        {
            if (entity && (entity->GetName() == "Camera" || entity->GetName() == "NailongCamera"))
            {
                m_cameraEntityId = entity->GetId();
                // Camera entity located; no per-frame log.
                return false;
            }
            return true;
        });
    }

    void PlayerControllerComponent::CacheRockPositions()
    {
        if (m_rocksCached)
        {
            return;
        }
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }
        appRequests->EnumerateEntities([this](AZ::Entity* entity)
        {
            if (entity && entity->GetName() == "Rock")
            {
                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(pos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                m_rockPositions.push_back(pos);
            }
            return true;
        });
        m_rocksCached = !m_rockPositions.empty();
    }

    void PlayerControllerComponent::UpdateCamera(float deltaTime)
    {
        // Camera shake decays even while the game is frozen by hitstop.
        if (m_shakeAmplitude > 0.001f)
        {
            m_shakeAmplitude *= 0.90f;
        }
        else
        {
            m_shakeAmplitude = 0.0f;
        }

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        // Smooth-follow: the camera chases a damped copy of the player position
        // so movement feels weighty instead of rigidly attached.
        if (!m_cameraSmoothedInit)
        {
            m_cameraSmoothedPos = playerPos;
            m_cameraSmoothedInit = true;
        }
        const float followRate = 10.0f;
        const float blend = 1.0f - expf(-followRate * deltaTime);
        m_cameraSmoothedPos += (playerPos - m_cameraSmoothedPos) * blend;

        AZ::Quaternion playerRotation = AZ::Quaternion::CreateIdentity();
        AZ::TransformBus::EventResult(playerRotation, GetEntityId(), &AZ::TransformInterface::GetWorldRotationQuaternion);

        // Third-person offset: behind the player (-Y) and above (+Z).
        const AZ::Vector3 relativeOffset(0.0f, -m_cameraDistance, m_cameraHeight);
        AZ::Vector3 cameraPos = m_cameraSmoothedPos + playerRotation.TransformVector(relativeOffset);
        const AZ::Vector3 lookAtPos = m_cameraSmoothedPos + AZ::Vector3(0.0f, 0.0f, m_cameraLookAtHeight);

        // Keep the camera out of the boulders: if the desired position falls
        // inside a rock's bounding sphere, slide it toward the player instead.
        CacheRockPositions();
        for (const AZ::Vector3& rock : m_rockPositions)
        {
            const float rockRadius = 2.6f;
            const AZ::Vector3 center = rock + AZ::Vector3(0.0f, 0.0f, 1.2f);
            AZ::Vector3 offset = cameraPos - center;
            const float dist = offset.GetLength();
            if (dist < rockRadius && dist > 0.001f)
            {
                cameraPos = center + offset * (rockRadius / dist);
            }
        }

        if (m_shakeAmplitude > 0.0f)
        {
            cameraPos += AZ::Vector3(
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * m_shakeAmplitude,
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * m_shakeAmplitude,
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * m_shakeAmplitude * 0.6f);
        }

        // The camera's view forward axis is +Y in O3DE; CreateLookAt's third
        // argument selects which local basis axis aims at the target.
        const AZ::Transform cameraTransform = AZ::Transform::CreateLookAt(
            cameraPos, lookAtPos, AZ::Transform::Axis::YPositive);

        if (m_cameraEntityId.IsValid())
        {
            AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformInterface::SetWorldTM, cameraTransform);
            // Ensure the runtime camera remains the active view so the viewport context
            // is driven by the Camera component system rather than being manually overridden.
            Camera::CameraRequestBus::Event(m_cameraEntityId, &Camera::CameraRequestBus::Events::MakeActiveView);
        }

        // Also update the viewport context directly as a fallback in case the Camera
        // system has not yet taken ownership of the default viewport.
        if (auto viewportContextManager = AZ::RPI::ViewportContextRequests::Get())
        {
            if (auto viewportContext = viewportContextManager->GetDefaultViewportContext())
            {
                viewportContext->SetCameraTransform(cameraTransform);
            }
        }
    }
} // namespace YellowMythNailong
