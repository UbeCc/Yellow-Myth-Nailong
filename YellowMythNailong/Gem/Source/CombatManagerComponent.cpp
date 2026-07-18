#include "CombatManagerComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/ILogger.h>

namespace YellowMythNailong
{
    CombatManagerComponent::CombatManagerComponent()
    {
    }

    void CombatManagerComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CombatManagerComponent, AZ::Component>()
                ->Version(1)
                ->Field("PlayerEntityId", &CombatManagerComponent::m_playerEntityId)
                ->Field("BossEntityId", &CombatManagerComponent::m_bossEntityId)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CombatManagerComponent>("Combat Manager", "Manages combat state between player and boss")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Yellow Myth")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CombatManagerComponent::m_playerEntityId, "Player Entity", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CombatManagerComponent::m_bossEntityId, "Boss Entity", "")
                    ;
            }
        }
    }

    void CombatManagerComponent::Init()
    {
    }

    void CombatManagerComponent::Activate()
    {
        CombatNotificationBus::Handler::BusConnect();
        TryFindEntities();
    }

    void CombatManagerComponent::Deactivate()
    {
        CombatNotificationBus::Handler::BusDisconnect();
    }

    void CombatManagerComponent::TryFindEntities()
    {
        if (m_playerEntityId.IsValid() && m_bossEntityId.IsValid())
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
            const AZStd::string name = entity->GetName();
            if (name == "NailongPlayer" && !m_playerEntityId.IsValid())
            {
                m_playerEntityId = entity->GetId();
                AZLOG_INFO("CombatManagerComponent: found NailongPlayer entity %s", m_playerEntityId.ToString().c_str());
            }
            else if (name == "DarkBoss" && !m_bossEntityId.IsValid())
            {
                m_bossEntityId = entity->GetId();
                AZLOG_INFO("CombatManagerComponent: found DarkBoss entity %s", m_bossEntityId.ToString().c_str());
            }
            return true;
        });
    }

    void CombatManagerComponent::OnPlayerAttack(const AZ::Vector3& position, float radius, float damage)
    {
        if (m_gameOver)
        {
            return;
        }

        if (!m_bossEntityId.IsValid())
        {
            TryFindEntities();
            return;
        }

        AZ::Vector3 bossPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(bossPos, m_bossEntityId, &AZ::TransformInterface::GetWorldTranslation);

        float distSq = (bossPos - position).GetLengthSq();
        if (distSq <= radius * radius)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossDamaged, damage);
        }
    }

    void CombatManagerComponent::OnBossAttack(const AZ::Vector3& position, float radius, float damage)
    {
        if (m_gameOver)
        {
            return;
        }

        if (!m_playerEntityId.IsValid())
        {
            TryFindEntities();
            return;
        }

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, m_playerEntityId, &AZ::TransformInterface::GetWorldTranslation);

        float distSq = (playerPos - position).GetLengthSq();
        if (distSq <= radius * radius)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnPlayerDamaged, damage);
        }
    }

    void CombatManagerComponent::OnPlayerDied()
    {
        if (m_gameOver)
        {
            return;
        }
        m_gameOver = true;
        AZLOG_ERROR("GAME OVER - The Yellow Myth has fallen...");
    }

    void CombatManagerComponent::OnBossDied()
    {
        if (m_gameOver)
        {
            return;
        }
        m_gameOver = true;
        m_victory = true;
        AZLOG_INFO("VICTORY - The Dark Lord is defeated! Nailong reigns supreme!");
    }

    void CombatManagerComponent::OnRestartGame()
    {
        m_gameOver = false;
        m_victory = false;
        AZLOG_INFO("Restarting the Yellow Myth...");
    }
} // namespace YellowMythNailong
