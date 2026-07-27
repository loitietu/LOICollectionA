#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include "LOICollectionA/include/server/Plugins/MutePlugin.h"

#include "LOICollectionA/include/server/Events/modules/MuteEvent.h"

namespace LOICollection::server::Events {
    std::string MuteAddBeforeEvent::getCause() const {
        return mCause;
    }

    std::string MuteAddAfterEvent::getCause() const {
        return mCause;
    }

    int MuteAddBeforeEvent::getTime() const {
        return mTime;
    }

    int MuteAddAfterEvent::getTime() const {
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
        ll::Expected<void>,
        Player& player,
        const std::string& cause,
        int time
    ) {
        MuteAddBeforeEvent beforeEvent(player, cause, time);
        ll::event::EventBus::getInstance().publish(beforeEvent);
        if (beforeEvent.isCancelled())
            return {};

        auto result = origin(player, cause, time);

        MuteAddAfterEvent afterEvent(player, cause, time);
        ll::event::EventBus::getInstance().publish(afterEvent);

        return result;
    }

    LL_TYPE_INSTANCE_HOOK(
        MuteRemoveEventHook,
        HookPriority::Normal,
        Plugins::MutePlugin,
        &Plugins::MutePlugin::delMute,
        ll::Expected<void>,
        const std::string& id
    ) {
        MuteRemoveEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return {};
        
        return origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> MuteEmitterFactoryAdd();
    class MuteAddEventEmitter : public ll::event::Emitter<MuteEmitterFactoryAdd, MuteAddBeforeEvent, MuteAddAfterEvent> {
        ll::memory::HookRegistrar<MuteAddEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> MuteEmitterFactoryAdd() {
        return std::make_unique<MuteAddEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> MuteEmitterFactoryRemove();
    class MuteRemoveEventEmitter : public ll::event::Emitter<MuteEmitterFactoryRemove, MuteRemoveEvent> {
        ll::memory::HookRegistrar<MuteRemoveEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> MuteEmitterFactoryRemove() {
        return std::make_unique<MuteRemoveEventEmitter>();
    }
}