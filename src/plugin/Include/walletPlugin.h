#ifndef LOICOLLECTION_A_WALLETPLUGIN_H
#define LOICOLLECTION_A_WALLETPLUGIN_H

#include <map>
#include <string>
#include <variant>

namespace walletPlugin {
    namespace MainGui {
        void transfer(void* player_ptr);
        void wealth(void* player_ptr);
        void open(void* player_ptr);
    }

    void registery(std::map<std::string, std::variant<std::string, double>>& options);
}

#endif