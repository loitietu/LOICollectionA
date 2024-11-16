#ifndef LOICOLLECTION_A_TPAPLUGIN_H
#define LOICOLLECTION_A_TPAPLUGIN_H

#include "ExportLib.h"

enum class TpaType {
    tpa,
    tphere
};

namespace LOICollection::Plugins::tpa {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(void* player_ptr);
        LOICOLLECTION_A_API void tpa(void* player_ptr, void* target_ptr, TpaType type);
        LOICOLLECTION_A_API void content(void* player_ptr, void* target_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_NDAPI bool isInvite(void* player_ptr);
    
    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif