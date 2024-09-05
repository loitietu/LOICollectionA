#ifndef LOICOLLECTION_A_WALLETPLUGIN_H
#define LOICOLLECTION_A_WALLETPLUGIN_H

#include <map>
#include <string>
#include <variant>

#include "ExportLib.h"

namespace walletPlugin {
    namespace MainGui {
        LOICOLLECTION_A_API void transfer(void* player_ptr);
        LOICOLLECTION_A_API void wealth(void* player_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_API void registery(std::map<std::string, std::variant<std::string, double>>& options);
}

#endif