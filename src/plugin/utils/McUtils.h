#ifndef TOOLUTILS_H
#define TOOLUTILS_H

#include <string>
#include <vector>
#include <functional>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace McUtils {
    void executeCommand(void* player_ptr, std::string cmd);
    void clearItem(void* player_ptr, std::string mTypeName, int mNumber);
    void broadcastText(const std::string& text);

    std::vector<void*> getAllPlayers();
        
    bool isItemPlayerInventory(void* player_ptr, std::string mTypeName, int mNumber);

    namespace Gui {
        void submission(void* player_ptr, std::function<void(void*)> callback);
    }

    namespace scoreboard {
        int getScore(void* player_ptr, const std::string& name);
        void modifyScore(void* player_ptr, const std::string& name, int score, int action);

        inline void addScore(void* player_ptr, const std::string &name, int score) {
            modifyScore(player_ptr, name, score, 0x1);
        }

        inline void reduceScore(void* player_ptr, const std::string &name, int score) {
            modifyScore(player_ptr, name, score, 0x2);
        }
    }
}

#endif