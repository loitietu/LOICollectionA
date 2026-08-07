#include <memory>
#include <string>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/ui/base/UIRawMessage.h>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/builtin/ui/base/UIRawMessageClass.h"

using namespace LOICollection::frontend;

namespace UIRawMessageClass {
    ll::Expected<ObjectRef> text(const CallbackTypeValues& args) {
        auto handle = std::make_unique<UIRawMessageHandle>();
        handle->base = ll::ui::UIRawMessage::text(std::get<std::string>(args[0]));

        auto obj = std::make_shared<Object>();
        obj->className = "UIRawMessage";
        obj->classIndex = -1;
        obj->native = std::move(handle);

        return obj;
    }

    ll::Expected<ObjectRef> translate(const CallbackTypeValues& args) {
        auto key = std::get<std::string>(args[0]);
        auto handle = std::make_unique<UIRawMessageHandle>();

        if (args.size() == 1) {
            handle->base = ll::ui::UIRawMessage::translate(key);
        } else {
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ArrayRef>) {
                    auto subs = arg->elements
                        | std::views::filter([](auto&& e) { return std::holds_alternative<std::string>(e); })
                        | std::views::transform([](auto&& e) { return std::get<std::string>(e); })
                        | std::ranges::to<std::vector>();
                    
                    handle->base = ll::ui::UIRawMessage::translate(key, subs);
                } else if constexpr (std::is_same_v<T, ObjectRef>) {
                    handle->base = ll::ui::UIRawMessage::translate(key,
                        static_cast<UIRawMessageHandle*>(arg->native.get())->base
                    );
                }
            }, args[1]);
        }

        auto obj = std::make_shared<Object>();
        obj->className = "UIRawMessage";
        obj->classIndex = -1;
        obj->native = std::move(handle);
        return obj;
    }

    ll::Expected<ObjectRef> rawText(const CallbackTypeValues& args) {
        auto handle = std::make_unique<UIRawMessageHandle>();

        auto subs = std::get<ArrayRef>(args[0])->elements
            | std::views::filter([](auto&& e) { return std::holds_alternative<ObjectRef>(e); })
            | std::views::transform([](auto&& e) { return static_cast<UIRawMessageHandle*>(std::get<ObjectRef>(e)->native.get())->base; })
            | std::ranges::to<std::vector>();
        
        handle->base = ll::ui::UIRawMessage::rawText(subs);

        auto obj = std::make_shared<Object>();
        obj->className = "UIRawMessage";
        obj->classIndex = -1;
        obj->native = std::move(handle);

        return obj;
    }

    void registerClasses(const std::string&) {
        ClassCall& classes = ClassCall::getInstance();

        classes.registerClass("UIRawMessage", {});
        classes.registerStaticMethod("UIRawMessage", "text", text, { ParamType::STRING });
        classes.registerStaticMethod("UIRawMessage", "translate", translate, { ParamType::STRING });
        classes.registerStaticMethod("UIRawMessage", "translate", translate, { ParamType::STRING, ParamType::ARRAY });
        classes.registerStaticMethod("UIRawMessage", "translate", translate, { ParamType::STRING, ParamType::OBJECT });
        classes.registerStaticMethod("UIRawMessage", "rawText", rawText, { ParamType::ARRAY });
    }
}

REGISTER_CALLBACK(UIRawMessage, UIRawMessageClass::registerClasses)
