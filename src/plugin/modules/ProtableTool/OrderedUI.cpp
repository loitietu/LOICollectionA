#include <string>
#include <iterator>
#include <unordered_map>

#include <ll/api/memory/Hook.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerLeaveEvent.h>

#include <mc/world/actor/player/Player.h>

#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/PacketHandlerDispatcherInstance.h>
#include <mc/network/packet/ModalFormRequestPacket.h>
#include <mc/network/packet/ModalFormResponsePacket.h>

#include <mc/enums/SubClientId.h>

#include "include/ProtableTool/OrderedUI.h"

std::unordered_map<std::string, uint> mFormResponse;
std::unordered_map<std::string, std::unordered_map<uint, std::string>> mFormLists;

LL_TYPE_INSTANCE_HOOK(
    ModalFormRequestPacketHook,
    HookPriority::Highest,
    ModalFormRequestPacket,
    &ModalFormRequestPacket::sendTo,
    void,
    Player const& player
) {
    if (!this->mFormId || this->mFormJSON.empty()) return origin(player);

    std::string mUuid = player.getUuid().asString();
    if (mFormResponse.contains(mUuid)) {
        mFormLists[mUuid].insert({this->mFormId, this->mFormJSON});
        return;
    }
    mFormResponse[mUuid] = this->mFormId;
    return origin(player);
};

LL_TYPE_INSTANCE_HOOK(
    ModalFormResponsePacketHook,
    HookPriority::Highest,
    PacketHandlerDispatcherInstance<ModalFormResponsePacket>,
    &PacketHandlerDispatcherInstance<ModalFormResponsePacket>::handle,
    void,
    NetworkIdentifier const& identifier,
    NetEventCallback& callback,
    std::shared_ptr<Packet>& packet
) {
    if (auto player = ((ServerNetworkHandler&)callback).getServerPlayer(identifier, SubClientId::PrimaryClient); player) {
        auto& response = (ModalFormResponsePacket&)*packet;

        std::string mUuid = player->getUuid().asString();
        if (!mFormResponse.contains(mUuid))
            return origin(identifier, callback, packet);
        if (response.mFormId != mFormResponse[mUuid])
            return;
        mFormResponse.erase(mUuid);

        if (!mFormLists.contains(mUuid))
            return origin(identifier, callback, packet);
        if (mFormLists[mUuid].empty()) {
            mFormLists.erase(mUuid);
            return origin(identifier, callback, packet);
        }

        origin(identifier, callback, packet);
        auto it = std::prev(mFormLists[mUuid].end());
        ModalFormRequestPacket(it->first, it->second).sendTo(*player);
        mFormLists[mUuid].erase(it);
        return;
    }
    return origin(identifier, callback, packet);
};

namespace LOICollection::ProtableTool::OrderedUI {
    ll::event::ListenerPtr PlayerLeaveEventListener;

    void registery() {
        ModalFormRequestPacketHook::hook();
        ModalFormResponsePacketHook::hook();

        auto& eventBus = ll::event::EventBus::getInstance();
        PlayerLeaveEventListener = eventBus.emplaceListener<ll::event::PlayerLeaveEvent>(
            [](ll::event::PlayerLeaveEvent& event) {
                if (event.self().isSimulatedPlayer())
                    return;
                std::string mUuid = event.self().getUuid().asString();
                if (mFormResponse.contains(mUuid))
                    mFormResponse.erase(mUuid);
                if (mFormLists.contains(mUuid))
                    mFormLists.erase(mUuid);
            }
        );
    }

    void unregistery() {
        ModalFormRequestPacketHook::unhook();
        ModalFormResponsePacketHook::unhook();

        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerLeaveEventListener);
    }
}