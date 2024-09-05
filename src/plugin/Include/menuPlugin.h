#ifndef LOICOLLECTION_A_MENUPLUGIN_H
#define LOICOLLECTION_A_MENUPLUGIN_H

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ExportLib.h"

namespace menuPlugin {
    namespace MainGui {
        LOICOLLECTION_A_API void simple(void* player_ptr, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void modal(void* player_ptr, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void open(void* player_ptr, std::string uiName);
    }

    LOICOLLECTION_A_NDAPI bool checkModifiedData(void* player_ptr, nlohmann::ordered_json& data);

    LOICOLLECTION_A_API   void registery(void* database, std::string itemid);
    LOICOLLECTION_A_API   void unregistery();
}

#endif