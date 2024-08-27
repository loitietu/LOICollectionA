#ifndef LOICOLLECTION_A_MUTE_H
#define LOICOLLECTION_A_MUTE_H

#include <string>

namespace mutePlugin {
    namespace MainGui {
        void add(void* player_ptr);
        void remove(void* player_ptr);
        void open(void* player_ptr);
    }

    void registery(void* database);
    
    void addMute(void* player_ptr, std::string cause, int time);
    void delMute(void* player_ptr);
    bool isMute(void* player_ptr);
}

#endif