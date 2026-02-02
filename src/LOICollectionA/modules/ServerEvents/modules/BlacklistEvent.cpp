#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/Plugins/BlacklistPlugin.h"

#include "LOICollectionA/include/ServerEvents/modules/BlacklistEvent.h"

namespace LOICollection::ServerEvents {
    std::string BlacklistAddEvent::getCause() const {
        return mCause;
    }

    int BlacklistAddEvent::getTime() const {
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
        void,
        Player& player,
        const std::string& cause,
        int time
    ) {
        BlacklistAddEvent event(player, cause, time);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(player, cause, time);
    }

    LL_TYPE_INSTANCE_HOOK(
        BlacklistRemoveEventHook,
        HookPriority::Normal,
        Plugins::BlacklistPlugin,
        &Plugins::BlacklistPlugin::delBlacklist,
        void,
        const std::string& id
    ) {
        BlacklistRemoveEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;
        
        origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryAdd();
    class BlacklistAddEventEmitter : public ll::event::Emitter<emitterFactoryAdd, BlacklistAddEvent> {
        ll::memory::HookRegistrar<BlacklistAddEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryAdd() {
        return std::make_unique<BlacklistAddEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryRemove();
    class BlacklistRemoveEventEmitter : public ll::event::Emitter<emitterFactoryRemove, BlacklistRemoveEvent> {
        ll::memory::HookRegistrar<BlacklistRemoveEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryRemove() {
        return std::make_unique<BlacklistRemoveEventEmitter>();
    }
}