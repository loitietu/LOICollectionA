#pragma once

#include <string>

#include "LOICollectionA/frontend/Callback.h"

namespace MathBuiltin {
    void registerFunctions(const std::string& namespaces);

    LOICollection::frontend::TypedValue abs(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue min(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue max(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue sqrt(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue pow(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue log(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue sin(const LOICollection::frontend::CallbackTypeValues& args);
    LOICollection::frontend::TypedValue cos(const LOICollection::frontend::CallbackTypeValues& args);

    LOICollection::frontend::TypedValue random(const LOICollection::frontend::CallbackTypeValues& args);
}
