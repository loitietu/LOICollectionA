#include <vector>
#include <string>

#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/Minecraft.h>
#include <mc/world/level/Level.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/PlayerScoreSetFunction.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

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

#include "McUtils.h"

namespace McUtils {
    void executeCommand(Player& player, std::string cmd) {
        if (cmd.empty())
            return;

        ll::string_utils::replaceAll(cmd, "${player}", std::string(player.mName));

        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        Command* command = ll::service::getMinecraft()->mCommands->compileCommand(
            HashedString(cmd), origin, (CurrentCmdVersion)CommandVersion::CurrentVersion(),
            [&](std::string const&) -> void {}
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    void clearItem(Player& player, const std::string& mTypeName, int mNumber) {
        auto& inventory = player.mInventory->mInventory;
        for (int i = 0; i < inventory->getContainerSize() && mNumber > 0; ++i) {
            const ItemStack& mItemObject = inventory->getItem(i);
            if ((mItemObject || !mItemObject.isNull()) && mItemObject.getTypeName() == mTypeName) {
                int mCount = std::min((int)mItemObject.mCount, mNumber);
                inventory->removeItem(i, mCount);
                mNumber -= mCount;
            }
        }
    }

    void giveItem(Player& player, ItemStack& item, int mNumber) {
        std::vector<ItemStack> mItemStacks{};
        for (int count; mNumber > 0; mNumber -= count)
            mItemStacks.emplace_back(item).mCount = (uchar)(count = std::min(mNumber, 64));
        Util::LootTableUtils::givePlayer(player, mItemStacks, true);
    }

    void broadcastText(const std::string& text, std::function<bool(Player&)> filter) {
        TextPacket packet = TextPacket::createSystemMessage(text);
        ll::service::getLevel()->forEachPlayer([&packet, filter = std::move(filter)](Player& player) -> bool {
            if (filter(player) && packet.isValid())
                packet.sendTo(player);
            return true;
        });
    }

    std::vector<Player*> getAllPlayers() {
        std::vector<Player*> mObjectLists{};
        ll::service::getLevel()->forEachPlayer([&mObjectLists](Player& player) -> bool {
            if (!player.isSimulatedPlayer())
                mObjectLists.emplace_back(&player);
            return true;
        });
        return mObjectLists;
    }

    bool isItemPlayerInventory(Player& player, const std::string& mTypeName, int mNumber) {
        if (mTypeName.empty() || mNumber <= 0)
            return false;
        
        auto& mItemInventory = player.mInventory->mInventory;
        for (int i = 0; i < mItemInventory->getContainerSize() && mNumber > 0; ++i) {
            const ItemStack& mItemObject = mItemInventory->getItem(i);
            if ((mItemObject || !mItemObject.isNull()) && mTypeName == mItemObject.getTypeName())
                mNumber -= (int)mItemObject.mCount;
        }
        return mNumber <= 0;
    }

    namespace scoreboard {
        int getScore(Player& player, const std::string& name) {
            optional_ref<Level> level = ll::service::getLevel();
            ScoreboardId identity = level->getScoreboard().getScoreboardId(player);
            if (identity.mRawID == ScoreboardId::INVALID().mRawID)
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
            if (identity.mRawID == ScoreboardId::INVALID().mRawID) 
                identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);
            
            modifyScore(identity, name, score, ScoreType::add);
        }

        void reduceScore(Player& player, const std::string &name, int score) {
            ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
            if (identity.mRawID == ScoreboardId::INVALID().mRawID)
                identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);

            modifyScore(identity, name, score, ScoreType::reduce);
        }
    }
}
