#ifndef LOICOLLECTION_A_SHOPPLUGIN_H
#define LOICOLLECTION_A_SHOPPLUGIN_H

#include <map>
#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace shopPlugin {
    namespace MainGui {
        void menu(void* player_ptr, nlohmann::ordered_json& data, bool type);
        void commodity(void* player_ptr, nlohmann::ordered_json& data, std::map<std::string, std::string> options, bool type);
        void title(void* player_ptr, nlohmann::ordered_json& data, std::map<std::string, std::string> options, bool type);
        void open(void* player_ptr, std::string uiName);
    }

    bool checkModifiedData(void* player_ptr, nlohmann::ordered_json data, int number);

    void registery(void* database);
}

#endif