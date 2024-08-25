#ifndef LOICOLLECTION_A_MUTE_H
#define LOICOLLECTION_A_MUTE_H

#include <string>

namespace mutePlugin {
    void registery(void* database);
    void unregistery();
    void addMute(void* player_ptr, std::string cause, int time);
    void delMute(void* player_ptr);
    bool isMute(void* player_ptr);
}

#endif