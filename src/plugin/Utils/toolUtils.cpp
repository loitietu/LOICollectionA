#include <ctime>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>

#include <ll/api/Mod/Manifest.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/reflection/Serialization.h>
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
#include <mc/enums/BuildPlatform.h>

#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/ServerCommandOrigin.h>
#include <mc/server/commands/MinecraftCommands.h>
#include <mc/network/ConnectionRequest.h>
#include <mc/_HeaderOutputPredefine.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Include/languagePlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/JsonUtils.h"

#include "LOICollectionA.h"
#include "ConfigPlugin.h"

#include "toolUtils.h"

namespace toolUtils {
    namespace Mc {
        void executeCommand(Player* player, std::string cmd) {
            ll::string_utils::replaceAll(cmd, "${player}", player->getRealName());
            auto origin = ServerCommandOrigin(
                "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player->getDimensionId()
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

        void clearItem(Player* player, void* itemStack_ptr) {
            ItemStack* itemStack = static_cast<ItemStack*>(itemStack_ptr);
            if (!itemStack || !player)
                return;
            int mItemStackCount = itemStack->mCount;
            Container& mItemInventory = player->getInventory();
            for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
                auto& mItemObject = mItemInventory.getItem(i);
                if (mItemObject.isValid()) {
                    if (itemStack->getTypeName() == mItemObject.getTypeName()) {
                        if (mItemObject.mCount >= mItemStackCount) {
                            mItemInventory.removeItem(i, mItemStackCount);
                            return;
                        } else {
                            mItemStackCount -= mItemObject.mCount;
                            mItemInventory.removeItem(i, mItemObject.mCount);
                        }
                    }
                }
            }
        }

        void broadcastText(const std::string& text) {
            ll::service::getLevel()->forEachPlayer([text](Player& player) {
                player.sendMessage(text);
                return true;
            });
        }

        std::string getDevice(Player* player) {
            if (!player->isSimulatedPlayer()) {
                auto request = player->getConnectionRequest();
                if (request) {
                    switch (request->getDeviceOS()) {
                        case BuildPlatform::Android:
                            return "android";
                        case BuildPlatform::iOS:
                            return "ios";
                        case BuildPlatform::Amazon:
                            return "amazon";
                        case BuildPlatform::Linux:
                            return "linux";
                        case BuildPlatform::Xbox:
                            return "xbox";
                        case BuildPlatform::Dedicated:
                            return "dedicated";
                        case BuildPlatform::GearVR:
                            return "gearvr";
                        case BuildPlatform::Nx:
                            return "nx";
                        case BuildPlatform::OSX:
                            return "osx";
                        case BuildPlatform::PS4:
                            return "ps4";
                        case BuildPlatform::WindowsPhone:
                            return "windowsphone";
                        case BuildPlatform::Win32:
                            return "win32";
                        case BuildPlatform::UWP:
                            return "uwp";
                        default:
                            return "unknown";
                    }
                }
            }
            return "unknown";
        }

        std::vector<Player*> getAllPlayers() {
            std::vector<Player*> mObjectLists = {};
            ll::service::getLevel()->forEachPlayer([&](Player& player) -> bool {
                if (!player.isSimulatedPlayer())
                    mObjectLists.push_back(&player);
                return true;
            });
            return mObjectLists;
        }

        std::vector<std::string> getAllPlayerName() {
            std::vector<std::string> mObjectLists = {};
            for (auto& player : getAllPlayers())
                mObjectLists.push_back(player->getRealName());
            return mObjectLists;
        }

        Player* getPlayerFromName(const std::string& name) {
            for (auto& player : getAllPlayers()) {
                if (player->getRealName() == name)
                    return player;
            }
            return nullptr;
        }
        
        bool isItemPlayerInventory(Player* player, void* itemStack_ptr) {
            ItemStack* itemStack = static_cast<ItemStack*>(itemStack_ptr);
            if (!itemStack || !player)
                return false;
            int mItemStackCount = itemStack->mCount;
            Container& mItemInventory = player->getInventory();
            for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
                auto& mItemObject = mItemInventory.getItem(i);
                if (mItemObject.isValid()) {
                    if (itemStack->getTypeName() == mItemObject.getTypeName()) {
                        if (mItemStackCount <= mItemObject.mCount) {
                            return true;
                        } else mItemStackCount -= mItemObject.mCount;
                    }
                }
            }
            return false;
        }
    }

