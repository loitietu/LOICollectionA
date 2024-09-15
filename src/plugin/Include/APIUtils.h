#ifndef LOICOLLECTIONAPI_APIUTILS_H
#define LOICOLLECTIONAPI_APIUTILS_H

#include <string>

#include "ExportLib.h"

namespace LOICollectionAPI {
    LOICOLLECTION_A_API void initialization();
    LOICOLLECTION_A_API std::string& translateString(std::string& contentString, void* player_ptr, bool enable);
    LOICOLLECTION_A_API std::string& translateString(const std::string& contentString, void* player_ptr, bool enable);
}

#endif