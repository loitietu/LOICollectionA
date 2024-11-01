#ifndef LOICOLLECTION_A_SHOPPLUGIN_H
#define LOICOLLECTION_A_SHOPPLUGIN_H

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ExportLib.h"

#define SHOP_BUY 1
#define SHOP_SELL 2

namespace LOICollection::Plugins::shop {
    namespace MainGui {
        LOICOLLECTION_A_API void editNew(void* player_ptr);
        LOICOLLECTION_A_API void editRemove(void* player_ptr);
        LOICOLLECTION_A_API void editAwardSetting(void* player_ptr, std::string uiName, int type);
        LOICOLLECTION_A_API void editAwardNew(void* player_ptr, std::string uiName, int type);
        LOICOLLECTION_A_API void editAwardRemove(void* player_ptr, std::string uiName);
        LOICOLLECTION_A_API void editAwardContent(void* player_ptr, std::string uiName);
        LOICOLLECTION_A_API void editAward(void* player_ptr);
        LOICOLLECTION_A_API void edit(void* player_ptr);
        LOICOLLECTION_A_API void menu(void* player_ptr, nlohmann::ordered_json& data, bool type);
        LOICOLLECTION_A_API void commodity(void* player_ptr, nlohmann::ordered_json& data, nlohmann::ordered_json original, bool type);
        LOICOLLECTION_A_API void title(void* player_ptr, nlohmann::ordered_json& data, nlohmann::ordered_json original, bool type);
        LOICOLLECTION_A_API void open(void* player_ptr, std::string uiName);
    }

    LOICOLLECTION_A_NDAPI bool checkModifiedData(void* player_ptr, nlohmann::ordered_json data, int number);

    LOICOLLECTION_A_API   void registery(void* database);
}

#endif