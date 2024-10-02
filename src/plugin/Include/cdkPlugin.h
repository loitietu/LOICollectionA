#ifndef LOICOLLECTION_A_CDK_H
#define LOICOLLECTION_A_CDK_H

#include <string>

#include "ExportLib.h"

namespace LOICollection::Plugins::cdk {
    namespace MainGui {
        LOICOLLECTION_A_API void convert(void* player_ptr);
        LOICOLLECTION_A_API void cdkNew(void* player_ptr);
        LOICOLLECTION_A_API void cdkRemove(void* player_ptr);
        LOICOLLECTION_A_API void cdkAwardScore(void* player_ptr);
        LOICOLLECTION_A_API void cdkAwardItem(void* player_ptr);
        LOICOLLECTION_A_API void cdkAwardTitle(void *player_ptr);
        LOICOLLECTION_A_API void cdkAward(void* player_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_API void cdkConvert(void* player_ptr, std::string convertString);

    LOICOLLECTION_A_API void registery(void* database);
}

#endif