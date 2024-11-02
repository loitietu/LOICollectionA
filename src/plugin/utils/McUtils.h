#ifndef TOOLUTILS_H
#define TOOLUTILS_H

#include <string>
#include <vector>
#include <functional>

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerScoreSetFunction.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace McUtils {
    void executeCommand(Player* player, std::string cmd);
    void clearItem(Player* player, std::string mTypeName, int mNumber);
    void broadcastText(const std::string& text);

    std::vector<Player*> getAllPlayers();
    std::vector<std::string> getAllPlayerName();

    Player* getPlayerFromName(const std::string& name);
        
    bool isItemPlayerInventory(Player* player, std::string mTypeName, int mNumber);

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
}

#endif