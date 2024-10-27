#ifndef LOICOLLECTION_A_MENUPLUGIN_H
#define LOICOLLECTION_A_MENUPLUGIN_H

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ExportLib.h"

#define SIMPLE_TYPE 1
#define MODAL_TYPE 2
#define CUSTOM_TYPE 3

namespace LOICollection::Plugins::menu {
    namespace MainGui {
        LOICOLLECTION_A_API void editNew(void* player_ptr);
        LOICOLLECTION_A_API void editRemove(void* player_ptr);
        LOICOLLECTION_A_API void editAwardSetting(void* player_ptr, std::string uiName, int type);
        LOICOLLECTION_A_API void editAwardNew(void* player_ptr, std::string uiName, int type);
        LOICOLLECTION_A_API void editAwardRemove(void* player_ptr, std::string uiName);
        LOICOLLECTION_A_API void editAwardCommand(void* player_ptr, std::string uiName);
        LOICOLLECTION_A_API void editAwardContent(void* player_ptr, std::string uiName);
        LOICOLLECTION_A_API void editAward(void* player_ptr);
        LOICOLLECTION_A_API void edit(void* player_ptr);
        LOICOLLECTION_A_API void custom(void* player_ptr, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void simple(void* player_ptr, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void modal(void* player_ptr, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void open(void* player_ptr, std::string uiName);
    }

    LOICOLLECTION_A_NDAPI bool checkModifiedData(void* player_ptr, nlohmann::ordered_json& data);

    LOICOLLECTION_A_API   void logicalExecution(void* player_ptr, nlohmann::ordered_json& data, nlohmann::ordered_json original);

    LOICOLLECTION_A_API   void registery(void* database, std::string itemid);
    LOICOLLECTION_A_API   void unregistery();
}

#endif