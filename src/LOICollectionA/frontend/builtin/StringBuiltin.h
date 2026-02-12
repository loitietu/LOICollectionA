#pragma once

#include <string>

#include "LOICollectionA/frontend/Callback.h"

namespace StringBuiltin {
    void registerFunctions(const std::string& namespaces);

    std::string length(const LOICollection::frontend::CallbackTypeValues& args);
    std::string upper(const LOICollection::frontend::CallbackTypeValues& args);
    std::string lower(const LOICollection::frontend::CallbackTypeValues& args);
    std::string substr(const LOICollection::frontend::CallbackTypeValues& args);
    std::string trim(const LOICollection::frontend::CallbackTypeValues& args);
    std::string replace(const LOICollection::frontend::CallbackTypeValues& args);
}
