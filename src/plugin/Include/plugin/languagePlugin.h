#ifndef LOICOLLECTION_A_LANGUAGE_H
#define LOICOLLECTION_A_LANGUAGE_H

#include <string>

namespace languagePlugin {
    namespace MainGui {
        void open(void* player_ptr);
    }

    void registery(void* database);
    void unregistery();

    std::string getLanguage(void* player_ptr);
}

#endif