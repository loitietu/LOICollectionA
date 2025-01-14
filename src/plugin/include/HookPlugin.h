#pragma once

#include <string>
#include <functional>

#include "base/Macro.h"

class Mob;
class Actor;
class Player;
class NetworkIdentifier;

enum class ScoreChangedType {
    add,
    reduce,
    set
};

namespace LOICollection::HookPlugin {
    namespace Event {
        LOICOLLECTION_A_API void onLoginPacketSendEvent(const std::function<void(NetworkIdentifier*, std::string, std::string)>& callback);
        LOICOLLECTION_A_API void onPlayerScoreChangedEvent(const std::function<void(Player*, int, std::string, ScoreChangedType)>& callback);
        LOICOLLECTION_A_API void onPlayerHurtEvent(const std::function<bool(Mob*, Actor*, float)>& callback);
    }

    LOICOLLECTION_A_API void interceptTextPacket(const std::string& uuid);
    LOICOLLECTION_A_API void interceptGetNameTag(const std::string& uuid);
    LOICOLLECTION_A_API void uninterceptTextPacket(const std::string& uuid);
    LOICOLLECTION_A_API void uninterceptGetNameTag(const std::string& uuid);
    LOICOLLECTION_A_API void setFakeSeed(const std::string& str);
    
    LOICOLLECTION_A_API void registery();
    LOICOLLECTION_A_API void unregistery();
}