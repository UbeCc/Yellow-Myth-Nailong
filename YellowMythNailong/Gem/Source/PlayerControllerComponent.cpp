#include "PlayerControllerComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/ILogger.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialAssignmentId.h>


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
                ->Version(2)
                ->Field("MoveSpeed", &PlayerControllerComponent::m_moveSpeed)
                ->Field("DodgeSpeed", &PlayerControllerComponent::m_dodgeSpeed)
                ->Field("DodgeDuration", &PlayerControllerComponent::m_dodgeDuration)
                ->Field("AttackCooldown", &PlayerControllerComponent::m_attackCooldown)
                ->Field("AttackRadius", &PlayerControllerComponent::m_attackRadius)
                ->Field("AttackDamage", &PlayerControllerComponent::m_attackDamage)
                ->Field("MaxHealth", &PlayerControllerComponent::m_maxHealth)
                ->Field("CameraEntityId", &PlayerControllerComponent::m_cameraEntityId)
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
        else
        {
            return false;
        }
        return true;
    }

    void PlayerControllerComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        if (!m_loggedMaterialSlot)
        {
            m_loggedMaterialSlot = true;
            AZ::Render::MaterialAssignmentMap materialMap;
            AZ::Render::MaterialComponentRequestBus::EventResult(materialMap, GetEntityId(), &AZ::Render::MaterialComponentRequestBus::Events::GetMaterialMapCopy);
            AZ_Printf("PlayerControllerComponent", "Nailong material map size: %zu", materialMap.size());
            for (const auto& pair : materialMap)
            {
                AZ_Printf("PlayerControllerComponent", "Nailong material slot stable id: %u lod: %u", pair.first.m_materialSlotStableId, pair.first.m_lodIndex);
            }
            AZ::Render::MaterialAssignmentMap defaultMap;
            AZ::Render::MaterialComponentRequestBus::EventResult(defaultMap, GetEntityId(), &AZ::Render::MaterialComponentRequestBus::Events::GetDefaultMaterialMap);
            AZ_Printf("PlayerControllerComponent", "Nailong default map size: %zu", defaultMap.size());
            for (const auto& pair : defaultMap)
            {
                AZ_Printf("PlayerControllerComponent", "Nailong default slot stable id: %u lod: %u", pair.first.m_materialSlotStableId, pair.first.m_lodIndex);
            }
        }

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

    void PlayerControllerComponent::Move(const AZ::Vector3& direction, float deltaTime)
    {
        AZ::Vector3 currentTranslation = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(currentTranslation, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        float speed = m_isDodging ? m_dodgeSpeed : m_moveSpeed;
        AZ::Vector3 newPos = currentTranslation + direction * speed * deltaTime;
        newPos.SetZ(0.5f); // keep on ground

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
            if (entity && entity->GetName() == "NailongCamera")
            {
                m_cameraEntityId = entity->GetId();
                AZLOG_INFO("PlayerControllerComponent: found NailongCamera entity %s", m_cameraEntityId.ToString().c_str());
                return false; // stop enumeration
            }
            return true;
        });
    }

    void PlayerControllerComponent::UpdateCamera()
    {
        if (!m_cameraEntityId.IsValid())
        {
            return;
        }

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        AZ::Vector3 cameraOffset(-4.0f, -8.0f, 4.0f);
        AZ::Vector3 targetPos = playerPos + cameraOffset;
        AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformInterface::SetWorldTranslation, targetPos);

        AZ::Vector3 lookAt = playerPos;
        lookAt.SetZ(1.0f);
        AZ::Vector3 forward = (lookAt - targetPos).GetNormalized();
        float yaw = atan2(forward.GetX(), forward.GetY());
        AZ::Quaternion rot = AZ::Quaternion::CreateRotationZ(yaw);
        AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformInterface::SetWorldRotationQuaternion, rot);
    }
} // namespace YellowMythNailong
