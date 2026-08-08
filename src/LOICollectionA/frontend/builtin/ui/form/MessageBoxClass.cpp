#include <any>
#include <memory>
#include <string>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/ui/form/MessageBox.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

#include "LOICollectionA/frontend/ir/VM.h"

#include "LOICollectionA/frontend/builtin/ui/form/CustomFormOptionsClass.h"
#include "LOICollectionA/frontend/builtin/ui/form/MessageBoxClass.h"

using namespace LOICollection::frontend;
using namespace CustomFormOptionsClass;

namespace MessageBoxClass {
    ll::Expected<ObjectRef> makeMessageBox(const CallbackTypeValues& args, const CallbackTypePlaces& placeholders) {
        auto title = toTextValue(args[0]);
        if (!title)
            return ll::Unexpected(title.error());

        auto handle = std::make_unique<MessageBoxHandle>();
        handle->base = std::make_unique<ll::ui::MessageBox>(
            std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0)), *title
        );

        auto obj = std::make_shared<Object>();
        obj->className = "MessageBox";
        obj->classIndex = -1;
        obj->native = std::move(handle);

        return obj;
    }

    ll::Expected<TypedValue> body(const ObjectRef& self, const CallbackTypeValues& args) {
        auto* box = static_cast<MessageBoxHandle*>(self->native.get())->base.get();

        auto value = toTextValue(args[0]);
        if (!value)
            return ll::Unexpected(value.error());

        box->body(*value);
        return self;
    }

    ll::Expected<TypedValue> button1(const ObjectRef& self, const CallbackTypeValues& args) {
        auto* box = static_cast<MessageBoxHandle*>(self->native.get())->base.get();

        auto label = toTextValue(args[0]);
        if (!label)
            return ll::Unexpected(label.error());

        if (args.size() == 2) {
            auto tooltip = toTextValue(args[1]);
            if (!tooltip)
                return ll::Unexpected(tooltip.error());

            box->button1(*label, *tooltip);
        } else {
            box->button1(*label);
        }

        return self;
    }

    ll::Expected<TypedValue> button2(const ObjectRef& self, const CallbackTypeValues& args) {
        auto* box = static_cast<MessageBoxHandle*>(self->native.get())->base.get();

        auto label = toTextValue(args[0]);
        if (!label)
            return ll::Unexpected(label.error());

        if (args.size() == 2) {
            auto tooltip = toTextValue(args[1]);
            if (!tooltip)
                return ll::Unexpected(tooltip.error());

            box->button2(*label, *tooltip);
        } else {
            box->button2(*label);
        }

        return self;
    }

    ll::Expected<TypedValue> show(
        const ObjectRef& self, const CallbackTypeValues& args, const CallbackTypePlaces& placeholders
    ) {
        auto* box = static_cast<MessageBoxHandle*>(self->native.get())->base.get();

        if (args.empty()) {
            auto result = box->show();
            if (!result)
                return ll::Unexpected(result.error());

            return self;
        }

        auto callback = std::get<FunctionRefPtr>(args[0]);
        if (callback->argCount != 1)
            return ll::makeStringError("show callback must take exactly one parameter");

        auto result = box->show([callback, placeholders](ll::ui::MessageBox::Result closeResult) {
            DiagnosticEngine diagnostics;
            CallbackTypeValues values;

            if (closeResult.has_value()) {
                auto resultObj = std::make_shared<Object>();
                resultObj->className = "MessageBoxResult";
                resultObj->classIndex = -1;
                resultObj->fields["closeReason"] = static_cast<int>(closeResult->closeReason);
                resultObj->fields["selection"] = closeResult->selection
                    ? TypedValue(static_cast<int>(*closeResult->selection))
                    : TypedValue(std::monostate{});
                values.push_back(resultObj);
            } else {
                values.push_back(std::monostate{});
            }

            [[maybe_unused]] auto cbResult = ir::VM::callFunctionRef(
                callback, values, placeholders, diagnostics);

            if (diagnostics.hasErrors()) {
                ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA")
                    ->error("MessageBox::show callback: {}", diagnostics.getErrorMessage());
            }
        });

        if (!result)
            return ll::Unexpected(result.error());

        return self;
    }

    ll::Expected<TypedValue> close(const ObjectRef& self, const CallbackTypeValues&) {
        auto result = static_cast<MessageBoxHandle*>(self->native.get())->base->close();
        if (!result)
            return ll::Unexpected(result.error());

        return self;
    }

    ll::Expected<TypedValue> isShowing(const ObjectRef& self, const CallbackTypeValues&) {
        return static_cast<MessageBoxHandle*>(self->native.get())->base->isShowing();
    }

    void registerClasses(const std::string&) {
        ClassCall& classes = ClassCall::getInstance();

        classes.registerClass("MessageBoxResult", { "closeReason", "selection" });
        classes.registerField("MessageBoxResult", "closeReason", 0);
        classes.registerField("MessageBoxResult", "selection", std::monostate{});

        classes.registerClass("MessageBox", {});
        classes.registerConstructor("MessageBox", makeMessageBox, { ParamType::STRING });
        classes.registerConstructor("MessageBox", makeMessageBox, { ParamType::OBJECT });

        classes.registerMethod("MessageBox", "body", body, { ParamType::STRING });
        classes.registerMethod("MessageBox", "body", body, { ParamType::OBJECT });

        classes.registerMethod("MessageBox", "button1", button1, { ParamType::STRING });
        classes.registerMethod("MessageBox", "button1", button1, { ParamType::OBJECT });
        classes.registerMethod("MessageBox", "button1", button1,
            { ParamType::STRING, ParamType::STRING });
        classes.registerMethod("MessageBox", "button1", button1,
            { ParamType::STRING, ParamType::OBJECT });
        classes.registerMethod("MessageBox", "button1", button1,
            { ParamType::OBJECT, ParamType::STRING });
        classes.registerMethod("MessageBox", "button1", button1,
            { ParamType::OBJECT, ParamType::OBJECT });

        classes.registerMethod("MessageBox", "button2", button2, { ParamType::STRING });
        classes.registerMethod("MessageBox", "button2", button2, { ParamType::OBJECT });
        classes.registerMethod("MessageBox", "button2", button2,
            { ParamType::STRING, ParamType::STRING });
        classes.registerMethod("MessageBox", "button2", button2,
            { ParamType::STRING, ParamType::OBJECT });
        classes.registerMethod("MessageBox", "button2", button2,
            { ParamType::OBJECT, ParamType::STRING });
        classes.registerMethod("MessageBox", "button2", button2,
            { ParamType::OBJECT, ParamType::OBJECT });

        classes.registerMethod("MessageBox", "show", show, {});
        classes.registerMethod("MessageBox", "show", show, { ParamType::FUNCTION });

        classes.registerMethod("MessageBox", "close", close, {});
        classes.registerMethod("MessageBox", "isShowing", isShowing, {});
    }
}

REGISTER_CALLBACK(MessageBox, MessageBoxClass::registerClasses)
