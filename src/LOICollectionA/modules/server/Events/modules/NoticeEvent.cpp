#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include "LOICollectionA/include/server/Plugins/NoticePlugin.h"

#include "LOICollectionA/include/server/Events/modules/NoticeEvent.h"

namespace LOICollection::server::Events {
    std::string NoticeCreateEvent::getTarget() const {
        return mTarget;
    }

    std::string NoticeCreateEvent::getTitle() const {
        return mTitle;
    }

    int NoticeCreateEvent::getPriority() const {
        return mPriority;
    }

    bool NoticeCreateEvent::isPoiontout() const {
        return mPoiontout;
    }

    std::string NoticeDeleteEvent::getTarget() const {
        return mTarget;
    }

    LL_TYPE_INSTANCE_HOOK(
        NoticeCreateEventHook,
        HookPriority::Normal,
        Plugins::NoticePlugin,
        &Plugins::NoticePlugin::create,
        ll::Expected<void>,
        const std::string& id,
        const std::string& title,
        int priority,
        bool poiontout
    ) {
        NoticeCreateEvent event(id, title, priority, poiontout);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return {};

        return origin(id, title, priority, poiontout);
    }

    LL_TYPE_INSTANCE_HOOK(
        NoticeDeleteEventHook,
        HookPriority::Normal,
        Plugins::NoticePlugin,
        &Plugins::NoticePlugin::remove,
        ll::Expected<void>,
        const std::string& id
    ) {
        NoticeDeleteEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return {};

        return origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> NoticeEmitterFactoryCreate();
    class NoticeCreateEventEmitter : public ll::event::Emitter<NoticeEmitterFactoryCreate, NoticeCreateEvent> {
        ll::memory::HookRegistrar<NoticeCreateEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> NoticeEmitterFactoryCreate() {
        return std::make_unique<NoticeCreateEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> NoticeEmitterFactoryDelete();
    class NoticeDeleteEventEmitter : public ll::event::Emitter<NoticeEmitterFactoryDelete, NoticeDeleteEvent> {
        ll::memory::HookRegistrar<NoticeDeleteEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> NoticeEmitterFactoryDelete() {
        return std::make_unique<NoticeDeleteEventEmitter>();
    }
}