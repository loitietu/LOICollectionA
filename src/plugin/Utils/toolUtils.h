#ifndef TOOLUTILS_H
#define TOOLUTILS_H

#include <string>
#include <vector>

#include <mc/world/actor/player/Player.h>

namespace toolUtils {
    void init(void* mSelfPtr);
    void SynchronousPluginConfigVersion(void* config_ptr);
    std::string getVersion();
    std::string getDevice(void* player_ptr);
    std::string replaceString(std::string str, const std::string& from, const std::string& to);
    std::string timeCalculate(int hours); 
    int toInt(const std::string& intString, int defaultValue);
    std::vector<std::string> split(const std::string& s, char delimiter);
    std::vector<std::string> getAllPlayerName();
    std::vector<Player*> getAllPlayers();
    Player* getPlayerFromName(const std::string& name);
    bool isReach(const std::string& timeString);
    bool isJsonArrayFind(void* mObject_ptr, const std::string& find);

    namespace scoreboard {
        int getScore(void* player_ptr, const std::string& name);
        void addScore(void* player_ptr, const std::string& name, int score);
        void* addObjective(const std::string& name, const std::string& displayName);
    }
}

#endif