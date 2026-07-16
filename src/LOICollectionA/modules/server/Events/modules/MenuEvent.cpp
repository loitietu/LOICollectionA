#include <memory>

#include <nlohmann/json_fwd.hpp>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"

#include "LOICollectionA/include/server/Events/modules/MenuEvent.h"

namespace LOICollection::server::Events {
    std::string MenuCreateEvent::getTarget() const {
        return mTarget;
    }

    std::string MenuDeleteEvent::getTarget() const {
        return mTarget;
    }

    LL_TYPE_INSTANCE_HOOK(
        MenuCreateEventHook,
        HookPriority::Normal,
        Plugins::MenuPlugin,
        &Plugins::MenuPlugin::create,
        void,
        const std::string& id,
        const nlohmann::ordered_json& data
    ) {
        MenuCreateEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(id, data);
    }

    LL_TYPE_INSTANCE_HOOK(
        MenuDeleteEventHook,
        HookPriority::Normal,
        Plugins::MenuPlugin,
        &Plugins::MenuPlugin::remove,
        void,
        const std::string& id
    ) {
        MenuDeleteEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> MenuEmitterFactoryCreate();
    class MenuCreateEventEmitter : public ll::event::Emitter<MenuEmitterFactoryCreate, MenuCreateEvent> {
        ll::memory::HookRegistrar<MenuCreateEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> MenuEmitterFactoryCreate() {
        return std::make_unique<MenuCreateEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> MenuEmitterFactoryDelete();
    class MenuDeleteEventEmitter : public ll::event::Emitter<MenuEmitterFactoryDelete, MenuDeleteEvent> {
        ll::memory::HookRegistrar<MenuDeleteEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> MenuEmitterFactoryDelete() {
        return std::make_unique<MenuDeleteEventEmitter>();
    }
}