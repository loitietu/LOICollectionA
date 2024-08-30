#ifndef LOICOLLECTION_A_MENUPLUGIN_H
#define LOICOLLECTION_A_MENUPLUGIN_H

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace menuPlugin {
    namespace MainGui {
        void simple(void* player_ptr, nlohmann::ordered_json& data);
        void modal(void* player_ptr, nlohmann::ordered_json& data);
        void open(void* player_ptr, std::string uiName);
    }

    bool checkModifiedData(void* player_ptr, nlohmann::ordered_json& data);

    void registery(void* database, std::string itemid);
    void unregistery();
}

#endif