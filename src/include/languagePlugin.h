#ifndef LOICOLLECTION_A_LANGUAGE_H
#define LOICOLLECTION_A_LANGUAGE_H

#include <string>

#include "ExportLib.h"

namespace LOICollection::Plugins::language {
    namespace MainGui {
        LOICOLLECTION_A_API void open(void* player_ptr);
    }

    LOICOLLECTION_A_NDAPI std::string getLanguage(void* player_ptr);

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif