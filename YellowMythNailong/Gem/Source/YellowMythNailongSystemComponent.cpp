
#include <AzCore/Serialization/SerializeContext.h>

#include "YellowMythNailongSystemComponent.h"

#include <YellowMythNailong/YellowMythNailongTypeIds.h>

namespace YellowMythNailong
{
    AZ_COMPONENT_IMPL(YellowMythNailongSystemComponent, "YellowMythNailongSystemComponent",
        YellowMythNailongSystemComponentTypeId);

    void YellowMythNailongSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<YellowMythNailongSystemComponent, AZ::Component>()
                ->Version(0)
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
    }

    void YellowMythNailongSystemComponent::Deactivate()
    {
        YellowMythNailongRequestBus::Handler::BusDisconnect();
    }
}
