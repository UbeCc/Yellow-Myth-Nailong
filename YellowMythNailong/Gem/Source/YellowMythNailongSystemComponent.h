#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <ImGuiBus.h>

#include <YellowMythNailong/YellowMythNailongBus.h>

#include "CombatNotificationBus.h"

namespace YellowMythNailong
{
    class YellowMythNailongSystemComponent
        : public AZ::Component
        , protected YellowMythNailongRequestBus::Handler
        , protected AzFramework::GameEntityContextEventBus::Handler
        , protected AzFramework::EntityContextEventBus::Handler
        , public AZ::TickBus::Handler
        , public ImGui::ImGuiUpdateListenerBus::Handler
        , public CombatNotificationBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(YellowMythNailongSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        YellowMythNailongSystemComponent();
        ~YellowMythNailongSystemComponent();

    protected:
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // AzFramework::GameEntityContextEventBus
        void OnGameEntitiesStarted() override;

        // AzFramework::EntityContextEventBus
        void OnEntityContextLoadedFromStream(const AzFramework::EntityList& contextEntities) override;

        // AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // ImGui::IImGuiUpdateListener
        void OnImGuiUpdate() override;

        // CombatNotificationBus
        void OnBossDamaged(float damage) override;
        void OnPlayerDamaged(float damage) override;
        void OnBossEngaged() override;
        void OnBossEnraged() override;
        void OnPlayerComboStrike() override;
        void OnPlayerDodged() override;
        void OnPlayerAttack(const AZ::Vector3& position, float radius, float damage) override;
        void OnBossAttack(const AZ::Vector3& position, float radius, float damage) override;
        void OnProjectileTick(const AZ::Vector3& position, bool hostile) override;

    private:
        struct FloatingText
        {
            AZ::Vector3 m_worldPos;
            AZStd::string m_text;
            float m_r = 1.0f;
            float m_g = 1.0f;
            float m_b = 1.0f;
            float m_scale = 1.0f;
            float m_age = 0.0f;
            float m_lifetime = 1.0f;
        };

        // Lightweight world-space visual effects, drawn through ImGui.
        struct Vfx
        {
            enum Type
            {
                SlashArc,   // melee swing arc in front of the attacker
                SparkRay,   // one ray of an impact burst
                DustPuff,   // small expanding ground ring
                ShockRing,  // big expanding ground ring for the combo finisher
                SpeedStreak // motion line trailing a dodge / charge
            };

            Type m_type = SlashArc;
            AZ::Vector3 m_pos = AZ::Vector3::CreateZero();
            AZ::Vector3 m_dir = AZ::Vector3::CreateAxisY();
            float m_r = 1.0f;
            float m_g = 1.0f;
            float m_b = 1.0f;
            float m_size = 1.0f;
            float m_age = 0.0f;
            float m_lifetime = 0.2f;
        };

        void SetupRuntimeCamera();
        bool CanCreateRuntimeCamera() const;
        bool SetupNailongModel();
        bool ApplyArenaMaterials();
        void DrawHud();
        void DrawTitleScreen(float screenW, float screenH);
        void DrawFloatingTexts(float screenW, float screenH);
        void DrawBossBanner(float screenW, float screenH);
        void DrawVignette(float screenW, float screenH);
        void DrawTelegraphRing(float screenW, float screenH);
        void DrawVfx(float screenW, float screenH);
        void SpawnFloatingText(const AZ::Vector3& worldPos, const char* text, float r, float g, float b, float scale);
        void SpawnVfx(Vfx::Type type, const AZ::Vector3& pos, const AZ::Vector3& dir, float r, float g, float b, float size, float lifetime);
        void SpawnSlashArc(const AZ::Vector3& attackerPos, const AZ::Vector3& facing, bool finisher, bool hostile);
        void SpawnImpactBurst(const AZ::Vector3& pos, float r, float g, float b);
        void SpawnDustBurst(const AZ::Vector3& pos, int count);
        bool WorldToScreen(const AZ::Vector3& worldPos, float screenW, float screenH, float& outX, float& outY, float& outDepth) const;

        AZ::EntityId m_runtimeCameraId;
        bool m_nailongModelSet = false;
        bool m_materialsApplied = false;

        AZStd::vector<FloatingText> m_floatingTexts;
        AZStd::vector<Vfx> m_vfx;
        float m_hurtFlash = 0.0f;
        float m_bannerTimer = 0.0f;
        int m_bannerKind = 0; // 0 none, 1 boss engaged, 2 boss enraged
        float m_comboPopupTimer = 0.0f;

        // Footstep dust emission state.
        AZ::Vector3 m_lastDustPos = AZ::Vector3::CreateZero();
        AZ::Vector3 m_lastBossPos = AZ::Vector3::CreateZero();
        float m_dustTimer = 0.0f;
        float m_chargeStreakTimer = 0.0f;
    };
} // namespace YellowMythNailong
