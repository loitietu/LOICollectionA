#ifndef LOICOLLECTIONAPI_APIUTILS_H
#define LOICOLLECTIONAPI_APIUTILS_H

#include <string>

namespace LOICollectionAPI {
    void initialization();
    void translateString2(std::string& contentString, void* player_ptr, bool enable);
    std::string translateString(std::string contentString, void* player_ptr, bool enable);
}

#endif