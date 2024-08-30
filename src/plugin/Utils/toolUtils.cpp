#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <cstddef>

#include <ll/api/Mod/Manifest.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/Minecraft.h>
#include <mc/world/Container.h>
#include <mc/world/level/Level.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerScoreSetFunction.h>
#include <mc/world/item/registry/ItemStack.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/PlayerCommandOrigin.h>
#include <mc/server/commands/MinecraftCommands.h>
#include <mc/network/ConnectionRequest.h>
#include <mc/enums/BuildPlatform.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "../ConfigPlugin.h"

#include "toolUtils.h"

namespace toolUtils {
    namespace {
        ll::mod::Manifest manifestPlugin;
    }

    void init(void* mSelfPtr) { manifestPlugin = static_cast<ll::mod::NativeMod*>(mSelfPtr)->getManifest(); }

    void SynchronousPluginConfigVersion(void* config_ptr) {
        C_Config* config = static_cast<C_Config*>(config_ptr);

        std::stringstream ss;
        std::vector<std::string> version = split(getVersion(), '.');
        for (size_t i = 0; i < version.size(); i++) {
            ss << version[i];
        }
        config->version = std::stoi(ss.str());
        ss.clear();
    }

    void executeCommand(Player* player, const std::string& command) {
        CommandContext context = CommandContext(command, std::make_unique<PlayerCommandOrigin>(PlayerCommandOrigin(*player)));
        ll::service::getMinecraft()->getCommands().executeCommand(context);
    }

    void clearItem(Player* player, void* itemStack_ptr) {
        ItemStack* itemStack = static_cast<ItemStack*>(itemStack_ptr);
        Container& mItemInventory = player->getInventory();
        for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
            auto& mItemObject = mItemInventory.getItem(i);
            if (mItemObject.isValid()) {
                if (itemStack->getTypeName() == mItemObject.getTypeName()) {
                    if (mItemObject.mCount > itemStack->mCount) {
                        mItemInventory.removeItem(i, itemStack->mCount);
                        return;
                    } else mItemInventory.removeItem(i, mItemObject.mCount);
                }
            }
        }
    }

    std::string getVersion() {
        return manifestPlugin.version->to_string();
    }

    std::string getDevice(Player* player) {
        if (!player->isSimulated()) {
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

    std::string replaceString(std::string str, const std::string& from, const std::string& to) {
        for (std::string::size_type pos(0); pos != std::string::npos; pos += to.length()) {
            if ((pos = str.find(from, pos)) != std::string::npos) {
                str.replace(pos, from.length(), to);
            } else break;
        }
        return str;
    }

    std::string timeCalculate(int hours) {
        if (hours > 0) {
            std::time_t currentTime = std::time(nullptr);
            std::tm timeInfo;
            localtime_s(&timeInfo, &currentTime);
            timeInfo.tm_hour += hours;
            std::time_t laterTime = std::mktime(&timeInfo);
            char formattedTime[15];
            std::tm laterTimeInfo;
            localtime_s(&laterTimeInfo, &laterTime);
            std::strftime(formattedTime, sizeof(formattedTime), "%Y%m%d%H%M%S", &laterTimeInfo);
            std::string formattedTimeString(formattedTime);
            return formattedTimeString;
        }
        return "0";
    }

    int toInt(const std::string& intString, int defaultValue) {
        int result = defaultValue;
        try {
            result = std::stoi(intString);
        } catch (const std::exception& /*unused*/) {
            result = defaultValue;
        }
        return result;
    }

    std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::vector<std::string> getAllPlayerName() {
        std::vector<std::string> mObjectLists = {};
        std::vector<Player*> mObjectPlayerLists = getAllPlayers();
        for (auto& player : mObjectPlayerLists) {
            mObjectLists.push_back(player->getRealName());
        }
        return mObjectLists;
    }

    std::vector<Player*> getAllPlayers() {
        std::vector<Player*> mObjectLists = {};
        ll::service::getLevel()->forEachPlayer([&](Player& player) -> bool {
            mObjectLists.push_back(&player);
            return true;
        });
        return mObjectLists;
    }

    Player* getPlayerFromName(const std::string& name) {
        std::vector<Player*> mObjectPlayerLists = getAllPlayers();
        for (auto& player : mObjectPlayerLists) {
            if (player->getRealName() == name) {
                return player;
            }
        }
        return nullptr;
    }

    bool isReach(const std::string& timeString) {
        if (timeString != "0") {
            std::time_t currentTime = std::time(nullptr);
            char formattedTime[15];
            std::tm currentTimeInfo;
            localtime_s(&currentTimeInfo, &currentTime);
            std::strftime(formattedTime, sizeof(formattedTime), "%Y%m%d%H%M%S", &currentTimeInfo);
            std::string formattedTimeString(formattedTime);
            int64_t formattedTimeInt = std::stoll(formattedTimeString);
            int64_t timeInt = std::stoll(timeString);
            return formattedTimeInt > timeInt;
        }
        return false;
    }

    bool isJsonArrayFind(void* mObject_ptr, const std::string& find) {
        nlohmann::ordered_json mObject = *static_cast<nlohmann::ordered_json*>(mObject_ptr);
        auto it = std::find(mObject.begin(), mObject.end(), find);
        return it != mObject.end();
    }

    bool isItemPlayerInventory(Player* player, void* itemStack_ptr) {
        ItemStack* itemStack = static_cast<ItemStack*>(itemStack_ptr);
        if (!itemStack || !player)
            return false;
        Container& mItemInventory = player->getInventory();
        for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
            auto& mItemObject = mItemInventory.getItem(i);
            if (mItemObject.isValid()) {
                if (itemStack->getTypeName() == mItemObject.getTypeName()) {
                    if (itemStack->mCount <= mItemObject.mCount) {
                        return true;
                    }
                }
            }
        }
        return false;
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
}
