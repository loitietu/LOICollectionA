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
#include <mc/entity/components_json_legacy/ProjectileComponent.h>

#include <mc/legacy/ActorUniqueID.h>

#include "LOICollectionA/include/ServerEvents/player/PlayerHurtEvent.h"

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

    PlayerHurtReason PlayerHurtEvent::getReason() const {
        return this->mReason;
    }

    LL_TYPE_INSTANCE_HOOK(
        PlayerHurtEventHook1,
        HookPriority::Normal,
        Mob,
        &Mob::hurt,
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

        PlayerHurtEvent event(*reinterpret_cast<Player*>(this), *mSource, static_cast<int>(damage), knock, ignite, PlayerHurtReason::Hurt);
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

        PlayerHurtEvent event(*reinterpret_cast<Player*>(this), *mSource, static_cast<int>(damage), knock, ignite, PlayerHurtReason::Effect);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return;

        origin(source, damage, knock, ignite);
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

        PlayerHurtEvent event(*reinterpret_cast<Player*>(this), *mSource, static_cast<int>(damage), false, false, PlayerHurtReason::Effect);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return 0.0f;

        return origin(source, damage);
    };

    LL_TYPE_INSTANCE_HOOK(
        PlayerHurtEventHook4,
        HookPriority::Normal,
        ProjectileComponent,
        &ProjectileComponent::onHit,
        void,
        Actor& owner,
        HitResult const& hitResult
    ) {
        Actor* mHitEntity = hitResult.getEntity();
        Player* mOwner = owner.getPlayerOwner();

        if ((!mHitEntity || !mOwner) || !mOwner->isPlayer() || !mHitEntity->isPlayer() || owner.getOrCreateUniqueID().rawID == mHitEntity->getOrCreateUniqueID().rawID)
            return origin(owner, hitResult);

        PlayerHurtEvent event(*reinterpret_cast<Player*>(mHitEntity), *mOwner, 0, false, false, PlayerHurtReason::Projectile);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return;

        origin(owner, hitResult);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class PlayerHurtEventEmitter : public ll::event::Emitter<emitterFactory, PlayerHurtEvent> {
        ll::memory::HookRegistrar<PlayerHurtEventHook1, PlayerHurtEventHook2, PlayerHurtEventHook3, PlayerHurtEventHook4> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<PlayerHurtEventEmitter>();
    }
}
