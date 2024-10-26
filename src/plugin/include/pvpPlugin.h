#ifndef LOICOLLECTION_A_PVPPLUGIN_H
#define LOICOLLECTION_A_PVPPLUGIN_H

#include "ExportLib.h"

namespace LOICollection::Plugins::pvp {
    namespace MainGui {
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_API   void enable(void* player_ptr, bool value);
    LOICOLLECTION_A_NDAPI bool isEnable(void* player_ptr);

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif