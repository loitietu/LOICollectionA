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

#include "LOICollectionA/include/ServerEvents/server/NetworkPacketEvent.h"

namespace LOICollection::ServerEvents {
    const NetworkIdentifier& NetworkPacketBeforeEvent::getNetworkIdentifier() const {
        return mNetworkIdentifier;
    }

    const NetworkIdentifier& NetworkPacketAfterEvent::getNetworkIdentifier() const {
        return mNetworkIdentifier;
    }

    const Packet& NetworkPacketBeforeEvent::getPacket() const {
        return mPacket;
    }

    const Packet& NetworkPacketAfterEvent::getPacket() const {
        return mPacket;
    }

    const Packet& NetworkBroadcastPacketEvent::getPacket() const {
        return mPacket;
    }

    SubClientId NetworkPacketBeforeEvent::getSubClientId() const {
        return mSubClientId;
    }

    SubClientId NetworkPacketAfterEvent::getSubClientId() const {
        return mSubClientId;
    }

    NetworkPacketType NetworkPacketBeforeEvent::getType() const {
        return mType;
    }

    NetworkPacketType NetworkPacketAfterEvent::getType() const {
        return mType;
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
        NetworkPacketBeforeEvent beforeEvent(identifier, packet, subClientId, NetworkPacketType::send);
        ll::event::EventBus::getInstance().publish(beforeEvent);
        if (beforeEvent.isCancelled())
            return;

        origin(identifier, packet, subClientId);

        NetworkPacketAfterEvent afterEvent(identifier, packet, subClientId, NetworkPacketType::send);
        ll::event::EventBus::getInstance().publish(afterEvent);
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
        NetworkPacketBeforeEvent beforeEvent(source, packet, packet.mSenderSubId, NetworkPacketType::receive);
        ll::event::EventBus::getInstance().publish(beforeEvent);
        if (beforeEvent.isCancelled())
            return;

        origin(source, packet, size);
        
        NetworkPacketAfterEvent afterEvent(source, packet, packet.mSenderSubId, NetworkPacketType::receive);
        ll::event::EventBus::getInstance().publish(afterEvent);
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
    class NetworkPacketEventEmitter : public ll::event::Emitter<emitterFactory1, NetworkPacketBeforeEvent, NetworkPacketAfterEvent> {
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
