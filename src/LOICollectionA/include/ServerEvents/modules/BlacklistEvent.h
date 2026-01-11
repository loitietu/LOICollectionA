#pragma once

#include <string>

#include <ll/api/event/Event.h>
#include <ll/api/event/player/PlayerEvent.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::ServerEvents {
    class BlacklistAddEvent final : public ll::event::PlayerEvent {
    protected:
        const std::string& mCause;
        int mTime;
    
    public:
        constexpr explicit BlacklistAddEvent(
            Player& player,
            const std::string& cause,
            int time
        ) : PlayerEvent(player), mCause(cause), mTime(time) {}

        LOICOLLECTION_A_NDAPI std::string getCause() const;
        LOICOLLECTION_A_NDAPI int getTime() const;
    };

    class BlacklistRemoveEvent final : public ll::event::Event {
    protected:
        const std::string& mTarget;

    public:
        constexpr explicit BlacklistRemoveEvent(
            const std::string& target
        ) : mTarget(target) {}

        LOICOLLECTION_A_NDAPI std::string getTarget() const;
    };
}