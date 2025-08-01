#include <string>
#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/network/ConnectionRequest.h>
#include <mc/network/NetworkIdentifier.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/packet/LoginPacket.h>

#include <mc/certificates/Certificate.h>
#include <mc/certificates/identity/LegacyMultiplayerToken.h>

#include <mc/platform/UUID.h>

#include "include/ServerEvents/LoginPacketEvent.h"

namespace LOICollection::ServerEvents {
    const NetworkIdentifier& LoginPacketEvent::getNetworkIdentifier() const {
        return this->mNetworkIdentifier;
    }

    const ConnectionRequest& LoginPacketEvent::getConnectionRequest() const {
        return this->mConnectionRequest;
    }

    const mce::UUID& LoginPacketEvent::getUUID() const {
        return this->mUUID;
    }

    std::string LoginPacketEvent::getIpAndPort() const {
        return this->mIpAndPort;
    }

    std::string LoginPacketEvent::getIp() const {
        std::string ipAndport = getIpAndPort();

        size_t pos = ipAndport.find(':');
        return ipAndport.substr(0, pos);
    }

    std::string LoginPacketEvent::getPort() const {
        std::string ipAndport = getIpAndPort();
        
        size_t pos = ipAndport.find(':');
        return ipAndport.substr(pos + 1);
    }

    LL_TYPE_INSTANCE_HOOK(
        LoginPacketEventHook,
        HookPriority::Normal,
        ServerNetworkHandler,
        &ServerNetworkHandler::$handle,
        void,
        NetworkIdentifier const& identifier,
        std::shared_ptr<::LoginPacket> packet
    ) {
        origin(identifier, packet);

        std::string ipAndPort = identifier.getIPAndPort();
        ConnectionRequest& conn = *packet->mConnectionRequest.get();
        mce::UUID uuid = conn.mLegacyMultiplayerToken->getIdentity();

        LoginPacketEvent event(identifier, conn, uuid, ipAndPort);
        ll::event::EventBus::getInstance().publish(event);
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class LoginPacketEventEmitter : public ll::event::Emitter<emitterFactory, LoginPacketEvent> {
        ll::memory::HookRegistrar<LoginPacketEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<LoginPacketEventEmitter>();
    }
}
