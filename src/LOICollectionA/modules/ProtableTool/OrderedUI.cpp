#include <memory>
#include <string>
#include <unordered_map>

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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/ServerEvents/NetworkPacketEvent.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/ProtableTool/OrderedUI.h"

namespace LOICollection::ProtableTool {
    struct OrderedUI::Impl {
        std::unordered_map<uint64, uint> mFormResponse;
        std::unordered_map<uint64, std::unordered_map<uint, std::string>> mFormLists;

        bool ModuleEnabled = false;

        ll::event::ListenerPtr ModalFormRequestEventListener;
        ll::event::ListenerPtr ModalFormResponseEventListener;
        ll::event::ListenerPtr PlayerDisconnectEventListener;
    };

    OrderedUI::OrderedUI() : mImpl(std::make_unique<Impl>()) {};
    OrderedUI::~OrderedUI() = default;

    OrderedUI& OrderedUI::getInstance() {
        static OrderedUI instance;
        return instance;
    }

    void OrderedUI::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->ModalFormRequestEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketEvent>([this](LOICollection::ServerEvents::NetworkPacketEvent& event) mutable -> void {
            if (event.getPacket().getId() != MinecraftPacketIds::ShowModalForm)
                return;

            auto& packet = static_cast<ModalFormRequestPacket&>(const_cast<Packet&>(event.getPacket()));

            uint64 mIdentifierHash = event.getNetworkIdentifier().getHash();
            if (!packet.mFormId || packet.mFormJSON.empty())
                return;
            if (this->mImpl->mFormResponse.contains(mIdentifierHash)) {
                this->mImpl->mFormLists[mIdentifierHash].insert({ packet.mFormId, packet.mFormJSON });

                event.cancel();

                return;
            }

            this->mImpl->mFormResponse[mIdentifierHash] = packet.mFormId;
        });
        this->mImpl->ModalFormResponseEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketEvent>([this](LOICollection::ServerEvents::NetworkPacketEvent& event) mutable -> void {
            if (event.getPacket().getId() != MinecraftPacketIds::ModalFormResponse)
                return;

            auto& packet = static_cast<ModalFormResponsePacket&>(const_cast<Packet&>(event.getPacket()));

            uint64 mIdentifierHash = event.getNetworkIdentifier().getHash();
            if (!this->mImpl->mFormResponse.contains(mIdentifierHash))
                return;
            if (packet.mFormId != this->mImpl->mFormResponse[mIdentifierHash]) {
                event.cancel();

                return;
            }

            this->mImpl->mFormResponse.erase(mIdentifierHash);

            if (!this->mImpl->mFormLists.contains(mIdentifierHash))
                return;
            if (this->mImpl->mFormLists[mIdentifierHash].empty()) {
                this->mImpl->mFormLists.erase(mIdentifierHash);

                return;
            }
            
            auto it = std::prev(this->mImpl->mFormLists[mIdentifierHash].end());
            ModalFormRequestPacket(it->first, it->second).sendToClient(event.getNetworkIdentifier(), event.getSubClientId());
            this->mImpl->mFormLists[mIdentifierHash].erase(it);
        });
        this->mImpl->PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            uint64 mIdentifierHash = event.self().getNetworkIdentifier().getHash();
            if (this->mImpl->mFormResponse.contains(mIdentifierHash))
                this->mImpl->mFormResponse.erase(mIdentifierHash);
            if (this->mImpl->mFormLists.contains(mIdentifierHash))
                this->mImpl->mFormLists.erase(mIdentifierHash);
        });
    }

    void OrderedUI::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->ModalFormRequestEventListener);
        eventBus.removeListener(this->mImpl->ModalFormResponseEventListener);
        eventBus.removeListener(this->mImpl->PlayerDisconnectEventListener);
    }

    bool OrderedUI::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().ProtableTool.OrderedUI)
            return false;

        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool OrderedUI::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->ModuleEnabled = false;

        return true;
    }

    bool OrderedUI::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->listenEvent();

        return true;
    }

    bool OrderedUI::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        return true;
    }
}

REGISTRY_HELPER("OrderedUI", LOICollection::ProtableTool::OrderedUI, LOICollection::ProtableTool::OrderedUI::getInstance())
