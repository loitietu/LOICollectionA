#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include "LOICollectionA/include/Plugins/MutePlugin.h"

#include "LOICollectionA/include/ServerEvents/modules/MuteEvent.h"

namespace LOICollection::ServerEvents {
    std::string MuteAddEvent::getCause() const {
        return mCause;
    }

    int MuteAddEvent::getTime() const {
        return mTime;
    }

    std::string MuteRemoveEvent::getTarget() const {
        return mTarget;
    }

    LL_TYPE_INSTANCE_HOOK(
        MuteAddEventHook,
        HookPriority::Normal,
        Plugins::MutePlugin,
        &Plugins::MutePlugin::addMute,
        void,
        Player& player,
        const std::string& cause,
        int time
    ) {
        origin(player, cause, time);

        MuteAddEvent event(player, cause, time);
        ll::event::EventBus::getInstance().publish(event);
    }

    LL_TYPE_INSTANCE_HOOK(
        MuteRemoveEventHook,
        HookPriority::Normal,
        Plugins::MutePlugin,
        &Plugins::MutePlugin::delMute,
        void,
        const std::string& id
    ) {
        origin(id);

        MuteRemoveEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryAdd();
    class MuteAddEventEmitter : public ll::event::Emitter<emitterFactoryAdd, MuteAddEvent> {
        ll::memory::HookRegistrar<MuteAddEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryAdd() {
        return std::make_unique<MuteAddEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryRemove();
    class MuteRemoveEventEmitter : public ll::event::Emitter<emitterFactoryRemove, MuteRemoveEvent> {
        ll::memory::HookRegistrar<MuteRemoveEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryRemove() {
        return std::make_unique<MuteRemoveEventEmitter>();
    }
}