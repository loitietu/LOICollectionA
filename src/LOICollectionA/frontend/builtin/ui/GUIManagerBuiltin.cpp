#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/include/form/GUIManager.h"

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/builtin/ui/GUIManagerBuiltin.h"

using namespace LOICollection::form;
using namespace LOICollection::frontend;

namespace GUIManagerBuiltin {
    ll::Expected<TypedValue> value(const CallbackTypeValues& args, const CallbackTypePlaces& placeholders) {
        return GUIManager::getInstance().getValue(
            std::get<std::string>(args[0]),
            std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0))
        );
    }

    ll::Expected<TypedValue> callback(const CallbackTypeValues& args, const CallbackTypePlaces& placeholders) {
        auto result = GUIManager::getInstance().getCallback(
            std::get<std::string>(args[0]),
            std::get<ArrayRef>(args[1]),
            std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0))
        );

        if (!result.has_value())
            return ll::Unexpected(result.error());

        return TypedValue{};
    }

    ll::Expected<TypedValue> switchTo(const CallbackTypeValues& args, const CallbackTypePlaces& placeholders) {
        ll::Expected<void> result;

        switch (std::get<int>(args[1])) {
            case static_cast<int>(GUIManagerType::CustomForm): {
                result = GUIManager::getInstance().switchToCustomForm(
                    std::get<std::string>(args[0]),
                    std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0))
                );

                break;
            }
            case static_cast<int>(GUIManagerType::MessageBox): {
                result = GUIManager::getInstance().switchToMessageBox(
                    std::get<std::string>(args[0]),
                    std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0))
                );

                break;
            }
            default: result = ll::makeStringError("switchTo: Unknown form type");
        }

        if (!result.has_value())
            return ll::Unexpected(result.error());

        return TypedValue{};
    }

    void registerFunctions(const std::string& namespaces) {
        FunctionCall& functions = FunctionCall::getInstance();

        functions.registerFunction(namespaces, "value", value, { ParamType::STRING });
        functions.registerFunction(namespaces, "callback", callback, { ParamType::STRING, ParamType::ARRAY });
        functions.registerFunction(namespaces, "switchTo", switchTo, { ParamType::STRING, ParamType::INT });
    }
}

REGISTER_CALLBACK(GUIManager, GUIManagerBuiltin::registerFunctions)
