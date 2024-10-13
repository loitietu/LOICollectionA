#ifndef TOOLUTILS_H
#define TOOLUTILS_H

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerScoreSetFunction.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace toolUtils {
    namespace Mc {
        void executeCommand(Player* player, std::string cmd);
        void clearItem(Player* player, void* itemStack_ptr);
        void broadcastText(const std::string& text);

        std::string getDevice(Player* player);

        std::vector<Player*> getAllPlayers();
        std::vector<std::string> getAllPlayerName();

        Player* getPlayerFromName(const std::string& name);
        
        bool isItemPlayerInventory(Player* player, void* itemStack_ptr);
    }

    namespace System {
        std::string getNowTime();
        std::string timeCalculate(int hours); 
        std::string formatDataTime(const std::string& timeString);

        int toInt(const std::string& intString, int defaultValue);
        int64_t toInt64t(const std::string& intString, int defaultValue);

        std::vector<std::string> split(const std::string& s, const std::string& delimiter);

        bool isReach(const std::string& timeString);
    }

    namespace Gui {
        void submission(Player* player, std::function<void(Player*)> callback);
    }

    namespace scoreboard {
        int getScore(Player* player, const std::string& name);
        void modifyScore(Player* player, const std::string& name, int score, PlayerScoreSetFunction action);
        void addScore(Player* player, const std::string &name, int score);
        void reduceScore(Player* player, const std::string &name, int score);
        void* addObjective(const std::string& name, const std::string& displayName);
    }

    namespace Config {
        std::string getVersion();
        void mergeJson(nlohmann::ordered_json& source, nlohmann::ordered_json& target);
        void SynchronousPluginConfigVersion(void* config_ptr);
        void SynchronousPluginConfigType(void* config_ptr, const std::filesystem::path& path);
    }
}

#endif