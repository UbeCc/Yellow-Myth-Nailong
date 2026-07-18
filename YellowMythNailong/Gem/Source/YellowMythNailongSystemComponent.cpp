#include "YellowMythNailongSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Component/ComponentApplicationBus.h>

#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzCore/Component/TransformBus.h>

#include <Atom/Component/DebugCamera/CameraComponent.h>

namespace YellowMythNailong
{
    AZ_COMPONENT_IMPL(YellowMythNailongSystemComponent, "YellowMythNailongSystemComponent",
        YellowMythNailongSystemComponentTypeId);

    void YellowMythNailongSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<YellowMythNailongSystemComponent, AZ::Component>()
                ->Version(1)
                ;
        }
    }

    void YellowMythNailongSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("YellowMythNailongService"));
    }

    void YellowMythNailongSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("YellowMythNailongService"));
    }

    void YellowMythNailongSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void YellowMythNailongSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    YellowMythNailongSystemComponent::YellowMythNailongSystemComponent()
    {
        if (YellowMythNailongInterface::Get() == nullptr)
        {
            YellowMythNailongInterface::Register(this);
        }
    }

    YellowMythNailongSystemComponent::~YellowMythNailongSystemComponent()
    {
        if (YellowMythNailongInterface::Get() == this)
        {
            YellowMythNailongInterface::Unregister(this);
        }
    }

    void YellowMythNailongSystemComponent::Init()
    {
    }

    void YellowMythNailongSystemComponent::Activate()
    {
        YellowMythNailongRequestBus::Handler::BusConnect();
        AzFramework::GameEntityContextEventBus::Handler::BusConnect();

        AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(contextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (!contextId.IsNull())
        {
            AzFramework::EntityContextEventBus::Handler::BusConnect(contextId);
        }
    }

    void YellowMythNailongSystemComponent::Deactivate()
    {
        AzFramework::EntityContextEventBus::Handler::BusDisconnect();
        AzFramework::GameEntityContextEventBus::Handler::BusDisconnect();
        YellowMythNailongRequestBus::Handler::BusDisconnect();
    }

    void YellowMythNailongSystemComponent::OnEntityContextLoadedFromStream(const AzFramework::EntityList& contextEntities)
    {
        AZLOG_INFO("NailongSystem: OnEntityContextLoadedFromStream with %zu entities", contextEntities.size());
        for (AZ::Entity* entity : contextEntities)
        {
            if (!entity)
            {
                continue;
            }
            const AZStd::string name = entity->GetName();
            AZLOG_INFO("NailongSystem: loaded entity '%s' id=%s", name.c_str(), entity->GetId().ToString().c_str());
        }
    }

    void YellowMythNailongSystemComponent::OnGameEntitiesStarted()
    {
        AZLOG_INFO("NailongSystem: OnGameEntitiesStarted");
        SetupRuntimeCamera();
    }

    void YellowMythNailongSystemComponent::SetupRuntimeCamera()
    {
        if (m_runtimeCameraId.IsValid())
        {
            return;
        }

        AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            contextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (contextId.IsNull())
        {
            AZLOG_WARN("NailongSystem: failed to get game entity context id");
            return;
        }

        AZ::Entity* cameraEntity = nullptr;
        AzFramework::EntityContextRequestBus::EventResult(
            cameraEntity, contextId, &AzFramework::EntityContextRequests::CreateEntity, "NailongCamera");
        if (!cameraEntity)
        {
            AZLOG_WARN("NailongSystem: failed to create runtime camera entity");
            return;
        }

        cameraEntity->CreateComponent(AZ::TransformComponentTypeId);
        cameraEntity->CreateComponent<AZ::Debug::CameraComponent>();
        cameraEntity->Activate();
        m_runtimeCameraId = cameraEntity->GetId();
        Camera::CameraRequestBus::Event(m_runtimeCameraId, &Camera::CameraRequestBus::Events::MakeActiveView);
        AZLOG_INFO("NailongSystem: created runtime camera entity %s", m_runtimeCameraId.ToString().c_str());
    }

} // namespace YellowMythNailong
