#ifndef LOICOLLECTION_A_TPAPLUGIN_H
#define LOICOLLECTION_A_TPAPLUGIN_H

#include <string>

#include "ExportLib.h"

#define TPA_TYPE_TPA 0
#define TPA_TYPE_HERE 1

namespace LOICollection::Plugins::tpa {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(void* player_ptr);
        LOICOLLECTION_A_API void tpa(void* player_ptr, void* target_ptr, int type);
        LOICOLLECTION_A_API void content(void* player_ptr, std::string target);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_NDAPI bool isInvite(void* player_ptr);
    
    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif