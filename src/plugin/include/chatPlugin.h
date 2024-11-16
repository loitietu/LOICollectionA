#ifndef LOICOLLECTION_A_CHATPLUGIN_H
#define LOICOLLECTION_A_CHATPLUGIN_H

#include <string>

#include "ExportLib.h"

namespace LOICollection::Plugins::chat {
    namespace MainGui {
        LOICOLLECTION_A_API void contentAdd(void* player_ptr, void* target_ptr);
        LOICOLLECTION_A_API void contentRemove(void* player_ptr, void* target_ptr);
        LOICOLLECTION_A_API void add(void* player_ptr);
        LOICOLLECTION_A_API void remove(void* player_ptr);
        LOICOLLECTION_A_API void title(void* player_ptr);
        LOICOLLECTION_A_API void setting(void* player_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_API   void update(void* player_ptr);
    LOICOLLECTION_A_API   void addChat(void* player_ptr, std::string text, int time);
    LOICOLLECTION_A_API   void delChat(void* player_ptr, std::string text);

    LOICOLLECTION_A_NDAPI bool isChat(void* player_ptr, std::string text);

    LOICOLLECTION_A_NDAPI std::string getTitle(void* player_ptr);
    LOICOLLECTION_A_NDAPI std::string getTitleTime(void* player_ptr, std::string text);

    LOICOLLECTION_A_API   void registery(void* database, std::string chat);
    LOICOLLECTION_A_API   void unregistery();
}

#endif