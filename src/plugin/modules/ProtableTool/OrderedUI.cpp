#include <string>
#include <unordered_map>

#include <ll/api/memory/Hook.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>

#include <mc/server/ServerPlayer.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/NetworkSystem.h>
#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/PacketHandlerDispatcherInstance.h>
#include <mc/network/packet/ModalFormRequestPacket.h>
#include <mc/network/packet/ModalFormResponsePacket.h>

#include <mc/common/SubClientId.h>

#include "include/ProtableTool/OrderedUI.h"

std::unordered_map<uint64, uint> mFormResponse;
std::unordered_map<uint64, std::unordered_map<uint, std::string>> mFormLists;

LL_TYPE_INSTANCE_HOOK(
    ModalFormRequestPacketHook,
    HookPriority::Highest,
    NetworkSystem,
    &NetworkSystem::send,
    void,
    NetworkIdentifier const& identifier,
    Packet const& packet,
    SubClientId senderSubId
) {
    if (packet.getId() == MinecraftPacketIds::ShowModalForm) {
        uint64 identifierHash = identifier.getHash();

        auto& response = (ModalFormRequestPacket&)packet;
        if (!response.mFormId || response.mFormJSON.empty())
            return origin(identifier, packet, senderSubId);
        if (mFormResponse.contains(identifierHash)) {
            mFormLists[identifierHash].insert({response.mFormId, response.mFormJSON});
            return;
        }
        mFormResponse[identifierHash] = response.mFormId;
    }
    origin(identifier, packet, senderSubId);
};

LL_TYPE_INSTANCE_HOOK(
    ModalFormResponsePacketHook,
    HookPriority::Highest,
    PacketHandlerDispatcherInstance<ModalFormResponsePacket>,
    &PacketHandlerDispatcherInstance<ModalFormResponsePacket>::$handle,
    void,
    NetworkIdentifier const& identifier,
    NetEventCallback& callback,
    std::shared_ptr<Packet>& packet
) {
    uint64 identifierHash = identifier.getHash();

    auto& response = (ModalFormResponsePacket&)*packet;
    if (!mFormResponse.contains(identifierHash))
        return origin(identifier, callback, packet);
    if (response.mFormId != mFormResponse[identifierHash])
        return;
    mFormResponse.erase(identifierHash);

    if (!mFormLists.contains(identifierHash))
        return origin(identifier, callback, packet);
    if (mFormLists[identifierHash].empty()) {
        mFormLists.erase(identifierHash);
        return origin(identifier, callback, packet);
    }

    origin(identifier, callback, packet);
    auto it = std::prev(mFormLists[identifierHash].end());
    ModalFormRequestPacket(it->first, it->second).sendToClient(identifier, packet->mSenderSubId);
    mFormLists[identifierHash].erase(it);
};

namespace LOICollection::ProtableTool::OrderedUI {
    ll::event::ListenerPtr PlayerDisconnectEventListener;

    void registery() {
        ModalFormRequestPacketHook::hook();
        ModalFormResponsePacketHook::hook();

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
            [](ll::event::PlayerDisconnectEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;
                uint64 identifierHash = event.self().getNetworkIdentifier().getHash();
                if (mFormResponse.contains(identifierHash))
                    mFormResponse.erase(identifierHash);
                if (mFormLists.contains(identifierHash))
                    mFormLists.erase(identifierHash);
            }
        );
    }

    void unregistery() {
        ModalFormRequestPacketHook::unhook();
        ModalFormResponsePacketHook::unhook();

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerDisconnectEventListener);
    }
}