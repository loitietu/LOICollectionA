#ifndef LOICOLLECTION_A_LANGUAGE_H
#define LOICOLLECTION_A_LANGUAGE_H

#include <string>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::language {
    namespace MainGui {
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_NDAPI std::string getLanguage(std::string mObject);
    LOICOLLECTION_A_NDAPI std::string getLanguage(Player& player);

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif