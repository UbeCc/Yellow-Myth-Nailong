#include "YellowMythNailongSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/string/string.h>

#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Components/CameraBus.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewGroup.h>
#include <Atom/RPI.Public/ViewProviderBus.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentBus.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/TransformBus.h>

#include "PlayerControllerComponent.h"
#include "BossAIComponent.h"
#include "CombatManagerComponent.h"

#include <imgui/imgui.h>

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
        ImGui::ImGuiUpdateListenerBus::Handler::BusConnect();

        AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(contextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (!contextId.IsNull())
        {
            AzFramework::EntityContextEventBus::Handler::BusConnect(contextId);
        }
    }

    void YellowMythNailongSystemComponent::Deactivate()
    {
        ImGui::ImGuiUpdateListenerBus::Handler::BusDisconnect();
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
        // Show the HUD while keeping keyboard/mouse input flowing to the game.
        // Set this here (rather than in Activate) so the ImGui manager is guaranteed
        // to be connected to ImGuiManagerBus already.
        ImGui::ImGuiManagerBus::Broadcast(&ImGui::IImGuiManager::SetDisplayState, ImGui::DisplayState::Visible);
        ImGui::ImGuiManagerBus::Broadcast(&ImGui::IImGuiManager::SetEnableDiscreteInputMode, false);

        if (CanCreateRuntimeCamera())
        {
            SetupRuntimeCamera();
        }

        // The level entities finish spawning after this callback, so keep ticking to
        // swap the player's model to the Nailong once it exists.
        AZ::TickBus::Handler::BusConnect();
    }

    void YellowMythNailongSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_runtimeCameraId.IsValid() && CanCreateRuntimeCamera())
        {
            SetupRuntimeCamera();
        }

        if (!m_nailongModelSet)
        {
            m_nailongModelSet = SetupNailongModel();
        }

        if (m_runtimeCameraId.IsValid() && m_nailongModelSet)
        {
            AZ::TickBus::Handler::BusDisconnect();
        }
    }

    namespace
    {
        void OverrideMaterialByProductPath(AZ::EntityId entityId, const char* productPath)
        {
            AZ::Data::AssetId materialAssetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                materialAssetId,
                &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                productPath,
                azrtti_typeid<AZ::RPI::MaterialAsset>(),
                false);
            if (materialAssetId.IsValid())
            {
                AZ::Render::MaterialComponentRequestBus::Event(
                    entityId,
                    &AZ::Render::MaterialComponentRequestBus::Events::SetMaterialAssetIdOnDefaultSlot,
                    materialAssetId);
            }
        }
    }

    bool YellowMythNailongSystemComponent::SetupNailongModel()
    {
        auto nailongAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::ModelAsset>(
            "quaternius/monsterpack/obj/nailong.glb.azmodel");
        if (!nailongAsset.IsReady())
        {
            return false;
        }



        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return false;
        }

        bool done = false;
        appRequests->EnumerateEntities([&done, &nailongAsset](AZ::Entity* entity)
        {
            if (entity && entity->GetName() == "NailongPlayer")
            {
                AZ::Render::MeshComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MeshComponentRequestBus::Events::SetModelAsset, nailongAsset);

                // The GLB Nailong's materials got merged into a single red one by the
                // importer; force the whole model to the yellow Nailong body color.
                OverrideMaterialByProductPath(
                    entity->GetId(), "assets/materials/yellownailong.azmaterial");
                done = true;
                return false;
            }
            return true;
        });
        return done;
    }

    bool YellowMythNailongSystemComponent::CanCreateRuntimeCamera() const
    {
        auto* rpiSystem = AZ::RPI::RPISystemInterface::Get();
        return rpiSystem && rpiSystem->IsInitialized();
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

    void YellowMythNailongSystemComponent::OnImGuiUpdate()
    {
        DrawHud();
    }

    void YellowMythNailongSystemComponent::DrawHud()
    {
        // Gather gameplay state from the entities.
        float playerHp = -1.0f, playerMax = 1.0f;
        float bossHp = -1.0f, bossMax = 1.0f;
        bool bossFound = false, gameOver = false, victory = false, gameStarted = true;

        if (auto* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get())
        {
            appRequests->EnumerateEntities([&](AZ::Entity* entity)
            {
                if (!entity)
                {
                    return true;
                }
                const AZStd::string& name = entity->GetName();
                if (name == "NailongPlayer")
                {
                    if (auto* pc = entity->FindComponent<PlayerControllerComponent>())
                    {
                        playerHp = pc->GetHealth();
                        playerMax = pc->GetMaxHealth();
                    }
                }
                else if (name == "DarkBoss")
                {
                    if (auto* boss = entity->FindComponent<BossAIComponent>())
                    {
                        bossFound = true;
                        bossHp = boss->GetHealth();
                        bossMax = boss->GetMaxHealth();
                    }
                }
                else if (name == "CombatManager")
                {
                    if (auto* cm = entity->FindComponent<CombatManagerComponent>())
                    {
                        gameOver = cm->IsGameOver();
                        victory = cm->IsVictory();
                        gameStarted = cm->IsGameStarted();
                    }
                }
                return true;
            });
        }

        ImGuiIO& io = ImGui::GetIO();
        const float sw = io.DisplaySize.x;
        const float sh = io.DisplaySize.y;

        const ImGuiWindowFlags hudFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

        auto fraction = [](float value, float max)
        {
            float f = max > 0.0f ? value / max : 0.0f;
            return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
        };

        // Title screen until the player first moves/acts.
        if (!gameStarted)
        {
            DrawTitleScreen(sw, sh);
            return;
        }

        // Player HP (top-left).
        if (playerHp >= 0.0f)
        {
            ImGui::SetNextWindowPos(ImVec2(20.0f, 16.0f));
            ImGui::Begin("PlayerHP", nullptr, hudFlags);
            ImGui::Text("NAILONG  %d / %d", static_cast<int>(playerHp), static_cast<int>(playerMax));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.8f, 0.1f, 1.0f));
            ImGui::ProgressBar(fraction(playerHp, playerMax), ImVec2(300.0f, 18.0f));
            ImGui::PopStyleColor();
            ImGui::End();
        }

        // Boss HP (top-right), only once engaged.
        if (bossFound && bossHp > 0.0f)
        {
            ImGui::SetNextWindowPos(ImVec2(sw - 340.0f, 16.0f));
            ImGui::Begin("BossHP", nullptr, hudFlags);
            ImGui::Text("DARK DRAGON  %d / %d", static_cast<int>(bossHp), static_cast<int>(bossMax));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.6f, 0.1f, 0.8f, 1.0f));
            ImGui::ProgressBar(fraction(bossHp, bossMax), ImVec2(300.0f, 18.0f));
            ImGui::PopStyleColor();
            ImGui::End();
        }

        // Objective (top-center).
        {
            const char* objective = victory ? "The Dark Dragon is slain" : "Defeat the Dark Dragon";
            const float tw = ImGui::CalcTextSize(objective).x;
            ImGui::SetNextWindowPos(ImVec2((sw - tw) * 0.5f, 18.0f));
            ImGui::Begin("Objective", nullptr, hudFlags);
            ImGui::TextUnformatted(objective);
            ImGui::End();
        }

        // Controls hint (bottom-center).
        {
            const char* hint = "WASD Move    Space Dodge    Enter / LMB Attack";
            const float tw = ImGui::CalcTextSize(hint).x;
            ImGui::SetNextWindowPos(ImVec2((sw - tw) * 0.5f, sh - 36.0f));
            ImGui::Begin("Hint", nullptr, hudFlags);
            ImGui::TextUnformatted(hint);
            ImGui::End();
        }

        // Game over / victory banner (center).
        if (gameOver)
        {
            const char* title = victory ? "VICTORY!" : "YOU DIED";
            const char* sub = victory ? "Nailong reigns supreme" : "The Yellow Myth has fallen...";
            const char* restart = "Press R to play again";
            const ImVec4 titleColor = victory ? ImVec4(1.0f, 0.85f, 0.1f, 1.0f) : ImVec4(0.9f, 0.15f, 0.1f, 1.0f);

            ImGui::SetNextWindowPos(ImVec2(sw * 0.5f - 220.0f, sh * 0.5f - 70.0f));
            ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));
            ImGui::Begin("GameOver", nullptr, hudFlags);

            ImGui::SetWindowFontScale(3.0f);
            {
                const float tw = ImGui::CalcTextSize(title).x;
                ImGui::SetCursorPosX((440.0f - tw) * 0.5f);
                ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
                ImGui::TextUnformatted(title);
                ImGui::PopStyleColor();
            }
            ImGui::SetWindowFontScale(1.2f);
            for (const char* line : { sub, restart })
            {
                const float tw = ImGui::CalcTextSize(line).x;
                ImGui::SetCursorPosX((440.0f - tw) * 0.5f);
                ImGui::TextUnformatted(line);
            }
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }
    }

    void YellowMythNailongSystemComponent::DrawTitleScreen(float sw, float sh)
    {
        // Dim the whole screen behind the title.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(sw, sh));
        ImGui::Begin("TitleBackdrop", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(0.0f, 0.0f), ImVec2(sw, sh), IM_COL32(8, 8, 12, 225));
        ImGui::End();

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::SetNextWindowPos(ImVec2(sw * 0.5f - 300.0f, sh * 0.30f));
        ImGui::SetNextWindowSize(ImVec2(600.0f, 0.0f));
        ImGui::Begin("Title", nullptr, flags);

        ImGui::SetWindowFontScale(3.2f);
        {
            const char* title = "BLACK MYTH : NAILONG";
            const float tw = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX((600.0f - tw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.15f, 1.0f));
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
        }

        ImGui::SetWindowFontScale(1.15f);
        static const char* story[] = {
            "For a hundred years the little yellow dragon Nailong",
            "dozed in the sunny valley, fat and content.",
            "",
            "Then the Dark Dragon came, and drank the light.",
            "Now only a chubby hero can take it back.",
        };
        for (const char* line : story)
        {
            const float tw = ImGui::CalcTextSize(line).x;
            ImGui::SetCursorPosX((600.0f - tw) * 0.5f);
            ImGui::TextUnformatted(line);
        }

        ImGui::SetWindowFontScale(1.3f);
        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        {
            const char* prompt = "Move to begin your legend";
            const float tw = ImGui::CalcTextSize(prompt).x;
            ImGui::SetCursorPosX((600.0f - tw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
            ImGui::TextUnformatted(prompt);
            ImGui::PopStyleColor();
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }

} // namespace YellowMythNailong
