#ifndef TOOLUTILS_H
#define TOOLUTILS_H

#include <string>
#include <vector>
#include <filesystem>

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerScoreSetFunction.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace toolUtils {
    void initialization();
    void executeCommand(Player* player, const std::string& command);
    void clearItem(Player* player, void* itemStack_ptr);
    void broadcastText(const std::string& text);

    std::string getVersion();
    std::string getDevice(Player* player);
    std::string timeCalculate(int hours); 

    int toInt(const std::string& intString, int defaultValue);

    std::vector<std::string> split(const std::string& s, const std::string& delimiter);
    std::vector<std::string> getAllPlayerName();
    std::vector<Player*> getAllPlayers();

    Player* getPlayerFromName(const std::string& name);
    
    bool isReach(const std::string& timeString);
    bool isItemPlayerInventory(Player* player, void* itemStack_ptr);

    namespace scoreboard {
        int getScore(Player* player, const std::string& name);
        void modifyScore(Player* player, const std::string& name, int score, PlayerScoreSetFunction action);
        void addScore(Player* player, const std::string &name, int score);
        void reduceScore(Player* player, const std::string &name, int score);
        void* addObjective(const std::string& name, const std::string& displayName);
    }

    namespace Config {
        void mergeJson(nlohmann::ordered_json& source, nlohmann::ordered_json& target);
        void SynchronousPluginConfigVersion(void* config_ptr);
        void SynchronousPluginConfigType(void* config_ptr, const std::filesystem::path& path);
    }
}

#endif