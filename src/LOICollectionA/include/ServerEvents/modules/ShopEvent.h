#pragma once

#include <string>

#include <ll/api/event/Event.h>
#include <ll/api/event/Cancellable.h>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::ServerEvents {
    class ShopCreateEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const std::string& mTarget;
    
    public:
        constexpr explicit ShopCreateEvent(
            const std::string& target
        ) : mTarget(target) {}

        LOICOLLECTION_A_NDAPI std::string getTarget() const;
    };

    class ShopDeleteEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const std::string& mTarget;
    
    public:
        constexpr explicit ShopDeleteEvent(
            const std::string& target
        ) : mTarget(target) {}

        LOICOLLECTION_A_NDAPI std::string getTarget() const;
    };
}