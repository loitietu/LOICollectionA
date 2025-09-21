#pragma once

#include <string>
#include <vector>

#include "base/Macro.h"

class Player;
class ConnectionRequest;
class NetworkIdentifier;

namespace LOICollection::Plugins::blacklist {
    namespace MainGui {
        LOICOLLECTION_A_API void info(Player& player, const std::string& id);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void addBlacklist(Player& player, const std::string& cause, int time);
    LOICOLLECTION_A_API   void delBlacklist(const std::string& target);

    LOICOLLECTION_A_NDAPI std::string getBlacklist(Player& player);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklists();
    
    LOICOLLECTION_A_NDAPI bool isBlacklist(const std::string& mId, const std::string& uuid, const std::string& ip, const std::string& clientId);
    LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}