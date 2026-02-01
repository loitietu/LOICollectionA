#pragma once

#include <ll/api/event/Event.h>
#include <ll/api/event/Cancellable.h>

#include <mc/common/SubClientId.h>

#include "LOICollectionA/base/Macro.h"

class Packet;
class ServerPlayer;
class NetworkIdentifier;

namespace LOICollection::ServerEvents {
    enum class NetworkPacketType {
        send,
        receive
    };

    class NetworkPacketBeforeEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const NetworkIdentifier& mNetworkIdentifier;
        const Packet& mPacket;
        
        SubClientId mSubClientId;

        NetworkPacketType mType;
    
    public:
        constexpr explicit NetworkPacketBeforeEvent(
            const NetworkIdentifier& networkIdentifier,
            const Packet& packet,
            SubClientId subClientId,
            NetworkPacketType type
        ) : mNetworkIdentifier(networkIdentifier), mPacket(packet), mSubClientId(subClientId), mType(type) {}

        LOICOLLECTION_A_NDAPI const NetworkIdentifier& getNetworkIdentifier() const;
        LOICOLLECTION_A_NDAPI const Packet& getPacket() const;
        LOICOLLECTION_A_NDAPI SubClientId getSubClientId() const;
        LOICOLLECTION_A_NDAPI NetworkPacketType getType() const;
    };

    class NetworkPacketAfterEvent final : public ll::event::Event {
    protected:
        const NetworkIdentifier& mNetworkIdentifier;
        const Packet& mPacket;
        
        SubClientId mSubClientId;

        NetworkPacketType mType;
    
    public:
        constexpr explicit NetworkPacketAfterEvent(
            const NetworkIdentifier& networkIdentifier,
            const Packet& packet,
            SubClientId subClientId,
            NetworkPacketType type
        ) : mNetworkIdentifier(networkIdentifier), mPacket(packet), mSubClientId(subClientId), mType(type) {}

        LOICOLLECTION_A_NDAPI const NetworkIdentifier& getNetworkIdentifier() const;
        LOICOLLECTION_A_NDAPI const Packet& getPacket() const;
        LOICOLLECTION_A_NDAPI SubClientId getSubClientId() const;
        LOICOLLECTION_A_NDAPI NetworkPacketType getType() const;
    };

    class NetworkBroadcastPacketEvent final : public ll::event::Cancellable<ll::event::Event> {
    protected:
        const Packet& mPacket;
    
    public:
        constexpr explicit NetworkBroadcastPacketEvent(
            const Packet& packet
        ) : mPacket(packet) {}

        LOICOLLECTION_A_NDAPI const Packet& getPacket() const;
    };
}