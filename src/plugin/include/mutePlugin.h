#ifndef LOICOLLECTION_A_MUTE_H
#define LOICOLLECTION_A_MUTE_H

#include <string>

#include "ExportLib.h"

namespace LOICollection::Plugins::mute {
    namespace MainGui {
        LOICOLLECTION_A_API void info(void* player_ptr, std::string target);
        LOICOLLECTION_A_API void content(void* player_ptr, void* target_ptr);
        LOICOLLECTION_A_API void add(void* player_ptr);
        LOICOLLECTION_A_API void remove(void* player_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_API   void addMute(void* player_ptr, std::string cause, int time);
    LOICOLLECTION_A_API   void delMute(void* player_ptr);
    LOICOLLECTION_A_API   void delMute(std::string target);
    
    LOICOLLECTION_A_NDAPI bool isMute(void* player_ptr);

    LOICOLLECTION_A_API   void registery(void* database);
}

#endif