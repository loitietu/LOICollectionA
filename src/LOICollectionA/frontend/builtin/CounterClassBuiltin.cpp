#include <string>
#include <variant>

#include <ll/api/Expected.h>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/builtin/CounterClassBuiltin.h"

using namespace LOICollection::frontend;

namespace CounterClassBuiltin {
    ll::Expected<ObjectRef> makeCounter(const CallbackTypeValues&) {
        auto obj = std::make_shared<Object>();
        obj->className = "Counter";
        obj->classIndex = -1;
        obj->fields["count"] = 0;

        return obj;
    }

    LOICollection::frontend::TypedValue increment(const ObjectRef& self, const CallbackTypeValues&) {
        int value = 0;
        if (const auto* current = std::get_if<int>(&self->fields["count"]))
            value = *current;

        value += 1;
        self->fields["count"] = value;

        return value;
    }

    LOICollection::frontend::TypedValue add(const ObjectRef& self, const CallbackTypeValues& args) {
        int value = 0;
        if (const auto* current = std::get_if<int>(&self->fields["count"]))
            value = *current;

        if (const auto* delta = std::get_if<int>(&args[0]))
            value += *delta;

        self->fields["count"] = value;

        return value;
    }

    LOICollection::frontend::TypedValue get(const ObjectRef& self, const CallbackTypeValues&) {
        if (const auto* current = std::get_if<int>(&self->fields["count"]))
            return *current;

        return 0;
    }

    void registerClasses(const std::string&) {
        ClassCall& classes = ClassCall::getInstance();

        classes.registerClass("Counter", { "count" });
        classes.registerConstructor("Counter", makeCounter, {});
        classes.registerMethod("Counter", "increment", increment, {});
        classes.registerMethod("Counter", "add", add, { ParamType::INT });
        classes.registerMethod("Counter", "get", get, {});
    }
}

REGISTER_CALLBACK(Counter, NativeClassBuiltin::registerClasses)