#include <memory>
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

#include "include/RegistryHelper.h"

#include "base/Wrapper.h"
#include "base/ServiceProvider.h"

#include "ConfigPlugin.h"

#include "include/ProtableTool/OrderedUI.h"

namespace LOICollection::ProtableTool {
    struct OrderedUI::Impl {
        std::unordered_map<uint64, uint> mFormResponse;
        std::unordered_map<uint64, std::unordered_map<uint, std::string>> mFormLists;

        bool ModuleEnabled = false;

        ll::event::ListenerPtr PlayerDisconnectEventListener;
    };
}

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

        auto& instance = LOICollection::ProtableTool::OrderedUI::getInstance();

        auto& response = (ModalFormRequestPacket&)packet;
        if (!response.mFormId || response.mFormJSON.empty())
            return origin(identifier, packet, senderSubId);

        if (instance.mImpl->mFormResponse.contains(identifierHash)) {
            instance.mImpl->mFormLists[identifierHash].insert({response.mFormId, response.mFormJSON});
            return;
        }

        instance.mImpl->mFormResponse[identifierHash] = response.mFormId;
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

    auto& instance = LOICollection::ProtableTool::OrderedUI::getInstance();

    auto& response = (ModalFormResponsePacket&)*packet;
    if (!instance.mImpl->mFormResponse.contains(identifierHash))
        return origin(identifier, callback, packet);

    if (response.mFormId != instance.mImpl->mFormResponse[identifierHash])
        return;

    instance.mImpl->mFormResponse.erase(identifierHash);

    if (!instance.mImpl->mFormLists.contains(identifierHash))
        return origin(identifier, callback, packet);

    if (instance.mImpl->mFormLists[identifierHash].empty()) {
        instance.mImpl->mFormLists.erase(identifierHash);
        return origin(identifier, callback, packet);
    }

    origin(identifier, callback, packet);
    
    auto it = std::prev(instance.mImpl->mFormLists[identifierHash].end());
    ModalFormRequestPacket(it->first, it->second).sendToClient(identifier, packet->mSenderSubId);
    instance.mImpl->mFormLists[identifierHash].erase(it);
};

namespace LOICollection::ProtableTool {
    OrderedUI::OrderedUI() : mImpl(std::make_unique<Impl>()) {};
    OrderedUI::~OrderedUI() = default;

    bool OrderedUI::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().ProtableTool.OrderedUI)
            return false;

        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool OrderedUI::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        ModalFormRequestPacketHook::hook();
        ModalFormResponsePacketHook::hook();

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& event) mutable -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                uint64 identifierHash = event.self().getNetworkIdentifier().getHash();
                if (this->mImpl->mFormResponse.contains(identifierHash))
                    this->mImpl->mFormResponse.erase(identifierHash);
                if (this->mImpl->mFormLists.contains(identifierHash))
                    this->mImpl->mFormLists.erase(identifierHash);
            }
        );

        return true;
    }

    bool OrderedUI::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        ModalFormRequestPacketHook::unhook();
        ModalFormResponsePacketHook::unhook();

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerDisconnectEventListener);

        return true;
    }
}

REGISTRY_HELPER("OrderedUI", LOICollection::ProtableTool::OrderedUI, LOICollection::ProtableTool::OrderedUI::getInstance())
