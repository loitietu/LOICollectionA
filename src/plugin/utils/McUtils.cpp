#include <vector>
#include <string>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/RandomUtils.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/Minecraft.h>
#include <mc/world/Container.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/PlayerScoreSetFunction.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/util/LootTableUtils.h>

#include <mc/server/commands/Command.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CurrentCmdVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/ServerCommandOrigin.h>
#include <mc/server/commands/MinecraftCommands.h>

#include <mc/network/packet/TextPacket.h>
#include <mc/network/packet/TextPacketType.h>
#include <mc/network/packet/StartGamePacket.h>

#include <mc/deps/core/utility/BinaryStream.h>

#include "McUtils.h"

namespace McUtils {
    void executeCommand(std::string cmd, int dimension) {
        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, dimension
        );
        Command* command = ll::service::getMinecraft()->getCommands().compileCommand(
            HashedString(cmd), origin, (CurrentCmdVersion)CommandVersion::CurrentVersion(),
            [&](std::string const&) -> void {}
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    void executeCommand(Player& player, std::string cmd) {
        executeCommand(
            ll::string_utils::replaceAll(cmd, "${player}", player.getRealName()),
            player.getDimensionId()
        );
    }

    void clearItem(Player& player, std::string mTypeName, int mNumber) {
        Container& mItemInventory = player.getInventory();
        for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
            const ItemStack& mItemObject = mItemInventory.getItem(i);
            if ((mItemObject || !mItemObject.isNull()) && mTypeName == mItemObject.getTypeName()) {
                if (mItemObject.mCount >= mNumber) {
                    mItemInventory.removeItem(i, mNumber);
                    return;
                }
                mNumber -= mItemObject.mCount;
                mItemInventory.removeItem(i, mItemObject.mCount);
            }
        }
    }

    void giveItem(Player& player, ItemStack& item, int mNumber) {
        std::vector<ItemStack> items;
        while (mNumber > 0) {
            int count = std::min(mNumber, 64);
            item.mCount = (uchar)count;
            items.push_back(item);
            mNumber -= count;
        }
        Util::LootTableUtils::givePlayer(player, items, true);
    }

    void broadcastText(const std::string& text, std::function<bool(Player&)> filter) {
        TextPacket packet = TextPacket::createSystemMessage(text);
        ll::service::getLevel()->forEachPlayer([packet, filter = std::move(filter)](Player& player) -> bool {
            if (filter(player) && packet.isValid())
                packet.sendTo(player);
            return true;
        });
    }

    std::vector<Player*> getAllPlayers() {
        std::vector<Player*> mObjectLists{};
        ll::service::getLevel()->forEachPlayer([&](Player& player) -> bool {
            if (!player.isSimulatedPlayer())
                mObjectLists.push_back(&player);
            return true;
        });
        return mObjectLists;
    }

    bool isItemPlayerInventory(Player& player, std::string mTypeName, int mNumber) {
        Container& mItemInventory = player.getInventory();
        for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
            const ItemStack& mItemObject = mItemInventory.getItem(i);
            if ((mItemObject || !mItemObject.isNull()) && mTypeName == mItemObject.getTypeName()) {
                if (mNumber <= mItemObject.mCount)
                    return true;
                mNumber -= mItemObject.mCount;
            }
        }
        return false;
    }

    namespace manager {
        int64 mFakeSeed = 0;

        LL_AUTO_TYPE_INSTANCE_HOOK(
            ServerStartGamePacketHook,
            HookPriority::Normal,
            StartGamePacket,
            &StartGamePacket::$write,
            void,
            BinaryStream& stream
        ) {
            if (mFakeSeed) this->mSettings->setRandomSeed(LevelSeed64(mFakeSeed));
            return origin(stream);
        };

        void setFakeSeed(const std::string& str) {
            const char* ptr = str.data();
            char* endpt{};
            int64 result = std::strtoll(ptr, &endpt, 10);
            mFakeSeed = ptr == endpt ? ll::random_utils::rand<int64_t>() : result;
        }
    }

    namespace scoreboard {
        int getScore(Player& player, const std::string& name) {
            optional_ref<Level> level = ll::service::getLevel();
            ScoreboardId identity = level->getScoreboard().getScoreboardId(player);
            if (!identity.isValid())
                return 0;
            Objective* obj = level->getScoreboard().getObjective(name);
            return !obj ? 0 : obj->getPlayerScore(identity).mValue;
        }

        void modifyScore(ScoreboardId& identity, const std::string& name, int score, ScoreType action) {
            optional_ref<Level> level = ll::service::getLevel();
            Objective* obj = level->getScoreboard().getObjective(name);
            
            if (!obj) {
                obj = static_cast<Objective*>(level->getScoreboard().addObjective(
                    name, name, *level->getScoreboard().getCriteria("dummy")
                ));
            }

            bool succes;
            level->getScoreboard().modifyPlayerScore(succes, identity, *obj, score, 
                action == ScoreType::set ? PlayerScoreSetFunction::Set : 
                action == ScoreType::add ? PlayerScoreSetFunction::Add : 
                PlayerScoreSetFunction::Subtract
            );
        }

        void addScore(Player& player, const std::string &name, int score) {
            ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
            if (!identity.isValid()) 
                identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);
            
            modifyScore(identity, name, score, ScoreType::add);
        }

        void reduceScore(Player& player, const std::string &name, int score) {
            ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
            if (!identity.isValid())
                identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);

            modifyScore(identity, name, score, ScoreType::reduce);
        }
    }
}
