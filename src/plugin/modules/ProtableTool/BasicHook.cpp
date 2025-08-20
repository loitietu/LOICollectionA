#include <string>

#include <ll/api/memory/Hook.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>

#include <mc/network/packet/StartGamePacket.h>

#include <mc/deps/core/utility/BinaryStream.h>

#include "include/ProtableTool/BasicHook.h"

std::string mFakeSeed;

LL_TYPE_INSTANCE_HOOK(
    ServerStartGamePacketHook,
    HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    if (!mFakeSeed.empty()) {
        const char* ptr = mFakeSeed.data();
        char* endpt{};
        int64 result = std::strtoll(ptr, &endpt, 10);
        this->mSettings->mSeed = LevelSeed64(ptr == endpt ? ll::random_utils::rand<int64_t>() : result);
    }
    return origin(stream);
};

namespace LOICollection::ProtableTool::BasicHook {
    void registery(const std::string& fakeSeed) {
        mFakeSeed = fakeSeed;

        ServerStartGamePacketHook::hook();
    }

    void unregistery() {
        ServerStartGamePacketHook::unhook();
    }
}

// #include <mc/network/NetworkSystem.h>
// #include <mc/network/MinecraftPacketIds.h>
// #include <mc/network/packet/AvailableCommandsPacket.h>

// #include <mc/common/SubClientId.h>

// LL_AUTO_TYPE_INSTANCE_HOOK(
//     AvailableCommandsPacketHook,
//     HookPriority::Highest,
//     NetworkSystem,
//     &NetworkSystem::send,
//     void,
//     NetworkIdentifier const& identifier,
//     Packet const& packet,
//     SubClientId senderSubId
// ) {
//     if (packet.getId() == MinecraftPacketIds::AvailableCommands) {
//         auto& acp = (AvailableCommandsPacket&)packet;
        
//         std::vector<size_t> toRemove;
//         for (size_t i = 0; i < acp.mCommands->size(); ++i) {
//             auto& cmd = acp.mCommands->at(i);
//             if (cmd.name.get() == "chat") {
//                 for (auto& overload : cmd.overloads.get()) {
//                     if (!overload.params.get().empty() && overload.params.get()[0].name.get() == "add") {
//                         toRemove.push_back(i);
//                         break; 
//                     }
//                 }
//             }
//         }
//         std::sort(toRemove.rbegin(), toRemove.rend());
//         for (size_t i : toRemove) {
//             if (i < acp.mCommands->size()) {
//                 acp.mCommands->at(i).$dtor();
//                 if (i < acp.mCommands->size() - 1) {
//                     new (&acp.mCommands->at(i)) AvailableCommandsPacket::CommandData(std::move(acp.mCommands->back()));
//                     acp.mCommands->back().$dtor();
//                 }
//                 acp.mCommands->pop_back();
//             }
//         }
//     }
//     origin(identifier, packet, senderSubId);
// };