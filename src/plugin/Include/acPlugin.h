#ifndef LOICOLLECTION_A_ACPLUGIN_H
#define LOICOLLECTION_A_ACPLUGIN_H

#include "ExportLib.h"

namespace LOICollection::Plugins::announcement {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(void* player_ptr);
        LOICOLLECTION_A_API void edit(void* player_ptr);
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_NDAPI bool isClose(void* player_ptr);

    LOICOLLECTION_A_API   void registery(void* database, void* database2);
    LOICOLLECTION_A_API   void unregistery();
}

#endif