    namespace System {
        std::string getNowTime() {
            std::tm currentTimeInfo;
            std::time_t currentTime = std::time(nullptr);
            localtime_s(&currentTimeInfo, &currentTime);
            std::ostringstream oss;
            oss << std::put_time(&currentTimeInfo, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        }

        std::string timeCalculate(const std::string& timeString, int hours) {
            if (!hours)
                return "0";
            std::tm tm = {};
            std::istringstream iss(timeString);
            iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            tm.tm_hour += hours;
            std::mktime(&tm);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y%m%d%H%M%S");
            return oss.str();
        }

        std::string formatDataTime(const std::string& timeString) {
            if (timeString.size() != 14) 
                return "None";
            std::string fomatted;
            fomatted += timeString.substr(0, 4) + "-";
            fomatted += timeString.substr(4, 2) + "-";
            fomatted += timeString.substr(6, 2) + " ";
            fomatted += timeString.substr(8, 2) + ":";
            fomatted += timeString.substr(10, 2) + ":";
            fomatted += timeString.substr(12, 2);
            return fomatted;
        }

        int toInt(const std::string& intString, int defaultValue) {
            try {
                return std::stoi(intString);
            } catch (...) {
                return defaultValue;
            }
        }

        int64_t toInt64t(const std::string& intString, int defaultValue) {
            try {
                return std::stoll(intString);
            } catch (...) {
                return defaultValue;
            }
        }

        bool isReach(const std::string& timeString) {
            if (timeString == "0") 
                return false;
            std::string formattedTime = getNowTime();
            ll::string_utils::replaceAll(formattedTime, "-", "");
            ll::string_utils::replaceAll(formattedTime, ":", "");
            ll::string_utils::replaceAll(formattedTime, " ", "");
            return std::stoll(formattedTime) > std::stoll(timeString);
        }
    }

    namespace Gui {
        using I18nUtils::tr;
        using LOICollection::Plugins::language::getLanguage;

        void submission(Player* player, std::function<void(Player*)> callback) {
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "exit.gui.title"), tr(mObjectLanguage, "exit.gui.label"));
            form.appendButton(tr(mObjectLanguage, "exit.gui.button1"), [callback](Player& pl) {
                callback(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "exit.gui.button2"), [](Player& pl) {
                pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }
    }

    namespace scoreboard {
        int getScore(Player* player, const std::string& name) {
            auto level = ll::service::getLevel();
            auto identity(level->getScoreboard().getScoreboardId(*player));
            if (!identity.isValid()) {
                return 0;
            }
            auto obj(level->getScoreboard().getObjective(name));
            if (!obj) {
                return 0;
            }

            auto scores(level->getScoreboard().getIdScores(identity));
            for (auto& score : scores) {
                if (score.mObjective == obj) {
                    return score.mScore;
                }
            }
            return 0;
        }

        void modifyScore(Player* player, const std::string& name, int score, PlayerScoreSetFunction action) {
            auto level = ll::service::getLevel();
            auto identity(level->getScoreboard().getScoreboardId(*player));
            if (!identity.isValid()) {
                identity = level->getScoreboard().createScoreboardId(*player);
            }
            auto obj(level->getScoreboard().getObjective(name));
            if (!obj) {
                obj = static_cast<Objective*>(addObjective(name, name));
            }

            bool succes;
            level->getScoreboard().modifyPlayerScore(succes, identity, *obj, score, action);
        }

        void addScore(Player* player, const std::string &name, int score) {
            modifyScore(player, name, score, PlayerScoreSetFunction::Add);
        }

        void reduceScore(Player* player, const std::string &name, int score) {
            int mObjectScore = getScore(player, name);
            modifyScore(player, name, (mObjectScore - score), PlayerScoreSetFunction::Set);
        }

        void* addObjective(const std::string& name, const std::string& displayName) {
            return ll::service::getLevel()->getScoreboard().addObjective(name, displayName, *ll::service::getLevel()->getScoreboard().getCriteria("dummy"));
        }
    }

    namespace Config {
        std::string getVersion() {
            return LOICollection::A::getInstance().getSelf().getManifest().version->to_string();
        }

        void mergeJson(nlohmann::ordered_json& source, nlohmann::ordered_json& target) {
            for (auto it = source.begin(); it != source.end(); ++it) {
                if (!target.contains(it.key())) {
                    target[it.key()] = it.value();
                    continue;
                }
                if (it.value().is_object() && target[it.key()].is_object()) {
                    mergeJson(it.value(), target[it.key()]);
                } else if (it->type() != target[it.key()].type()) {
                    target[it.key()] = it.value();
                }
            }
        }

        void SynchronousPluginConfigVersion(void* config_ptr) {
            C_Config* config = static_cast<C_Config*>(config_ptr);

            std::stringstream ss;
            auto version = ll::string_utils::splitByPattern(getVersion(), ".");
            for (size_t i = 0; i < version.size(); i++) {
                ss << version[i];
            }
            config->version = std::stoi(ss.str());
            ss.clear();
        }

        void SynchronousPluginConfigType(void* config_ptr, const std::filesystem::path& path) {
            JsonUtils mConfigObject(path);
            auto patch = ll::reflection::serialize<nlohmann::ordered_json>(*static_cast<C_Config*>(config_ptr));
            nlohmann::ordered_json mPatchJson = nlohmann::ordered_json::parse(patch->dump());
            nlohmann::ordered_json mConfigJson = mConfigObject.toJson();
            mergeJson(mPatchJson, mConfigJson);
            mConfigObject.write(mConfigJson);
            mConfigObject.save();
        }
    }
}
