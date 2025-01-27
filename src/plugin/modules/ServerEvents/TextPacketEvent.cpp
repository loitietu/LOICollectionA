#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/NetworkIdentifier.h>
#include <mc/network/LoopbackPacketSender.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/packet/TextPacketType.h>

#include <mc/common/SubClientId.h>

#include "include/ServerEvents/TextPacketEvent.h"

namespace LOICollection::ServerEvents {
    const TextPacket& TextPacketEvent::getPacket() const {
        return this->mPacket;
    }

    LL_TYPE_INSTANCE_HOOK(
        TextPacketEventHook1,
        HookPriority::Normal,
        LoopbackPacketSender,
        &LoopbackPacketSender::$sendBroadcast,
        void,
        NetworkIdentifier const& identifier,
        SubClientId subId,
        Packet const& packet
    ) {
        if (packet.getId() != MinecraftPacketIds::Text)
            return;

        TextPacketEvent event(static_cast<TextPacket const&>(packet));
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;
        origin(identifier, subId, packet);
    }

    LL_TYPE_INSTANCE_HOOK(
        TextPacketEventHook2,
        HookPriority::Normal,
        LoopbackPacketSender,
        &LoopbackPacketSender::$sendBroadcast,
        void,
        Packet const& packet
    ) {
        if (packet.getId() != MinecraftPacketIds::Text)
            return;

        TextPacketEvent event(static_cast<TextPacket const&>(packet));
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;
        origin(packet);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class TextPacketEventEmitter : public ll::event::Emitter<emitterFactory, TextPacketEvent> {
        ll::memory::HookRegistrar<TextPacketEventHook1> hook1;
        ll::memory::HookRegistrar<TextPacketEventHook2> hook2;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<TextPacketEventEmitter>();
    }
}