#ifndef LOICOLLECTION_A_SHOPPLUGIN_H
#define LOICOLLECTION_A_SHOPPLUGIN_H

#include <map>
#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ExportLib.h"

namespace shopPlugin {
    namespace MainGui {
        LOICOLLECTION_A_API void menu(void* player_ptr, nlohmann::ordered_json& data, bool type);
        LOICOLLECTION_A_API void commodity(void* player_ptr, nlohmann::ordered_json& data, std::map<std::string, std::string> options, bool type);
        LOICOLLECTION_A_API void title(void* player_ptr, nlohmann::ordered_json& data, std::map<std::string, std::string> options, bool type);
        LOICOLLECTION_A_API void open(void* player_ptr, std::string uiName);
    }

    LOICOLLECTION_A_NDAPI bool checkModifiedData(void* player_ptr, nlohmann::ordered_json data, int number);

    LOICOLLECTION_A_API   void registery(void* database);
}

#endif