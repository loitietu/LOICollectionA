#pragma once

#include <ll/api/event/Event.h>
#include <ll/api/event/Cancellable.h>

#include "base/Macro.h"

class TextPacket;

namespace LOICollection::ServerEvents {
    class TextPacketEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const TextPacket& mPacket;

    public:
        constexpr explicit TextPacketEvent(
            const TextPacket& packet
        ) : mPacket(packet) {}

        LOICOLLECTION_A_NDAPI const TextPacket& getPacket() const;
    };
}