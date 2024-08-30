#ifndef LOICOLLECTION_A_CHATPLUGIN_H
#define LOICOLLECTION_A_CHATPLUGIN_H

#include <string>

namespace chatPlugin {
    namespace MainGui {
        void add(void* player_ptr);
        void remove(void* player_ptr);
        void title(void* player_ptr);
        void setting(void* player_ptr);
        void open(void* player_ptr);
    }

    void update(void* player_ptr);
    void addChat(void* player_ptr, std::string text);
    void delChat(void* player_ptr, std::string text);

    bool isChat(void* player_ptr, std::string text);

    std::string translate(void* player_ptr, std::string text);

    void registery(void* database, std::string chat);
    void unregistery();
}

#endif