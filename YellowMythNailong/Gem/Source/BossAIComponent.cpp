#include "BossAIComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/ILogger.h>

namespace YellowMythNailong
{
    BossAIComponent::BossAIComponent()
    {
    }

    void BossAIComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BossAIComponent, AZ::Component>()
                ->Version(1)
                ->Field("MoveSpeed", &BossAIComponent::m_moveSpeed)
                ->Field("AttackRange", &BossAIComponent::m_attackRange)
                ->Field("AttackCooldown", &BossAIComponent::m_attackCooldown)
                ->Field("AttackDamage", &BossAIComponent::m_attackDamage)
                ->Field("AttackRadius", &BossAIComponent::m_attackRadius)
                ->Field("MaxHealth", &BossAIComponent::m_maxHealth)
                ->Field("StaggerDuration", &BossAIComponent::m_staggerDuration)
                ->Field("AgroRange", &BossAIComponent::m_agroRange)
                ->Field("PlayerEntityId", &BossAIComponent::m_playerEntityId)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<BossAIComponent>("Boss AI", "Simple boss AI for the dark lord")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Yellow Myth")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_moveSpeed, "Move Speed", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_attackRange, "Attack Range", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_attackCooldown, "Attack Cooldown", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_attackDamage, "Attack Damage", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_attackRadius, "Attack Radius", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_maxHealth, "Max Health", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_staggerDuration, "Stagger Duration", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_agroRange, "Agro Range", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BossAIComponent::m_playerEntityId, "Player Entity", "")
                    ;
            }
        }
    }

    void BossAIComponent::Init()
    {
    }

    void BossAIComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();
        CombatNotificationBus::Handler::BusConnect();
        m_health = m_maxHealth;
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, 0.02f);

        TryFindPlayerEntity();
    }

    void BossAIComponent::Deactivate()
    {
        CombatNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    void BossAIComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        if (m_health <= 0.0f)
        {
            return;
        }

        if (m_attackTimer > 0.0f)
        {
            m_attackTimer -= deltaTime;
        }

        UpdateAI(deltaTime);
    }

    void BossAIComponent::TryFindPlayerEntity()
    {
        if (m_playerEntityId.IsValid())
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
            if (entity && entity->GetName() == "NailongPlayer")
            {
                m_playerEntityId = entity->GetId();
                AZLOG_INFO("BossAIComponent: found NailongPlayer entity %s", m_playerEntityId.ToString().c_str());
                return false;
            }
            return true;
        });
    }

    void BossAIComponent::UpdateAI(float deltaTime)
    {
        if (!m_playerEntityId.IsValid())
        {
            TryFindPlayerEntity();
            return;
        }

        if (m_state == State::Stagger)
        {
            m_staggerTimer -= deltaTime;
            if (m_staggerTimer <= 0.0f)
            {
                m_state = State::Chase;
            }
            return;
        }

        AZ::Vector3 myPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(myPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, m_playerEntityId, &AZ::TransformInterface::GetWorldTranslation);

        AZ::Vector3 toPlayer = playerPos - myPos;
        toPlayer.SetZ(0.0f);
        float distance = toPlayer.GetLength();

        if (distance > m_agroRange)
        {
            m_state = State::Idle;
            return;
        }

        m_state = State::Chase;

        if (toPlayer.GetLengthSq() > 0.001f)
        {
            toPlayer.Normalize();
            AZ::Quaternion targetRotation = AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisY(), toPlayer);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldRotationQuaternion, targetRotation);
        }

        if (distance <= m_attackRange && m_attackTimer <= 0.0f)
        {
            m_state = State::Attack;
            PerformAttack();
            m_attackTimer = m_attackCooldown;
            return;
        }

        if (m_state == State::Chase)
        {
            AZ::Vector3 newPos = myPos + toPlayer * m_moveSpeed * deltaTime;
            newPos.SetZ(0.75f); // boss sits slightly higher
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, newPos);
        }
    }

    void BossAIComponent::PerformAttack()
    {
        AZ::Vector3 pos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
        AZLOG_INFO("Boss attacks!");
        CombatNotificationBus::Broadcast(&CombatNotifications::OnBossAttack, pos, m_attackRadius, m_attackDamage);
    }

    void BossAIComponent::OnPlayerAttack(const AZ::Vector3& position, float radius, float damage)
    {
        AZ::Vector3 myPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(myPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        float distSq = (myPos - position).GetLengthSq();
        if (distSq <= radius * radius)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossDamaged, damage);
        }
    }

    void BossAIComponent::OnBossDamaged(float damage)
    {
        m_health -= damage;
        AZLOG_INFO("Boss health: %.1f", m_health);
        m_state = State::Stagger;
        m_staggerTimer = m_staggerDuration;
        if (m_health <= 0.0f)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossDied);
        }
    }

    void BossAIComponent::OnBossDied()
    {
        AZLOG_INFO("Boss defeated!");
    }
} // namespace YellowMythNailong
