#include <AzCore/Math/Vector3.h>
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>

namespace YellowMythNailong
{
    class CombatNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

        virtual ~CombatNotifications() = default;

        virtual void OnPlayerAttack(const AZ::Vector3& position, float radius, float damage) { AZ_UNUSED(position); AZ_UNUSED(radius); AZ_UNUSED(damage); }
        virtual void OnBossAttack(const AZ::Vector3& position, float radius, float damage) { AZ_UNUSED(position); AZ_UNUSED(radius); AZ_UNUSED(damage); }
        virtual void OnPlayerDamaged(float damage) { AZ_UNUSED(damage); }
        virtual void OnBossDamaged(float damage) { AZ_UNUSED(damage); }
        virtual void OnPlayerDied() {}
        virtual void OnBossDied() {}
        // Broadcast when the player asks to restart after game over / victory.
        virtual void OnRestartGame() {}
        // Broadcast once when the player first moves/acts (dismisses the title screen).
        virtual void OnGameStart() {}
        // Broadcast once when the boss first aggros onto the player.
        virtual void OnBossEngaged() {}
        // Broadcast once when the boss drops below half health and enters phase two.
        virtual void OnBossEnraged() {}
        // Broadcast when the player lands the third hit of the melee combo.
        virtual void OnPlayerComboStrike() {}
        // Broadcast when the player starts a dodge roll.
        virtual void OnPlayerDodged() {}
        // Broadcast every tick while a projectile flies (spit / fireball trail VFX).
        virtual void OnProjectileTick(const AZ::Vector3& position, bool hostile) { AZ_UNUSED(position); AZ_UNUSED(hostile); }
    };
    using CombatNotificationBus = AZ::EBus<CombatNotifications>;
} // namespace YellowMythNailong
