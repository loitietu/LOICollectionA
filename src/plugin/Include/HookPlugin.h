#ifndef LOICOLLECTION_A_HOOKPLUGIN_H
#define LOICOLLECTION_A_HOOKPLUGIN_H

#include <string>
#include <cstdint>
#include <functional>

namespace HookPlugin {
    namespace Event {
        void onTextPacketSendEvent(const std::function<bool(void*, const std::string&)>& callback);
    }

    void registery();
    void setFakeSeed(int64_t fakeSeed);
}

#endif