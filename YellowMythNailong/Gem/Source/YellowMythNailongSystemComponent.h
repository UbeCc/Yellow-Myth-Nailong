#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <ImGuiBus.h>

#include <YellowMythNailong/YellowMythNailongBus.h>

namespace YellowMythNailong
{
    class YellowMythNailongSystemComponent
        : public AZ::Component
        , protected YellowMythNailongRequestBus::Handler
        , protected AzFramework::GameEntityContextEventBus::Handler
        , protected AzFramework::EntityContextEventBus::Handler
        , public AZ::TickBus::Handler
        , public ImGui::ImGuiUpdateListenerBus::Handler
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

    private:
        void SetupRuntimeCamera();
        bool CanCreateRuntimeCamera() const;
        void DrawHud();

        AZ::EntityId m_runtimeCameraId;
    };
} // namespace YellowMythNailong
