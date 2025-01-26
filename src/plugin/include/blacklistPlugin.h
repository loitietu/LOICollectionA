#pragma once

#include <string>

#include "base/Macro.h"

class Player;
class ConnectionRequest;
class NetworkIdentifier;

enum class BlacklistType {
    ip,
    uuid,
    clientid
};

namespace LOICollection::Plugins::blacklist {
    namespace MainGui {
        LOICOLLECTION_A_API void info(Player& player, std::string target);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }
    
    LOICOLLECTION_A_NDAPI BlacklistType getType(std::string type);
    
    LOICOLLECTION_A_NDAPI std::string getResult(const NetworkIdentifier& identifier, const ConnectionRequest& conn, BlacklistType type);
    LOICOLLECTION_A_NDAPI std::string getResult(Player& player, BlacklistType type);

    LOICOLLECTION_A_API   void addBlacklist(Player& player, std::string cause, int time, BlacklistType type);
    LOICOLLECTION_A_API   void delBlacklist(std::string target);
    
    LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
}