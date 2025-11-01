#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/server/ServerPlayer.h>

#include <mc/entity/components/UserEntityIdentifierComponent.h>

#include <mc/network/LoopbackPacketSender.h>

#include <mc/network/Packet.h>
#include <mc/network/NetworkSystem.h>
#include <mc/network/NetworkStatistics.h>
#include <mc/network/NetworkIdentifier.h>
#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/ServerNetworkHandler.h>

#include <mc/common/SubClientId.h>

#include "LOICollectionA/include/ServerEvents/NetworkPacketEvent.h"

namespace LOICollection::ServerEvents {
    const NetworkIdentifier& NetworkPacketEvent::getNetworkIdentifier() const {
        return mNetworkIdentifier;
    }

    const Packet& NetworkPacketEvent::getPacket() const {
        return mPacket;
    }

    const Packet& NetworkBroadcastPacketEvent::getPacket() const {
        return mPacket;
    }

    SubClientId NetworkPacketEvent::getSubClientId() const {
        return mSubClientId;
    }

    LL_TYPE_INSTANCE_HOOK(
        NetworkPacketEventHook1,
        HookPriority::Normal,
        NetworkSystem,
        &NetworkSystem::send,
        void,
        NetworkIdentifier const& identifier,
        Packet const& packet,
        SubClientId subClientId
    ) {
        NetworkPacketEvent event(identifier, packet, subClientId);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(identifier, packet, subClientId);
    }

    LL_TYPE_INSTANCE_HOOK(
        NetworkPacketEventHook2,
        HookPriority::Normal,
        NetworkStatistics,
        &NetworkStatistics::$packetReceivedFrom,
        void,
        NetworkIdentifier const& source,
        Packet const& packet,
        uint size
    ) {
        NetworkPacketEvent event(source, packet, packet.mSenderSubId);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(source, packet, size);
    }

    LL_TYPE_INSTANCE_HOOK(
        NetworkBroadcastPacketEventHook1,
        HookPriority::Normal,
        LoopbackPacketSender,
        &LoopbackPacketSender::$sendBroadcast,
        void,
        NetworkIdentifier const& exceptId,
        SubClientId exceptSubid,
        Packet const& packet
    ) {
        NetworkBroadcastPacketEvent event(packet);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(exceptId, exceptSubid, packet);
    }

    LL_TYPE_INSTANCE_HOOK(
        NetworkBroadcastPacketEventHook2,
        HookPriority::Normal,
        LoopbackPacketSender,
        &LoopbackPacketSender::$sendBroadcast,
        void,
        Packet const& packet
    ) {
        NetworkBroadcastPacketEvent event(packet);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;
        
        origin(packet);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory1();
    class NetworkPacketEventEmitter : public ll::event::Emitter<emitterFactory1, NetworkPacketEvent> {
        ll::memory::HookRegistrar<NetworkPacketEventHook1, NetworkPacketEventHook2> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory1() {
        return std::make_unique<NetworkPacketEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory2();
    class NetworkBroadcastPacketEventEmitter : public ll::event::Emitter<emitterFactory2, NetworkBroadcastPacketEvent> {
        ll::memory::HookRegistrar<NetworkBroadcastPacketEventHook1, NetworkBroadcastPacketEventHook2> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory2() {
        return std::make_unique<NetworkBroadcastPacketEventEmitter>();
    }
}
