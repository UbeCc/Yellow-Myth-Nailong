#include "BossAIComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Math/Random.h>

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
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale);

        TryFindPlayerEntity();
    }

    void BossAIComponent::Deactivate()
    {
        CombatNotificationBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
    }

    void BossAIComponent::OnTick(float deltaTime, AZ::ScriptTimePoint /*time*/)
    {
        // Death animation: tip over and sink into the ground, then hold.
        if (m_health <= 0.0f)
        {
            if (m_deathTimer > 0.0f)
            {
                m_deathTimer -= deltaTime;
                const float t = AZ::GetClamp(1.0f - m_deathTimer / 2.0f, 0.0f, 1.0f);

                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
                pos.SetZ(0.75f - 3.0f * t);
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, pos);

                AZ::Quaternion roll = AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisX(), t * 1.4f);
                AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldRotationQuaternion, roll);

                AZ::TransformBus::Event(
                    GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale * (1.0f - 0.6f * t));
            }
            return;
        }

        m_animTime += deltaTime;

        if (m_attackTimer > 0.0f)
        {
            m_attackTimer -= deltaTime;
        }
        if (m_chargeTimer > 0.0f)
        {
            m_chargeTimer -= deltaTime;
        }
        if (m_fireballCooldown > 0.0f)
        {
            m_fireballCooldown -= deltaTime;
        }

        UpdateAI(deltaTime);
        UpdateFireball(deltaTime);
        UpdateVisuals(deltaTime);
    }

    void BossAIComponent::UpdateFireball(float deltaTime)
    {
        if (!m_fireballActive)
        {
            return;
        }
        if (!m_playerEntityId.IsValid())
        {
            m_fireballActive = false;
            return;
        }

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, m_playerEntityId, &AZ::TransformInterface::GetWorldTranslation);

        m_fireballTimer -= deltaTime;

        // Mild homing: enough to threaten, slow enough to dodge.
        AZ::Vector3 toPlayer = playerPos - m_fireballPosition;
        toPlayer.SetZ(0.0f);
        if (toPlayer.GetLengthSq() > 0.001f)
        {
            toPlayer.Normalize();
            m_fireballDirection = (m_fireballDirection + toPlayer * 1.6f * deltaTime).GetNormalized();
        }

        m_fireballPosition += m_fireballDirection * 15.0f * deltaTime;
        CombatNotificationBus::Broadcast(&CombatNotifications::OnProjectileTick, m_fireballPosition, true);

        AZ::Vector3 delta = playerPos - m_fireballPosition;
        delta.SetZ(0.0f);
        if (delta.GetLengthSq() <= 2.2f * 2.2f)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossAttack, m_fireballPosition, 2.5f, m_fireballDamage);
            m_fireballActive = false;
            return;
        }
        if (m_fireballTimer <= 0.0f)
        {
            m_fireballActive = false;
        }
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

        // The boss loses interest once its foe is down.
        if (m_playerDead)
        {
            m_state = State::Idle;
            return;
        }

        AZ::Vector3 myPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(myPos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);

        AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(playerPos, m_playerEntityId, &AZ::TransformInterface::GetWorldTranslation);

        if (m_state == State::Stagger)
        {
            // Flinch jitter while staggered.
            AZ::Vector3 jitter(
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * 0.06f,
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * 0.06f, 0.0f);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, myPos + jitter);

            m_staggerTimer -= deltaTime;
            if (m_staggerTimer <= 0.0f)
            {
                m_state = State::Chase;
            }
            return;
        }

        if (m_state == State::Telegraph)
        {
            // Shake in place to warn of the incoming charge.
            AZ::Vector3 jitter(
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * 0.1f,
                (m_rng.GetRandomFloat() * 2.0f - 1.0f) * 0.1f, 0.0f);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, myPos + jitter);

            m_telegraphTimer -= deltaTime;
            if (m_telegraphTimer <= 0.0f)
            {
                m_state = State::Charge;
            }
            return;
        }

        if (m_state == State::Charge)
        {
            AZ::Vector3 toTarget = m_chargeTarget - myPos;
            toTarget.SetZ(0.0f);
            const float distance = toTarget.GetLength();
            if (distance < 0.6f)
            {
                // Slam down at the target point.
                CombatNotificationBus::Broadcast(&CombatNotifications::OnBossAttack, myPos, m_chargeRadius, m_chargeDamage);
                m_state = State::Chase;
                m_chargeTimer = m_chargeCooldown;
                return;
            }
            toTarget.Normalize();
            AZ::Vector3 newPos = myPos + toTarget * m_chargeSpeed * deltaTime;
            newPos.SetZ(0.75f);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, newPos);
            AZ::Quaternion targetRotation = AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisY(), toTarget);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldRotationQuaternion, targetRotation);
            return;
        }

        AZ::Vector3 toPlayer = playerPos - myPos;
        toPlayer.SetZ(0.0f);
        float distance = toPlayer.GetLength();

        if (distance > m_agroRange)
        {
            m_state = State::Idle;
            return;
        }

        if (!m_engaged)
        {
            m_engaged = true;
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossEngaged);
        }

        m_state = State::Chase;

        if (toPlayer.GetLengthSq() > 0.001f)
        {
            toPlayer.Normalize();
            AZ::Quaternion targetRotation = AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisY(), toPlayer);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldRotationQuaternion, targetRotation);
        }

        // Phase two: periodically telegraph a charge when not already toe to toe.
        if (m_enraged && m_chargeTimer <= 0.0f && distance > 6.0f)
        {
            m_state = State::Telegraph;
            m_telegraphTimer = m_telegraphDuration;
            m_chargeTarget = playerPos;
            return;
        }

        // Phase two: lob a dark fireball when the player keeps their distance.
        if (m_enraged && m_fireballCooldown <= 0.0f && !m_fireballActive && distance > 5.0f)
        {
            m_fireballActive = true;
            m_fireballTimer = 2.0f;
            m_fireballPosition = myPos + AZ::Vector3(0.0f, 0.0f, 1.5f);
            m_fireballDirection = toPlayer;
            m_fireballCooldown = m_fireballCooldownMax;
            AZLOG_INFO("Boss hurls a dark fireball!");
        }

        if (distance <= m_attackRange && m_attackTimer <= 0.0f)
        {
            m_state = State::Attack;
            m_lungeTimer = 0.15f;
            m_lungeDirection = toPlayer;
            PerformAttack();
            m_attackTimer = m_attackCooldown;
            return;
        }

        if (m_state == State::Chase)
        {
            const float speed = m_enraged ? m_enragedMoveSpeed : m_moveSpeed;
            AZ::Vector3 newPos = myPos + toPlayer * speed * deltaTime;
            newPos.SetZ(0.75f);
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, newPos);
        }
    }

    void BossAIComponent::UpdateVisuals(float deltaTime)
    {
        if (m_pulseTimer > 0.0f)
        {
            m_pulseTimer -= deltaTime;
        }
        // Hit pulse: briefly swell when damaged.
        const float pulse = m_pulseTimer > 0.0f ? 0.2f * (m_pulseTimer / 0.15f) : 0.0f;
        const float enrageBulk = m_enraged ? 1.15f : 1.0f;
        AZ::TransformBus::Event(
            GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale * enrageBulk * (1.0f + pulse));

        // Attack lunge: a short forward dash adds weight to the swing.
        if (m_lungeTimer > 0.0f)
        {
            m_lungeTimer -= deltaTime;
            AZ::Vector3 pos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
            AZ::Vector3 newPos = pos + m_lungeDirection * 8.0f * deltaTime;
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, newPos);
        }

        // Hover bob while alive and not charging.
        if (m_state != State::Charge)
        {
            AZ::Vector3 pos = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
            pos.SetZ(0.75f + 0.15f * sinf(m_animTime * 1.8f));
            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, pos);
        }
    }

    void BossAIComponent::PerformAttack()
    {
        AZ::Vector3 pos = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(pos, GetEntityId(), &AZ::TransformInterface::GetWorldTranslation);
        AZLOG_INFO("Boss attacks!");
        const float damage = m_enraged ? m_enragedAttackDamage : m_attackDamage;
        CombatNotificationBus::Broadcast(&CombatNotifications::OnBossAttack, pos, m_attackRadius, damage);
    }

    void BossAIComponent::OnBossDamaged(float damage)
    {
        if (m_health <= 0.0f)
        {
            return;
        }

        m_health -= damage;
        AZLOG_INFO("Boss health: %.1f", m_health);
        m_pulseTimer = 0.15f;
        m_state = State::Stagger;
        // Heavy focus blows floor the boss; pokes only make it flinch.
        m_staggerTimer = damage >= 100.0f ? 1.5f : m_staggerDuration;

        if (!m_enraged && m_health <= m_maxHealth * 0.5f && m_health > 0.0f)
        {
            m_enraged = true;
            AZLOG_INFO("Boss enraged!");
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossEnraged);
        }

        if (m_health <= 0.0f)
        {
            CombatNotificationBus::Broadcast(&CombatNotifications::OnBossDied);
        }
    }

    void BossAIComponent::OnBossDied()
    {
        AZLOG_INFO("Boss defeated!");
        m_deathTimer = 2.0f;
    }

    void BossAIComponent::OnPlayerDied()
    {
        m_playerDead = true;
    }

    void BossAIComponent::OnRestartGame()
    {
        // Reset the boss to full health back at its spawn point, waiting to be engaged.
        m_health = m_maxHealth;
        m_state = State::Idle;
        m_attackTimer = 0.0f;
        m_staggerTimer = 0.0f;
        m_animTime = 0.0f;
        m_pulseTimer = 0.0f;
        m_lungeTimer = 0.0f;
        m_deathTimer = 0.0f;
        m_engaged = false;
        m_enraged = false;
        m_playerDead = false;
        m_chargeTimer = 0.0f;
        m_telegraphTimer = 0.0f;
        m_fireballActive = false;
        m_fireballCooldown = 0.0f;
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetLocalUniformScale, m_baseScale);
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldTranslation, AZ::Vector3(20.0f, 20.0f, 0.75f));
        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformInterface::SetWorldRotationQuaternion, AZ::Quaternion::CreateIdentity());
    }
} // namespace YellowMythNailong
