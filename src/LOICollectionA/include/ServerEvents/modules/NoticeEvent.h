#pragma once

#include <string>

#include <ll/api/event/Event.h>
#include <ll/api/event/Cancellable.h>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::ServerEvents {
    class NoticeCreateEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const std::string& mTarget;
        const std::string& mTitle;
        int mPriority;
        bool mPoiontout;
    
    public:
        constexpr explicit NoticeCreateEvent(
            const std::string& target,
            const std::string& title,
            int priority,
            bool poiontout
        ) : mTarget(target), mTitle(title), mPriority(priority), mPoiontout(poiontout) {}

        LOICOLLECTION_A_NDAPI std::string getTarget() const;
        LOICOLLECTION_A_NDAPI std::string getTitle() const;
        LOICOLLECTION_A_NDAPI int getPriority() const;
        LOICOLLECTION_A_NDAPI bool isPoiontout() const;
    };

    class NoticeDeleteEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const std::string& mTarget;

    public:
        constexpr explicit NoticeDeleteEvent(
            const std::string& target
        ) : mTarget(target) {}

        LOICOLLECTION_A_NDAPI std::string getTarget() const;
    };
}