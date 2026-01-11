#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include "LOICollectionA/include/Plugins/NoticePlugin.h"

#include "LOICollectionA/include/ServerEvents/modules/NoticeEvent.h"

namespace LOICollection::ServerEvents {
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
        void,
        const std::string& id,
        const std::string& title,
        int priority,
        bool poiontout
    ) {
        NoticeCreateEvent event(id, title, priority, poiontout);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(id, title, priority, poiontout);
    }

    LL_TYPE_INSTANCE_HOOK(
        NoticeDeleteEventHook,
        HookPriority::Normal,
        Plugins::NoticePlugin,
        &Plugins::NoticePlugin::remove,
        void,
        const std::string& id
    ) {
        NoticeDeleteEvent event(id);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return;

        origin(id);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryCreate();
    class NoticeCreateEventEmitter : public ll::event::Emitter<emitterFactoryCreate, NoticeCreateEvent> {
        ll::memory::HookRegistrar<NoticeCreateEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryCreate() {
        return std::make_unique<NoticeCreateEventEmitter>();
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryDelete();
    class NoticeDeleteEventEmitter : public ll::event::Emitter<emitterFactoryDelete, NoticeDeleteEvent> {
        ll::memory::HookRegistrar<NoticeDeleteEventHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactoryDelete() {
        return std::make_unique<NoticeDeleteEventEmitter>();
    }
}