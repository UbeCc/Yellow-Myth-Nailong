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
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentConstants.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentConstants.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialAssignment.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/TransformBus.h>

#include "PlayerControllerComponent.h"
#include "BossAIComponent.h"
#include "CombatManagerComponent.h"

#include <imgui/imgui.h>
#include <cmath>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Quaternion.h>

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
        CombatNotificationBus::Handler::BusConnect();

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
        CombatNotificationBus::Handler::BusDisconnect();
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

    void YellowMythNailongSystemComponent::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (!m_runtimeCameraId.IsValid() && CanCreateRuntimeCamera())
        {
            SetupRuntimeCamera();
        }

        if (!m_nailongModelSet)
        {
            m_nailongModelSet = SetupNailongModel();
        }

        if (!m_materialsApplied)
        {
            m_materialsApplied = ApplyArenaMaterials();
        }

        // Age and retire floating combat text.
        for (auto it = m_floatingTexts.begin(); it != m_floatingTexts.end();)
        {
            it->m_age += deltaTime;
            if (it->m_age >= it->m_lifetime)
            {
                it = m_floatingTexts.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Age and retire visual effects.
        for (auto it = m_vfx.begin(); it != m_vfx.end();)
        {
            it->m_age += deltaTime;
            if (it->m_age >= it->m_lifetime)
            {
                it = m_vfx.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Footstep dust while the player runs, and speed streaks behind a charging boss.
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
                    AZ::Vector3 pos = AZ::Vector3::CreateZero();
                    AZ::TransformBus::EventResult(pos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                    const float speed = (pos - m_lastDustPos).GetLength() * 60.0f; // rough m/s at 60 Hz
                    m_lastDustPos = pos;
                    if (speed > 2.0f)
                    {
                        m_dustTimer -= deltaTime;
                        if (m_dustTimer <= 0.0f)
                        {
                            m_dustTimer = 0.16f;
                            SpawnVfx(Vfx::DustPuff, pos + AZ::Vector3(0.0f, 0.0f, 0.05f), AZ::Vector3::CreateAxisY(),
                                0.75f, 0.65f, 0.5f, 0.7f, 0.45f);
                        }
                    }
                }
                else if (name == "DarkBoss")
                {
                    if (auto* boss = entity->FindComponent<BossAIComponent>())
                    {
                        if (boss->IsCharging())
                        {
                            m_chargeStreakTimer -= deltaTime;
                            if (m_chargeStreakTimer <= 0.0f)
                            {
                                m_chargeStreakTimer = 0.05f;
                                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                                AZ::TransformBus::EventResult(pos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                                AZ::Vector3 motion = pos - m_lastBossPos;
                                motion.SetZ(0.0f);
                                if (motion.GetLengthSq() < 0.001f)
                                {
                                    motion = AZ::Vector3::CreateAxisY();
                                }
                                motion.Normalize();
                                m_lastBossPos = pos;
                                SpawnVfx(Vfx::SpeedStreak, pos + AZ::Vector3(0.0f, 0.0f, 2.0f), motion,
                                    1.0f, 0.25f, 0.2f, 2.5f, 0.3f);
                            }
                        }
                    }
                }
                return true;
            });
        }

        if (m_hurtFlash > 0.0f)
        {
            m_hurtFlash = AZ::GetMax(0.0f, m_hurtFlash - deltaTime * 2.5f);
        }
        if (m_bannerTimer > 0.0f)
        {
            m_bannerTimer -= deltaTime;
        }
        if (m_comboPopupTimer > 0.0f)
        {
            m_comboPopupTimer -= deltaTime;
        }
    }

    void YellowMythNailongSystemComponent::SpawnFloatingText(
        const AZ::Vector3& worldPos, const char* text, float r, float g, float b, float scale)
    {
        FloatingText ft;
        ft.m_worldPos = worldPos;
        ft.m_text = text;
        ft.m_r = r;
        ft.m_g = g;
        ft.m_b = b;
        ft.m_scale = scale;
        m_floatingTexts.push_back(ft);
    }

    void YellowMythNailongSystemComponent::SpawnVfx(
        Vfx::Type type, const AZ::Vector3& pos, const AZ::Vector3& dir, float r, float g, float b, float size, float lifetime)
    {
        Vfx v;
        v.m_type = type;
        v.m_pos = pos;
        v.m_dir = dir;
        v.m_r = r;
        v.m_g = g;
        v.m_b = b;
        v.m_size = size;
        v.m_lifetime = lifetime;
        m_vfx.push_back(v);
    }

    void YellowMythNailongSystemComponent::SpawnSlashArc(
        const AZ::Vector3& attackerPos, const AZ::Vector3& facing, bool finisher, bool hostile)
    {
        const float r = hostile ? 1.0f : 1.0f;
        const float g = hostile ? 0.22f : 0.85f;
        const float b = hostile ? 0.15f : 0.35f;
        SpawnVfx(Vfx::SlashArc, attackerPos, facing, r, g, b, finisher ? 1.6f : 1.0f, 0.22f);
        if (finisher)
        {
            SpawnVfx(Vfx::ShockRing, attackerPos, facing, r, g, b, 1.0f, 0.4f);
        }
    }

    void YellowMythNailongSystemComponent::SpawnImpactBurst(const AZ::Vector3& pos, float r, float g, float b)
    {
        // Eight short rays shooting out of the impact point.
        for (int i = 0; i < 8; ++i)
        {
            const float angle = AZ::Constants::TwoPi * static_cast<float>(i) / 8.0f + 0.4f;
            const AZ::Vector3 dir(cosf(angle), sinf(angle), (i % 2 == 0) ? 0.6f : -0.2f);
            SpawnVfx(Vfx::SparkRay, pos, dir, r, g, b, 1.0f, 0.16f);
        }
    }

    void YellowMythNailongSystemComponent::SpawnDustBurst(const AZ::Vector3& pos, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const float angle = AZ::Constants::TwoPi * static_cast<float>(i) / count;
            SpawnVfx(Vfx::DustPuff, pos + AZ::Vector3(cosf(angle) * 0.4f, sinf(angle) * 0.4f, 0.05f),
                AZ::Vector3::CreateAxisY(), 0.75f, 0.65f, 0.5f, 1.1f, 0.5f);
        }
    }

    void YellowMythNailongSystemComponent::OnPlayerAttack(const AZ::Vector3& position, float /*radius*/, float damage)
    {
        // Bright slash arc in front of the player along its facing.
        AZ::Vector3 facing = AZ::Vector3::CreateAxisY();
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (appRequests)
        {
            appRequests->EnumerateEntities([&](AZ::Entity* entity)
            {
                if (entity && entity->GetName() == "NailongPlayer")
                {
                    AZ::Quaternion rotation = AZ::Quaternion::CreateIdentity();
                    AZ::TransformBus::EventResult(rotation, entity->GetId(), &AZ::TransformInterface::GetWorldRotationQuaternion);
                    facing = rotation.TransformVector(AZ::Vector3::CreateAxisY());
                    facing.SetZ(0.0f);
                    if (facing.GetLengthSq() < 0.001f)
                    {
                        facing = AZ::Vector3::CreateAxisY();
                    }
                    facing.Normalize();
                    return false;
                }
                return true;
            });
        }
        SpawnSlashArc(position, facing, damage >= 70.0f, false);
    }

    void YellowMythNailongSystemComponent::OnBossAttack(const AZ::Vector3& position, float /*radius*/, float /*damage*/)
    {
        // Red slash arc from the boss toward the player.
        AZ::Vector3 facing = AZ::Vector3::CreateAxisY();
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (appRequests)
        {
            AZ::Vector3 playerPos = AZ::Vector3::CreateZero();
            bool found = false;
            appRequests->EnumerateEntities([&](AZ::Entity* entity)
            {
                if (entity && entity->GetName() == "NailongPlayer")
                {
                    AZ::TransformBus::EventResult(playerPos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                    found = true;
                    return false;
                }
                return true;
            });
            if (found)
            {
                facing = playerPos - position;
                facing.SetZ(0.0f);
                if (facing.GetLengthSq() > 0.001f)
                {
                    facing.Normalize();
                }
            }
        }
        SpawnSlashArc(position, facing, false, true);
    }

    void YellowMythNailongSystemComponent::OnPlayerDodged()
    {
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }
        appRequests->EnumerateEntities([this](AZ::Entity* entity)
        {
            if (entity && entity->GetName() == "NailongPlayer")
            {
                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(pos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                SpawnDustBurst(pos, 6);
                return false;
            }
            return true;
        });
    }

    void YellowMythNailongSystemComponent::OnProjectileTick(const AZ::Vector3& position, bool hostile)
    {
        // Droplet trail behind the flying projectile.
        if (hostile)
        {
            SpawnVfx(Vfx::SparkRay, position, AZ::Vector3::CreateAxisZ(), 0.9f, 0.2f, 0.25f, 0.6f, 0.12f);
        }
        else
        {
            SpawnVfx(Vfx::SparkRay, position, AZ::Vector3::CreateAxisZ(), 0.95f, 0.95f, 0.9f, 0.5f, 0.10f);
        }
    }

    void YellowMythNailongSystemComponent::OnBossDamaged(float damage)
    {
        // Gold number over the boss; the combo finisher pops bigger.
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }
        appRequests->EnumerateEntities([this, damage](AZ::Entity* entity)
        {
            if (entity && entity->GetName() == "DarkBoss")
            {
                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(pos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                pos += AZ::Vector3(0.0f, 0.0f, 2.5f);
                const bool finisher = damage >= 70.0f;
                const AZStd::string text = AZStd::string::format("-%d", static_cast<int>(damage));
                SpawnFloatingText(pos, text.c_str(), 1.0f, 0.85f, 0.2f, finisher ? 2.2f : 1.4f);
                SpawnImpactBurst(pos, 1.0f, 0.85f, 0.3f);
                return false;
            }
            return true;
        });
    }

    void YellowMythNailongSystemComponent::OnPlayerDamaged(float damage)
    {
        m_hurtFlash = 1.0f;

        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }
        appRequests->EnumerateEntities([this, damage](AZ::Entity* entity)
        {
            if (entity && entity->GetName() == "NailongPlayer")
            {
                AZ::Vector3 pos = AZ::Vector3::CreateZero();
                AZ::TransformBus::EventResult(pos, entity->GetId(), &AZ::TransformInterface::GetWorldTranslation);
                pos += AZ::Vector3(0.0f, 0.0f, 2.0f);
                const AZStd::string text = AZStd::string::format("-%d", static_cast<int>(damage));
                SpawnFloatingText(pos, text.c_str(), 1.0f, 0.25f, 0.2f, 1.3f);
                SpawnImpactBurst(pos, 1.0f, 0.3f, 0.25f);
                SpawnDustBurst(pos - AZ::Vector3(0.0f, 0.0f, 1.9f), 3);
                return false;
            }
            return true;
        });
    }

    void YellowMythNailongSystemComponent::OnBossEngaged()
    {
        m_bannerKind = 1;
        m_bannerTimer = 3.0f;
    }

    void YellowMythNailongSystemComponent::OnBossEnraged()
    {
        m_bannerKind = 2;
        m_bannerTimer = 3.0f;
    }

    void YellowMythNailongSystemComponent::OnPlayerComboStrike()
    {
        m_comboPopupTimer = 0.9f;
    }

    bool YellowMythNailongSystemComponent::WorldToScreen(
        const AZ::Vector3& worldPos, float screenW, float screenH, float& outX, float& outY, float& outDepth) const
    {
        if (!m_runtimeCameraId.IsValid())
        {
            return false;
        }

        AZ::Transform camTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(camTM, m_runtimeCameraId, &AZ::TransformInterface::GetWorldTM);
        const AZ::Vector3 v = camTM.GetInverse().TransformPoint(worldPos);

        // The camera looks down its local +Y axis; screen-right is +X, up is +Z.
        const float depth = v.GetY();
        if (depth < 0.2f)
        {
            return false;
        }
        const float f = 1.0f / tanf(AZ::DegToRad(60.0f) * 0.5f);
        const float aspect = screenW / screenH;
        const float ndcX = (v.GetX() * f / aspect) / depth;
        const float ndcY = (v.GetZ() * f) / depth;
        outX = (ndcX * 0.5f + 0.5f) * screenW;
        outY = (0.5f - ndcY * 0.5f) * screenH;
        outDepth = depth;
        return true;
    }

    bool YellowMythNailongSystemComponent::SetupNailongModel()
    {
        auto nailongAsset = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::ModelAsset>(
            "quaternius/monsterpack/obj/nailongbody.glb.azmodel");
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
            if (entity && entity->GetName() == "NailongBody")
            {
                AZ::Render::MeshComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MeshComponentRequestBus::Events::SetModelAsset, nailongAsset);

                // Coloring is handled by ApplyArenaMaterials via property overrides.
                done = true;
                return false;
            }
            return true;
        });
        return done;
    }

    bool YellowMythNailongSystemComponent::ApplyArenaMaterials()
    {
        // Character parts get their colors from the cooked material assets (the only
        // override path the renderer honors for these meshes); the ground gets a
        // procedural texture through a property override instead.
        auto groundTexture = AZ::RPI::AssetUtils::LoadAssetByProductPath<AZ::RPI::StreamingImageAsset>(
            "assets/textures/ground.png.streamingimage");
        if (!groundTexture.IsReady())
        {
            return false; // texture still processing; retry next tick
        }
        AZ::Data::Asset<AZ::RPI::ImageAsset> groundImage(groundTexture);

        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return false;
        }

        const AZ::Render::MaterialAssignmentId slot = AZ::Render::DefaultMaterialAssignmentId;

        appRequests->EnumerateEntities([&](AZ::Entity* entity)
        {
            if (!entity)
            {
                return true;
            }
            const AZStd::string& name = entity->GetName();
            if (name == "DarkBoss")
            {
                // Near-black with a violet tinge.
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("baseColor.color"), AZStd::any(AZ::Color(0.02f, 0.012f, 0.035f, 1.0f)));
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("roughness.factor"), AZStd::any(0.6f));
            }
            else if (name == "Ground")
            {
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("baseColor.color"), AZStd::any(AZ::Color(0.42f, 0.30f, 0.17f, 1.0f)));
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("roughness.factor"), AZStd::any(0.95f));
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("baseColor.textureMap"), AZStd::any(groundImage));
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("uv.tileU"), AZStd::any(40.0f));
                AZ::Render::MaterialComponentRequestBus::Event(
                    entity->GetId(), &AZ::Render::MaterialComponentRequestBus::Events::SetPropertyValue, slot,
                    AZStd::string("uv.tileV"), AZStd::any(40.0f));
            }
            return true;
        });

        AZLOG_INFO("NailongSystem: applied arena material overrides");
        return true;
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
        float spitCooldown = 0.0f, spitCooldownMax = 1.0f;
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
                        spitCooldown = pc->GetSpitCooldownRemaining();
                        spitCooldownMax = pc->GetSpitCooldownMax();
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
            const char* hint = "WASD Move    Space Dodge    Enter / LMB Attack    Q Milk Spit";
            const float tw = ImGui::CalcTextSize(hint).x;
            ImGui::SetNextWindowPos(ImVec2((sw - tw) * 0.5f, sh - 36.0f));
            ImGui::Begin("Hint", nullptr, hudFlags);
            ImGui::TextUnformatted(hint);
            ImGui::End();
        }

        // Milk spit skill indicator (bottom-right).
        if (playerHp >= 0.0f && !gameOver)
        {
            const bool ready = spitCooldown <= 0.0f;
            ImGui::SetNextWindowPos(ImVec2(sw - 200.0f, sh - 90.0f));
            ImGui::SetNextWindowSize(ImVec2(180.0f, 0.0f));
            ImGui::Begin("SpitSkill", nullptr, hudFlags);
            if (ready)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.9f, 1.0f));
                ImGui::TextUnformatted("[Q] MILK SPIT");
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("[Q] %.1fs", spitCooldown);
            }
            ImGui::PopStyleColor();
            if (!ready)
            {
                ImGui::ProgressBar(1.0f - spitCooldown / spitCooldownMax, ImVec2(160.0f, 6.0f));
            }
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

        // Combo finisher popup.
        if (m_comboPopupTimer > 0.0f)
        {
            const float a = AZ::GetClamp(m_comboPopupTimer / 0.9f, 0.0f, 1.0f);
            ImGui::SetNextWindowPos(ImVec2(sw * 0.5f - 260.0f, sh * 0.62f));
            ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f));
            ImGui::Begin("ComboPopup", nullptr, hudFlags);
            ImGui::SetWindowFontScale(1.6f + 0.4f * (m_comboPopupTimer / 0.9f));
            const char* popup = "NAILONG SMASH!";
            const float tw = ImGui::CalcTextSize(popup).x;
            ImGui::SetCursorPosX((520.0f - tw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.15f, a));
            ImGui::TextUnformatted(popup);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }

        DrawBossBanner(sw, sh);
        DrawTelegraphRing(sw, sh);
        DrawVfx(sw, sh);
        DrawFloatingTexts(sw, sh);
        DrawVignette(sw, sh);
    }

    void YellowMythNailongSystemComponent::DrawVfx(float sw, float sh)
    {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        for (const Vfx& v : m_vfx)
        {
            const float t = AZ::GetClamp(v.m_age / v.m_lifetime, 0.0f, 1.0f);
            const float alpha = 1.0f - t;
            const ImU32 colCore = IM_COL32(
                static_cast<int>(v.m_r * 255), static_cast<int>(v.m_g * 255), static_cast<int>(v.m_b * 255),
                static_cast<int>(235 * alpha));
            const ImU32 colGlow = IM_COL32(
                static_cast<int>(v.m_r * 255), static_cast<int>(v.m_g * 255), static_cast<int>(v.m_b * 255),
                static_cast<int>(90 * alpha));

            if (v.m_type == Vfx::SlashArc)
            {
                // Expanding crescent in front of the attacker, at chest height.
                const float baseAngle = atan2f(v.m_dir.GetY(), v.m_dir.GetX());
                const float radius = (1.4f + 2.4f * t) * v.m_size;
                const float z = v.m_pos.GetZ() + 1.2f;
                ImVec2 pts[16];
                int n = 0;
                for (int i = 0; i < 16; ++i)
                {
                    const float a = baseAngle + AZ::DegToRad(-75.0f + 150.0f * static_cast<float>(i) / 15.0f);
                    const AZ::Vector3 p(v.m_pos.GetX() + cosf(a) * radius, v.m_pos.GetY() + sinf(a) * radius, z);
                    float sx = 0.0f, sy = 0.0f, d = 0.0f;
                    if (WorldToScreen(p, sw, sh, sx, sy, d))
                    {
                        pts[n++] = ImVec2(sx, sy);
                    }
                }
                if (n >= 2)
                {
                    drawList->AddPolyline(pts, n, colGlow, false, 14.0f * v.m_size);
                    drawList->AddPolyline(pts, n, colCore, false, 5.0f * v.m_size);
                }
            }
            else if (v.m_type == Vfx::SparkRay)
            {
                const float len = 1.6f * (0.4f + t);
                float x1 = 0.0f, y1 = 0.0f, d1 = 0.0f, x2 = 0.0f, y2 = 0.0f, d2 = 0.0f;
                if (WorldToScreen(v.m_pos, sw, sh, x1, y1, d1) &&
                    WorldToScreen(v.m_pos + v.m_dir * len, sw, sh, x2, y2, d2))
                {
                    drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), colCore, 3.0f);
                }
            }
            else if (v.m_type == Vfx::DustPuff)
            {
                const float radius = (0.25f + 0.9f * t) * v.m_size;
                ImVec2 pts[13];
                int n = 0;
                for (int i = 0; i <= 12; ++i)
                {
                    const float a = AZ::Constants::TwoPi * static_cast<float>(i) / 12.0f;
                    const AZ::Vector3 p = v.m_pos + AZ::Vector3(cosf(a) * radius, sinf(a) * radius, 0.0f);
                    float sx = 0.0f, sy = 0.0f, d = 0.0f;
                    if (WorldToScreen(p, sw, sh, sx, sy, d))
                    {
                        pts[n++] = ImVec2(sx, sy);
                    }
                }
                if (n >= 3)
                {
                    drawList->AddPolyline(pts, n, colGlow, true, 3.0f);
                }
            }
            else if (v.m_type == Vfx::ShockRing)
            {
                const float radius = (1.0f + 4.5f * t) * v.m_size;
                ImVec2 pts[25];
                int n = 0;
                for (int i = 0; i <= 24; ++i)
                {
                    const float a = AZ::Constants::TwoPi * static_cast<float>(i) / 24.0f;
                    const AZ::Vector3 p = v.m_pos + AZ::Vector3(cosf(a) * radius, sinf(a) * radius, 0.1f);
                    float sx = 0.0f, sy = 0.0f, d = 0.0f;
                    if (WorldToScreen(p, sw, sh, sx, sy, d))
                    {
                        pts[n++] = ImVec2(sx, sy);
                    }
                }
                if (n >= 3)
                {
                    drawList->AddPolyline(pts, n, colGlow, true, 10.0f);
                    drawList->AddPolyline(pts, n, colCore, true, 4.0f);
                }
            }
            else if (v.m_type == Vfx::SpeedStreak)
            {
                const float len = v.m_size * (0.5f + t);
                float x1 = 0.0f, y1 = 0.0f, d1 = 0.0f, x2 = 0.0f, y2 = 0.0f, d2 = 0.0f;
                if (WorldToScreen(v.m_pos, sw, sh, x1, y1, d1) &&
                    WorldToScreen(v.m_pos - v.m_dir * len, sw, sh, x2, y2, d2))
                {
                    drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), colCore, 4.0f);
                }
            }
        }
    }

    void YellowMythNailongSystemComponent::DrawFloatingTexts(float sw, float sh)
    {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        for (const FloatingText& ft : m_floatingTexts)
        {
            // Rise and fade over the lifetime.
            AZ::Vector3 pos = ft.m_worldPos + AZ::Vector3(0.0f, 0.0f, 1.6f * ft.m_age);
            float sx = 0.0f, sy = 0.0f, depth = 0.0f;
            if (!WorldToScreen(pos, sw, sh, sx, sy, depth))
            {
                continue;
            }
            const float alpha = AZ::GetClamp(1.0f - ft.m_age / ft.m_lifetime, 0.0f, 1.0f);
            const ImU32 outline = IM_COL32(0, 0, 0, static_cast<int>(200 * alpha));
            const ImU32 color = IM_COL32(
                static_cast<int>(ft.m_r * 255), static_cast<int>(ft.m_g * 255), static_cast<int>(ft.m_b * 255),
                static_cast<int>(255 * alpha));
            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize() * ft.m_scale;
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ft.m_text.c_str());
            const ImVec2 center(sx - textSize.x * 0.5f, sy - textSize.y * 0.5f);
            for (const ImVec2& o : { ImVec2(-1.5f, 0.0f), ImVec2(1.5f, 0.0f), ImVec2(0.0f, -1.5f), ImVec2(0.0f, 1.5f) })
            {
                drawList->AddText(font, fontSize, ImVec2(center.x + o.x, center.y + o.y), outline, ft.m_text.c_str());
            }
            drawList->AddText(font, fontSize, center, color, ft.m_text.c_str());
        }
    }

    void YellowMythNailongSystemComponent::DrawBossBanner(float sw, float sh)
    {
        if (m_bannerTimer <= 0.0f || m_bannerKind == 0)
        {
            return;
        }

        const float alpha = AZ::GetClamp(m_bannerTimer, 0.0f, 1.0f);
        const bool enrage = (m_bannerKind == 2);
        const char* title = enrage ? "THE DARK DRAGON IS ENRAGED" : "DARK DRAGON";
        const char* sub = enrage ? "Its fury doubles. Watch for the charge." : "Devourer of the Light";
        const ImVec4 color = enrage ? ImVec4(1.0f, 0.2f, 0.1f, alpha) : ImVec4(0.85f, 0.2f, 0.9f, alpha);

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::SetNextWindowPos(ImVec2(sw * 0.5f - 400.0f, sh * 0.20f));
        ImGui::SetNextWindowSize(ImVec2(800.0f, 0.0f));
        ImGui::Begin("BossBanner", nullptr, flags);

        ImGui::SetWindowFontScale(2.6f);
        {
            const float tw = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX((800.0f - tw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
        }
        ImGui::SetWindowFontScale(1.2f);
        {
            const float tw = ImGui::CalcTextSize(sub).x;
            ImGui::SetCursorPosX((800.0f - tw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, alpha * 0.9f));
            ImGui::TextUnformatted(sub);
            ImGui::PopStyleColor();
        }
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }

    void YellowMythNailongSystemComponent::DrawTelegraphRing(float sw, float sh)
    {
        // While the boss telegraphs its charge, paint the danger zone on the ground.
        AZ::ComponentApplicationRequests* appRequests = AZ::Interface<AZ::ComponentApplicationRequests>::Get();
        if (!appRequests)
        {
            return;
        }

        AZ::Vector3 target;
        float radius = 0.0f;
        bool telegraphing = false;
        appRequests->EnumerateEntities([&](AZ::Entity* entity)
        {
            if (entity && entity->GetName() == "DarkBoss")
            {
                if (auto* boss = entity->FindComponent<BossAIComponent>())
                {
                    telegraphing = boss->IsTelegraphing();
                    target = boss->GetTelegraphTarget();
                    radius = boss->GetTelegraphRadius();
                }
                return false;
            }
            return true;
        });
        if (!telegraphing)
        {
            return;
        }

        // Draw the danger zone as a perspective-correct ring on the ground:
        // sample world-space points around the circle and project each one.
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        AZStd::vector<ImVec2> points;
        for (int i = 0; i < 40; ++i)
        {
            const float angle = AZ::Constants::TwoPi * static_cast<float>(i) / 40.0f;
            const AZ::Vector3 p = target + AZ::Vector3(cosf(angle) * radius, sinf(angle) * radius, 0.15f);
            float px = 0.0f, py = 0.0f, pd = 0.0f;
            if (WorldToScreen(p, sw, sh, px, py, pd))
            {
                points.push_back(ImVec2(px, py));
            }
        }
        if (points.size() < 3)
        {
            return;
        }
        drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), IM_COL32(255, 30, 20, 55));
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(255, 70, 50, 230), true, 3.0f);
    }

    void YellowMythNailongSystemComponent::DrawVignette(float sw, float sh)
    {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        // Permanent soft vignette for a cinematic frame.
        const int baseAlpha = 42;
        const float edge = sh * 0.18f;
        const ImU32 dark = IM_COL32(0, 0, 0, baseAlpha);
        const ImU32 clear = IM_COL32(0, 0, 0, 0);
        drawList->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(sw, edge), dark, dark, clear, clear);
        drawList->AddRectFilledMultiColor(ImVec2(0.0f, sh - edge), ImVec2(sw, sh), clear, clear, dark, dark);
        drawList->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(edge, sh), dark, clear, clear, dark);
        drawList->AddRectFilledMultiColor(ImVec2(sw - edge, 0.0f), ImVec2(sw, sh), clear, dark, dark, clear);

        // Red hurt flash on top when the player takes a hit.
        if (m_hurtFlash > 0.0f)
        {
            const int a = static_cast<int>(150.0f * m_hurtFlash);
            const ImU32 red = IM_COL32(200, 10, 10, a);
            const ImU32 redClear = IM_COL32(200, 10, 10, 0);
            drawList->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(sw, edge * 1.4f), red, red, redClear, redClear);
            drawList->AddRectFilledMultiColor(ImVec2(0.0f, sh - edge * 1.4f), ImVec2(sw, sh), redClear, redClear, red, red);
            drawList->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(edge * 1.4f, sh), red, redClear, redClear, red);
            drawList->AddRectFilledMultiColor(ImVec2(sw - edge * 1.4f, 0.0f), ImVec2(sw, sh), redClear, red, red, redClear);
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
