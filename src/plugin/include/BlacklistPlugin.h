#pragma once

#include <string>

#include "base/Macro.h"

class Player;
class ConnectionRequest;
class NetworkIdentifier;

namespace LOICollection::Plugins::blacklist {
    enum SelectorType : int {
        ip = 0,
        uuid = 1,
        clientid = 2
    };

    namespace MainGui {
        LOICOLLECTION_A_API void info(Player& player, const std::string& target);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }
    
    LOICOLLECTION_A_NDAPI SelectorType getType(const std::string& type);
    
    LOICOLLECTION_A_NDAPI std::string getResult(std::string ip, std::string uuid, std::string clientid, SelectorType type);
    LOICOLLECTION_A_NDAPI std::string getResult(Player& player, SelectorType type);

    LOICOLLECTION_A_API   void addBlacklist(Player& player, std::string cause, int time, SelectorType type);
    LOICOLLECTION_A_API   void delBlacklist(const std::string& target);
    
    LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}