#pragma once

#include <string>

class Player;
class ScoreboardId;

namespace ScoreboardUtils {
    enum class ScoreType {
        add,
        reduce,
        set
    };

    bool hasScoreboard(const std::string& name);

    int getScore(Player& player, const std::string& name);
    
    void modifyScore(ScoreboardId& identity, const std::string& name, int score, ScoreType action);
    void addScore(Player& player, const std::string &name, int score); 
    void reduceScore(Player& player, const std::string &name, int score);
}