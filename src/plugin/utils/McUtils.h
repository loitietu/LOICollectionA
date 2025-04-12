#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

class Player;
class ItemStack;
class ScoreboardId;

enum class ScoreType {
    add,
    reduce,
    set
};

namespace McUtils {
    void executeCommand(Player& player, std::string cmd);
    void clearItem(Player& player, const std::string& mTypeName, int mNumber);
    void giveItem(Player& player, ItemStack& item, int mNumber);

    std::vector<Player*> getAllPlayers();
        
    bool isItemPlayerInventory(Player& player, const std::string& mTypeName, int mNumber);

    namespace scoreboard {
        int getScore(Player& player, const std::string& name);
        void modifyScore(ScoreboardId& identity, const std::string& name, int score, ScoreType action);
        void addScore(Player& player, const std::string &name, int score); 
        void reduceScore(Player& player, const std::string &name, int score);
    }
}