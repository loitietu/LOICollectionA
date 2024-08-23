#ifndef LOICOLLECTION_A_BLACKLIST_H
#define LOICOLLECTION_A_BLACKLIST_H

#include <string>

namespace blacklistPlugin {
    void registery(void* database);
    void unregistery();
    void addBlacklist(void* player_ptr, std::string cause, int time, int type);
    void delBlacklist(std::string target);
}

#endif