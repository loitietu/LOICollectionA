#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/Plugins/BlacklistPlugin.h"

#include "LOICollectionA/include/server/Events/modules/BlacklistEvent.h"

namespace LOICollection::server::Events {
    std::string BlacklistAddBeforeEvent::getCause() const {
        return mCause;
    }

    std::string BlacklistAddAfterEvent::getCause() const {
        return mCause;
    }

    int BlacklistAddBeforeEvent::getTime() const {
        return mTime;
    }

    int BlacklistAddAfterEvent::getTime() const {
        return mTime;
    }

    std::string BlacklistRemoveEvent::getTarget() const {
        return mTarget;
    }

    LL_TYPE_INSTANCE_HOOK(
        BlacklistAddEventHook,
        HookPriority::Normal,
        Plugins::BlacklistPlugin,
        &Plugins::BlacklistPlugin::addBlacklist,
        ll::Expected<void>,
        Player& player,
        const std::string& cause,
        int time
    ) {
        BlacklistAddBeforeEvent beforeEvent(player, cause, time);
        ll::event::EventBus::getInstance().publish(beforeEvent);
        if (beforeEvent.isCancelled())
            return {};

        auto result = origin(player, cause, time);

        BlacklistAddAfterEvent afterEvent(player, cause, time);
        ll::event::EventBus::getInstance().publish(afterEvent);

        return result;
    }

    LL_TYPE_INSTANCE_HOOK(
        BlacklistRemoveEventHook,
        HookPriority::Normal,
        Plugins::BlacklistPlugin,
        &Plugins::BlacklistPlugin::delBlacklist,
        ll::Expected<void>,
        const std::string& id
    ) {
        BlacklistRemoveEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return {};
        
        return origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> BlacklistEmitterFactoryAdd();
    class BlacklistAddEventEmitter : public ll::event::Emitter<BlacklistEmitterFactoryAdd, BlacklistAddBeforeEvent, BlacklistAddAfterEvent> {
        ll::memory::HookRegistrar<BlacklistAddEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> BlacklistEmitterFactoryAdd() {
        return std::make_unique<BlacklistAddEventEmitter>();
    }
    
    static std::unique_ptr<ll::event::EmitterBase> BlacklistEmitterFactoryRemove();
    class BlacklistRemoveEventEmitter : public ll::event::Emitter<BlacklistEmitterFactoryRemove, BlacklistRemoveEvent> {
        ll::memory::HookRegistrar<BlacklistRemoveEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> BlacklistEmitterFactoryRemove() {
        return std::make_unique<BlacklistRemoveEventEmitter>();
    }
}