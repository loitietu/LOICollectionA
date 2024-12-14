#ifndef LOICOLLECTION_A_CHATPLUGIN_H
#define LOICOLLECTION_A_CHATPLUGIN_H

#include <string>

#include "ExportLib.h"

class Player;

namespace LOICollection::Plugins::chat {
    namespace MainGui {
        LOICOLLECTION_A_API void contentAdd(Player& player, Player& target);
        LOICOLLECTION_A_API void contentRemove(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void title(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void update(Player& player);
    LOICOLLECTION_A_API   void addChat(Player& player, std::string text, int time);
    LOICOLLECTION_A_API   void delChat(Player& player, std::string text);

    LOICOLLECTION_A_NDAPI bool isChat(Player& player, std::string text);

    LOICOLLECTION_A_NDAPI std::string getTitle(Player& player);
    LOICOLLECTION_A_NDAPI std::string getTitleTime(Player& player, std::string text);

    LOICOLLECTION_A_API   void registery(void* database, std::string chat);
    LOICOLLECTION_A_API   void unregistery();
}

#endif