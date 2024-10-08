#ifndef LOICOLLECTION_A_TPAPLUGIN_H
#define LOICOLLECTION_A_TPAPLUGIN_H

#include <string>

#include "ExportLib.h"

namespace LOICollection::Plugins::tpa {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(void* player_ptr);
        LOICOLLECTION_A_API void tpa(void* player_ptr, void* target_ptr, bool type);
        LOICOLLECTION_A_API void content(void* player_ptr, std::string target);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_NDAPI bool isInvite(void* player_ptr);
    
    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif