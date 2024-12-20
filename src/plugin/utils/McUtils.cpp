#include <vector>
#include <string>

#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/Minecraft.h>
#include <mc/world/Container.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/Command.h>

#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerScoreSetFunction.h>

#include <mc/world/item/registry/ItemStack.h>

#include <mc/enums/CurrentCmdVersion.h>

#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/ServerCommandOrigin.h>
#include <mc/server/commands/MinecraftCommands.h>
#include <mc/_HeaderOutputPredefine.h>

#include "include/languagePlugin.h"

#include "utils/I18nUtils.h"

#include "McUtils.h"

namespace McUtils {
    void executeCommand(Player& player, std::string cmd) {
        ll::string_utils::replaceAll(cmd, "${player}", player.getRealName());
        auto origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        auto command = ll::service::getMinecraft()->getCommands().compileCommand(
            std::string(cmd), origin, (CurrentCmdVersion)CommandVersion::CurrentVersion,
            [&](std::string const& /*unused*/) {}
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    void clearItem(Player& player, std::string mTypeName, int mNumber) {
        Container& mItemInventory = player.getInventory();
        for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
            auto& mItemObject = mItemInventory.getItem(i);
            if (mItemObject.isValid() && mTypeName == mItemObject.getTypeName()) {
                if (mItemObject.mCount >= mNumber) {
                    mItemInventory.removeItem(i, mNumber);
                    return;
                }
                mNumber -= mItemObject.mCount;
                mItemInventory.removeItem(i, mItemObject.mCount);
            }
        }
    }

    void broadcastText(const std::string& text) {
        ll::service::getLevel()->forEachPlayer([text](Player& player) {
            player.sendMessage(text);
            return true;
        });
    }

    std::vector<Player*> getAllPlayers() {
        std::vector<Player*> mObjectLists{};
        ll::service::getLevel()->forEachPlayer([&](Player& player) {
            if (!player.isSimulatedPlayer())
                mObjectLists.push_back(&player);
            return true;
        });
        return mObjectLists;
    }

    bool isItemPlayerInventory(Player& player, std::string mTypeName, int mNumber) {
        Container& mItemInventory = player.getInventory();
        for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
            auto& mItemObject = mItemInventory.getItem(i);
            if (mItemObject.isValid() && mTypeName == mItemObject.getTypeName()) {
                if (mNumber <= mItemObject.mCount)
                    return true;
                mNumber -= mItemObject.mCount;
            }
        }
        return false;
    }

    namespace Gui {
        using I18nUtilsTools::tr;
        using LOICollection::Plugins::language::getLanguage;

        void submission(Player& player, std::function<void(Player&)> callback) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "exit.gui.title"), tr(mObjectLanguage, "exit.gui.label"));
            form.appendButton(tr(mObjectLanguage, "exit.gui.button1"), [callback](Player& pl) {
                callback(pl);
            });
            form.appendButton(tr(mObjectLanguage, "exit.gui.button2"), [](Player& pl) {
                pl.sendMessage(tr(getLanguage(pl), "exit"));
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(pl), "exit"));
            });
        }
    }

    namespace scoreboard {
        int getScore(Player& player, const std::string& name) {
            auto level = ll::service::getLevel();
            auto identity = level->getScoreboard().getScoreboardId(player);
            if (!identity.isValid())
                return 0;
            auto obj = level->getScoreboard().getObjective(name);
            return !obj ? 0 : obj->getPlayerScore(identity).mScore;
        }

        void modifyScore(ScoreboardId& identity, const std::string& name, int score, int action) {
            auto level = ll::service::getLevel();
            auto obj = level->getScoreboard().getObjective(name);
            
            if (!obj) {
                obj = static_cast<Objective*>(level->getScoreboard().addObjective(
                    name, name, *level->getScoreboard().getCriteria("dummy")
                ));
            }

            bool succes;
            level->getScoreboard().modifyPlayerScore(succes, identity, *obj, score, 
                action == 0x0 ? PlayerScoreSetFunction::Set : action == 0x1 ? PlayerScoreSetFunction::Add : PlayerScoreSetFunction::Subtract
            );
        }

        void addScore(Player& player, const std::string &name, int score) {
            ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
            if (!identity.isValid()) 
                identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);
            
            modifyScore(identity, name, score, 0x1);
        }

        void reduceScore(Player& player, const std::string &name, int score) {
            ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
            if (!identity.isValid())
                identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);

            modifyScore(identity, name, score, 0x2);
        }
    }
}
