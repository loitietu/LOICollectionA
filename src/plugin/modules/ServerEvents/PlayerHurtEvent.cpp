#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/world/level/Level.h>

#include <mc/world/actor/Mob.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/player/Player.h>

#include <mc/legacy/ActorUniqueID.h>

#include "include/ServerEvents/PlayerHurtEvent.h"

namespace LOICollection::ServerEvents {
    Actor& PlayerHurtEvent::getSource() const {
        return this->mSource;
    }

    int PlayerHurtEvent::getDamage() const {
        return this->mDamage;
    }

    bool PlayerHurtEvent::isKnock() const {
        return this->mKnock;
    }

    bool PlayerHurtEvent::isIgnite() const {
        return this->mIgnite;
    }

    LL_TYPE_INSTANCE_HOOK(
        PlayerHurtEventHook1,
        HookPriority::Normal,
        Mob,
        &Mob::$_hurt,
        bool,
        ActorDamageSource const& source,
        float damage,
        bool knock,
        bool ignite
    ) {
        if (!this->isPlayer() || !source.isEntitySource() || this->getOrCreateUniqueID().rawID == source.getEntityUniqueID().rawID)
            return origin(source, damage, knock, ignite);
        Actor* mSource = ll::service::getLevel()->fetchEntity(
            source.isChildEntitySource() ? source.getEntityUniqueID() : source.getDamagingEntityUniqueID(), false
        );
        if (!mSource)
            return origin(source, damage, knock, ignite);

        PlayerHurtEvent event(*(Player*)this, *mSource, (int)damage, knock, ignite);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return false;
        return origin(source, damage, knock, ignite);
    };

    LL_TYPE_INSTANCE_HOOK(
        PlayerHurtEventHook2,
        HookPriority::Normal,
        Mob,
        &Mob::$hurtEffects,
        void,
        ActorDamageSource const& source,
        float damage,
        bool knock,
        bool ignite
    ) {
        if (!this->isPlayer() || !source.isEntitySource() || this->getOrCreateUniqueID().rawID == source.getEntityUniqueID().rawID)
            return origin(source, damage, knock, ignite);
        Actor* mSource = ll::service::getLevel()->fetchEntity(
            source.isChildEntitySource() ? source.getEntityUniqueID() : source.getDamagingEntityUniqueID(), false
        );
        if (!mSource)
            return origin(source, damage, knock, ignite);

        PlayerHurtEvent event(*(Player*)this, *mSource, (int)damage, knock, ignite);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return;
        return origin(source, damage, knock, ignite);
    };

    LL_TYPE_INSTANCE_HOOK(
        PlayerHurtEventHook3,
        HookPriority::Normal,
        Mob,
        &Mob::getDamageAfterResistanceEffect,
        float,
        ActorDamageSource const& source,
        float damage
    ) {
        if (!this->isPlayer() || !source.isEntitySource() || this->getOrCreateUniqueID().rawID == source.getEntityUniqueID().rawID)
            return origin(source, damage);
        Actor* mSource = ll::service::getLevel()->fetchEntity(
            source.isChildEntitySource() ? source.getEntityUniqueID() : source.getDamagingEntityUniqueID(), false
        );
        if (!mSource)
            return origin(source, damage);

        PlayerHurtEvent event(*(Player*)this, *mSource, (int)damage);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return 0.0f;
        return origin(source, damage);
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class PlayerHurtEventEmitter : public ll::event::Emitter<emitterFactory, PlayerHurtEvent> {
        ll::memory::HookRegistrar<PlayerHurtEventHook1> hook1;
        ll::memory::HookRegistrar<PlayerHurtEventHook2> hook2;
        ll::memory::HookRegistrar<PlayerHurtEventHook3> hook3;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<PlayerHurtEventEmitter>();
    }
}