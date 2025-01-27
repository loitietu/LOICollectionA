#pragma once

#include <string>

#include <ll/api/event/Event.h>

#include "base/Macro.h"

class NetworkIdentifier;
class ConnectionRequest;

namespace mce {
    class UUID;
}

namespace LOICollection::ServerEvents {
    class LoginPacketEvent final : public ll::event::Event {
    protected:
        const NetworkIdentifier& mNetworkIdentifier;
        const ConnectionRequest& mConnectionRequest;
        const mce::UUID& mUUID;
        const std::string mIpAndPort;
    
    public:
        constexpr explicit LoginPacketEvent(
            const NetworkIdentifier& networkIdentifier,
            const ConnectionRequest& connectionRequest,
            const mce::UUID& uuid,
            const std::string& ipAndPort
        ) : mNetworkIdentifier(networkIdentifier), mConnectionRequest(connectionRequest) , mUUID(uuid), mIpAndPort(ipAndPort) {}

        LOICOLLECTION_A_NDAPI const NetworkIdentifier& getNetworkIdentifier() const;
        LOICOLLECTION_A_NDAPI const ConnectionRequest& getConnectionRequest() const;
        LOICOLLECTION_A_NDAPI const mce::UUID& getUUID() const;

        LOICOLLECTION_A_NDAPI std::string getIpAndPort() const;
        LOICOLLECTION_A_NDAPI std::string getIp() const;
        LOICOLLECTION_A_NDAPI std::string getPort() const;
    };
}