
#pragma once

#include <YellowMythNailong/YellowMythNailongTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace YellowMythNailong
{
    class YellowMythNailongRequests
    {
    public:
        AZ_RTTI(YellowMythNailongRequests, YellowMythNailongRequestsTypeId);
        virtual ~YellowMythNailongRequests() = default;
        // Put your public methods here
    };

    class YellowMythNailongBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using YellowMythNailongRequestBus = AZ::EBus<YellowMythNailongRequests, YellowMythNailongBusTraits>;
    using YellowMythNailongInterface = AZ::Interface<YellowMythNailongRequests>;

} // namespace YellowMythNailong
