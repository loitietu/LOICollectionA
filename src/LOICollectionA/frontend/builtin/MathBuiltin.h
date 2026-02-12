#pragma once

#include <string>

#include "LOICollectionA/frontend/Callback.h"

namespace MathBuiltin {
    void registerFunctions(const std::string& namespaces);

    std::string abs(const LOICollection::frontend::CallbackTypeValues& args);
    std::string min(const LOICollection::frontend::CallbackTypeValues& args);
    std::string max(const LOICollection::frontend::CallbackTypeValues& args);
    std::string sqrt(const LOICollection::frontend::CallbackTypeValues& args);
    std::string pow(const LOICollection::frontend::CallbackTypeValues& args);
    std::string log(const LOICollection::frontend::CallbackTypeValues& args);
    std::string sin(const LOICollection::frontend::CallbackTypeValues& args);
    std::string cos(const LOICollection::frontend::CallbackTypeValues& args);

    std::string random(const LOICollection::frontend::CallbackTypeValues& args);
}
