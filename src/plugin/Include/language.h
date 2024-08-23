#ifndef LOICOLLECTION_A_LANGUAGE_H
#define LOICOLLECTION_A_LANGUAGE_H

#include <string>

namespace languagePlugin {
    void registery(void* database);
    void unregistery();

    std::string getLanguage(void* player);
}

#endif