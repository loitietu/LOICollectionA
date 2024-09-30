#ifndef LOICOLLECTION_A_BLACKLIST_H
#define LOICOLLECTION_A_BLACKLIST_H

#include <string>

#include "ExportLib.h"

namespace blacklistPlugin {
    namespace MainGui {
        LOICOLLECTION_A_API void info(void* player_ptr, std::string target);
        LOICOLLECTION_A_API void content(void* player_ptr, std::string target);
        LOICOLLECTION_A_API void add(void* player_ptr);
        LOICOLLECTION_A_API void remove(void* player_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_API   void addBlacklist(void* player_ptr, std::string cause, int time, int type);
    LOICOLLECTION_A_API   void delBlacklist(std::string target);
    LOICOLLECTION_A_NDAPI bool isBlacklist(void* player_ptr);

    LOICOLLECTION_A_API   void registery(void* database);
}

#endif