#pragma once

#include <string>
#include <vector>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::chat {
    namespace MainGui {
        LOICOLLECTION_A_API void contentAdd(Player& player, Player& target);
        LOICOLLECTION_A_API void contentRemove(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void title(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void addTitle(Player& player, const std::string& text, int time);
    LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
    LOICOLLECTION_A_API   void delTitle(Player& player, const std::string& text);
    LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);

    LOICOLLECTION_A_NDAPI std::string getTitle(Player& player);
    LOICOLLECTION_A_NDAPI std::string getTitleTime(Player& player, const std::string& text);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getTitles(Player& player);
    LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);

    LOICOLLECTION_A_NDAPI bool isTitle(Player& player, const std::string& text); 
    LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player, Player& target);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database, void* setting);
    LOICOLLECTION_A_API   void unregistery();
}