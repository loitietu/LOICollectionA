#ifndef TOOLUTILS_H
#define TOOLUTILS_H

#include <string>
#include <vector>

namespace toolUtils {
    void init(void* mSelfPtr);
    void SynchronousPluginConfigVersion(void* config_ptr);
    std::string getVersion();
    std::string getDevice(void* player_ptr);
    std::string replaceString(std::string str, const std::string& from, const std::string& to);
    std::vector<std::string> split(const std::string& s, char delimiter);

    namespace scoreboard {
        int getScore(void* player_ptr, const std::string& name);
    }
}

#endif