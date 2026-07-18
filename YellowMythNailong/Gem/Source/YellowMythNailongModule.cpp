#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

#include "YellowMythNailongSystemComponent.h"
#include "PlayerControllerComponent.h"
#include "BossAIComponent.h"
#include "CombatManagerComponent.h"

#include <YellowMythNailong/YellowMythNailongTypeIds.h>

namespace YellowMythNailong
{
    class YellowMythNailongModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(YellowMythNailongModule, YellowMythNailongModuleTypeId, AZ::Module);
        AZ_CLASS_ALLOCATOR(YellowMythNailongModule, AZ::SystemAllocator);

        YellowMythNailongModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                YellowMythNailongSystemComponent::CreateDescriptor(),
                PlayerControllerComponent::CreateDescriptor(),
                BossAIComponent::CreateDescriptor(),
                CombatManagerComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<YellowMythNailongSystemComponent>(),
            };
        }
    };
}// namespace YellowMythNailong

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), YellowMythNailong::YellowMythNailongModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_YellowMythNailong, YellowMythNailong::YellowMythNailongModule)
#endif
