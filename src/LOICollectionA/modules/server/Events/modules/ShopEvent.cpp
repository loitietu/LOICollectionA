#include <memory>

#include <nlohmann/json_fwd.hpp>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"

#include "LOICollectionA/include/server/Events/modules/ShopEvent.h"

namespace LOICollection::server::Events {
    std::string ShopCreateEvent::getTarget() const {
        return mTarget;
    }

    std::string ShopDeleteEvent::getTarget() const {
        return mTarget;
    }

    LL_TYPE_INSTANCE_HOOK(
        ShopCreateEventHook,
        HookPriority::Normal,
        Plugins::ShopPlugin,
        &Plugins::ShopPlugin::create,
        ll::Expected<void>,
        const std::string& id,
        const nlohmann::ordered_json& data
    ) {
        ShopCreateEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return {};

        return origin(id, data);
    }

    LL_TYPE_INSTANCE_HOOK(
        ShopDeleteEventHook,
        HookPriority::Normal,
        Plugins::ShopPlugin,
        &Plugins::ShopPlugin::remove,
        ll::Expected<void>,
        const std::string& id
    ) {
        ShopDeleteEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return {};

        return origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> ShopEmitterFactoryCreate();
    class ShopCreateEventEmitter : public ll::event::Emitter<ShopEmitterFactoryCreate, ShopCreateEvent> {
        ll::memory::HookRegistrar<ShopCreateEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> ShopEmitterFactoryCreate() {
        return std::make_unique<ShopCreateEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> ShopEmitterFactoryDelete();
    class ShopDeleteEventEmitter : public ll::event::Emitter<ShopEmitterFactoryDelete, ShopDeleteEvent> {
        ll::memory::HookRegistrar<ShopDeleteEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> ShopEmitterFactoryDelete() {
        return std::make_unique<ShopDeleteEventEmitter>();
    }
}