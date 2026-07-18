#include "YellowMythNailongSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Components/CameraBus.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewGroup.h>
#include <Atom/RPI.Public/ViewProviderBus.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <AtomLyIntegration/CommonFeatures/CoreLights/CoreLightsConstants.h>
#include <AzCore/Component/TransformBus.h>

#include "PlayerControllerComponent.h"

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
        AZ::TickBus::Handler::BusDisconnect();
        AzFramework::EntityContextEventBus::Handler::BusDisconnect();
        AzFramework::GameEntityContextEventBus::Handler::BusDisconnect();
        YellowMythNailongRequestBus::Handler::BusDisconnect();
    }


    void YellowMythNailongSystemComponent::OnEntityContextLoadedFromStream(const AzFramework::EntityList& /*contextEntities*/)
    {
    }

    void YellowMythNailongSystemComponent::OnGameEntitiesStarted()
    {
        if (CanCreateRuntimeCamera())
        {
            SetupRuntimeCamera();
            CreateRuntimeSun();
        }
        else
        {
            // RPI is not ready yet; retry on the next tick.
            AZ::TickBus::Handler::BusConnect();
        }
    }

    void YellowMythNailongSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (CanCreateRuntimeCamera())
        {
            SetupRuntimeCamera();
            CreateRuntimeSun();
            AZ::TickBus::Handler::BusDisconnect();
        }
    }

    bool YellowMythNailongSystemComponent::CanCreateRuntimeCamera() const
    {
        auto* rpiSystem = AZ::RPI::RPISystemInterface::Get();
        return rpiSystem && rpiSystem->IsInitialized();
    }

    void YellowMythNailongSystemComponent::CreateRuntimeSun()
    {
        AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            contextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (contextId.IsNull())
        {
            return;
        }

        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (appRequests)
        {
            bool found = false;
            appRequests->EnumerateEntities([&found](AZ::Entity* entity)
            {
                if (entity && entity->GetName() == "NailongSun")
                {
                    found = true;
                    return false;
                }
                return true;
            });
            if (found)
            {
                return;
            }
        }

        AZ::Entity* sunEntity = nullptr;
        AzFramework::EntityContextRequestBus::EventResult(
            sunEntity, contextId, &AzFramework::EntityContextRequests::CreateEntity, "NailongSun");
        if (!sunEntity)
        {
            AZLOG_WARN("NailongSystem: failed to create runtime sun entity");
            return;
        }

        sunEntity->CreateComponent(AZ::TransformComponentTypeId);
        sunEntity->CreateComponent(AZ::Render::DirectionalLightComponentTypeId);
        sunEntity->Activate();

        AZ::Vector3 sunPos(0.0f, -20.0f, 30.0f);
        AZ::Vector3 lookAt(0.0f, 0.0f, 0.0f);
        AZ::Transform sunTransform = AZ::Transform::CreateLookAt(sunPos, lookAt, AZ::Transform::Axis::ZPositive);
        AZ::TransformBus::Event(sunEntity->GetId(), &AZ::TransformInterface::SetWorldTM, sunTransform);
        AZLOG_INFO("NailongSystem: created runtime sun entity %s", sunEntity->GetId().ToString().c_str());
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

        // Look for an existing runtime camera first (prefab Camera entities are editor-only by default).
        AZ::Entity* existingCamera = nullptr;
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (appRequests)
        {
            appRequests->EnumerateEntities([&existingCamera](AZ::Entity* entity)
            {
                if (entity && entity->GetName() == "NailongCamera")
                {
                    existingCamera = entity;
                    return false;
                }
                return true;
            });
        }

        if (existingCamera)
        {
            m_runtimeCameraId = existingCamera->GetId();
        }
        else
        {
            AZ::Entity* cameraEntity = nullptr;
            AzFramework::EntityContextRequestBus::EventResult(
                cameraEntity, contextId, &AzFramework::EntityContextRequests::CreateEntity, "NailongCamera");
            if (!cameraEntity)
            {
                AZLOG_WARN("NailongSystem: failed to create runtime camera entity");
                return;
            }

            cameraEntity->CreateComponent(AZ::TransformComponentTypeId);
            cameraEntity->CreateComponent(CameraComponentTypeId);
            cameraEntity->Activate();
            m_runtimeCameraId = cameraEntity->GetId();
        }

        // Initial third-person framing; the player controller refines this every tick.
        const AZ::Vector3 cameraPosition(0.0f, -6.0f, 2.5f);
        const AZ::Vector3 lookAtPosition(0.0f, 0.0f, 1.5f);

        AZ::Transform cameraTransform = AZ::Transform::CreateLookAt(
            cameraPosition,
            lookAtPosition,
            AZ::Transform::Axis::YPositive);
        AZ::TransformBus::Event(m_runtimeCameraId, &AZ::TransformInterface::SetWorldTM, cameraTransform);

        Camera::CameraRequestBus::Event(m_runtimeCameraId, &Camera::CameraRequestBus::Events::MakeActiveView);

        // Some standalone launcher viewport contexts are not registered under the
        // default viewport context name, so MakeActiveView alone is not always
        // enough. Push the runtime camera view onto every registered viewport context.
        AZ::RPI::ViewPtr cameraView;
        AZ::RPI::ViewProviderBus::EventResult(
            cameraView, m_runtimeCameraId, &AZ::RPI::ViewProviderBus::Events::GetView);
        if (cameraView)
        {
            auto cameraViewGroup = AZStd::make_shared<AZ::RPI::ViewGroup>();
            cameraViewGroup->Init(AZ::RPI::ViewGroup::Descriptor{});
            cameraViewGroup->SetView(cameraView, AZ::RPI::ViewType::Default);

            if (auto* viewportRequests = AZ::RPI::ViewportContextRequests::Get())
            {
                viewportRequests->EnumerateViewportContexts(
                    [&cameraViewGroup](AZ::RPI::ViewportContextPtr viewportContext)
                    {
                        if (viewportContext)
                        {
                            AZ::RPI::ViewportContextRequests::Get()->PushViewGroup(
                                viewportContext->GetName(), cameraViewGroup);
                        }
                    });
            }
        }

        bool isActive = false;
        Camera::CameraRequestBus::EventResult(isActive, m_runtimeCameraId, &Camera::CameraRequestBus::Events::IsActiveView);
        AZLOG_INFO("NailongSystem: runtime camera %s active=%d", m_runtimeCameraId.ToString().c_str(), isActive);

        // Hand the camera off to the player controller so it follows the character.
        if (appRequests)
        {
            appRequests->EnumerateEntities([this](AZ::Entity* entity)
            {
                if (entity && entity->GetName() == "NailongPlayer")
                {
                    PlayerControllerComponent* controller = entity->FindComponent<PlayerControllerComponent>();
                    if (controller)
                    {
                        controller->SetCameraEntityId(m_runtimeCameraId);
                        AZLOG_INFO("NailongSystem: assigned runtime camera to player controller");
                    }
                    return false;
                }
                return true;
            });
        }
    }

} // namespace YellowMythNailong
