#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

class Player;
class ScoreboardId;

namespace McUtils {
    void executeCommand(std::string cmd);
    void executeCommand(Player& player, std::string cmd);
    void clearItem(Player& player, std::string mTypeName, int mNumber);
    void broadcastText(const std::string& text);

    std::vector<Player*> getAllPlayers();
        
    bool isItemPlayerInventory(Player& player, std::string mTypeName, int mNumber);

    namespace scoreboard {
        int getScore(Player& player, const std::string& name);
        void modifyScore(ScoreboardId& identity, const std::string& name, int score, int action);
        void addScore(Player& player, const std::string &name, int score); 
        void reduceScore(Player& player, const std::string &name, int score);
    }
}