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
        // Nailong dragon to a playable size (~2 m tall from the 385 m source model).
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, 0.005f);

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
        else
        {
            return false;
        }
        return true;
    }

    void PlayerControllerComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        if (m_health <= 0.0f)
        {
            return;
        }

        if (m_attackTimer > 0.0f)
        {
            m_attackTimer -= deltaTime;
        }

        UpdateMovementFromKeyboard();

        if (m_isDodging)
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
        else
        {
            if (m_inputDirection.GetLengthSq() > 0.001f)
            {
                m_inputDirection.Normalize();
                Move(m_inputDirection, deltaTime);
            }

            if (m_attackRequested && m_attackTimer <= 0.0f)
            {
                PerformAttack();
                m_attackTimer = m_attackCooldown;
            }
        }

        m_attackRequested = false;
        m_inputDirection = AZ::Vector3::CreateZero();

        TryFindCameraEntity();
        UpdateCamera();
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
        AZLOG_INFO("Player health: %.1f", m_health);
        if (m_health <= 0.0f)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerDied);
        }
    }

    void PlayerControllerComponent::OnPlayerDied()
    {
        AZLOG_INFO("Player died!");
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

    void PlayerControllerComponent::PerformDodge()
    {
        if (m_isDodging)
        {
            return;
        }
        m_isDodging = true;
        m_dodgeTimer = m_dodgeDuration;
        m_dodgeDirection = m_inputDirection.GetLengthSq() > 0.001f ? m_inputDirection.GetNormalized() : AZ::Vector3::CreateAxisY();
        AZLOG_INFO("Nailong dodges!");
    }

    void PlayerControllerComponent::PerformAttack()
    {
        AZ::Vector3 pos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
        AZLOG_INFO("Nailong attacks!");
        CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerAttack, pos, m_attackRadius, m_attackDamage);
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

    void PlayerControllerComponent::UpdateCamera()
    {
        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        AZ::Quaternion playerRotation = AZ::Quaternion::CreateIdentity();
        AZ::TransformBus::EventResult(playerRotation, GetEntityId(), &AZ::TransformInterface::GetWorldRotationQuaternion);

        // Third-person offset: behind the player (-Y) and above (+Z).
        const AZ::Vector3 relativeOffset(0.0f, -m_cameraDistance, m_cameraHeight);
        const AZ::Vector3 cameraPos = playerPos + playerRotation.TransformVector(relativeOffset);
        const AZ::Vector3 lookAtPos = playerPos + AZ::Vector3(0.0f, 0.0f, m_cameraLookAtHeight);

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
