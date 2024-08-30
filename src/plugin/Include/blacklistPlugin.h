#ifndef LOICOLLECTION_A_BLACKLIST_H
#define LOICOLLECTION_A_BLACKLIST_H

#include <string>

namespace blacklistPlugin {
    namespace MainGui {
        void add(void* player_ptr);
        void remove(void* player_ptr);
        void open(void* player_ptr);
    }

    void registery(void* database);
    void unregistery();
    
    void addBlacklist(void* player_ptr, std::string cause, int time, int type);
    void delBlacklist(std::string target);
    bool isBlacklist(void* player_ptr);
}

#endif