#ifndef LOICOLLECTIONAPI_APIUTILS_H
#define LOICOLLECTIONAPI_APIUTILS_H

#include <string>
#include <functional>

#include "ExportLib.h"

namespace LOICollection::LOICollectionAPI {
    LOICOLLECTION_A_API void initialization();
    LOICOLLECTION_A_API void registerVariable(const std::string& name, const std::function<std::string(void*)> callback);
    LOICOLLECTION_A_API void registerVariableParameter(const std::string& name, const std::function<std::string(void*, std::string)> callback);

    LOICOLLECTION_A_API std::string& translateString(std::string& contentString, void* player_ptr);
    LOICOLLECTION_A_API std::string& translateString(const std::string& contentString, void* player_ptr);
}

#endif