#ifndef LOICOLLECTION_A_HOOKPLUGIN_H
#define LOICOLLECTION_A_HOOKPLUGIN_H

#include <string>
#include <functional>

#include "ExportLib.h"

namespace HookPlugin {
    namespace Event {
        LOICOLLECTION_A_API void onTextPacketSendEvent(const std::function<bool(void*, const std::string&)>& callback);
        LOICOLLECTION_A_API void onLoginPacketSendEvent(const std::function<void(void*, std::string, std::string)>& callback);
        LOICOLLECTION_A_API void onPlayerScoreChangedEvent(const std::function<void(void*, int, std::string)>& callback);
        LOICOLLECTION_A_API void onPlayerHurtEvent(const std::function<bool(void*, void*, float)>& callback);
    }

    LOICOLLECTION_A_API void interceptTextPacket(const std::string& uuid);
    LOICOLLECTION_A_API void setFakeSeed(const std::string& fakeSeed);

    LOICOLLECTION_A_API void registery();
    LOICOLLECTION_A_API void unregistery();
}

#endif