
#pragma once

#include <AzCore/Component/Component.h>

#include <YellowMythNailong/YellowMythNailongBus.h>

namespace YellowMythNailong
{
    class YellowMythNailongSystemComponent
        : public AZ::Component
        , protected YellowMythNailongRequestBus::Handler
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
        ////////////////////////////////////////////////////////////////////////
        // YellowMythNailongRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////
    };
}
