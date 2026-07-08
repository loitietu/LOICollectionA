#include <atomic>
#include <memory>
#include <string>

#include <ll/api/event/Event.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/Cancellable.h>
#include <ll/api/event/ListenerBase.h>

#include <ll/api/memory/Hook.h>

#include <mc/world/actor/Actor.h>

#include <mc/server/SimulatedPlayer.h>

#include "server/TestSimulatedPlayer.h"

class IsTestSimulatedPlayerEvent final : public ll::event::Cancellable<ll::event::Event> {
public:
    constexpr explicit IsTestSimulatedPlayerEvent() = default;
};

LL_TYPE_INSTANCE_HOOK(
    IsTestSimulatedPlayerEventHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::isSimulatedPlayer,
    bool
) {
    bool result = origin();

    IsTestSimulatedPlayerEvent event{};
    ll::event::EventBus::getInstance().publish(event);
    if (event.isCancelled())
        return false;

    return result;
}

static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
class IsTestSimulatedPlayerEventEmitter : public ll::event::Emitter<emitterFactory, IsTestSimulatedPlayerEvent> {
    ll::memory::HookRegistrar<IsTestSimulatedPlayerEventHook> hook;
};

static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
    return std::make_unique<IsTestSimulatedPlayerEventEmitter>();
}

struct TestSimulatedPlayer::Impl {
    std::atomic<bool> alive{ false };

    std::string name;

    SimulatedPlayer* simulatedPlayer{};

    ll::event::ListenerPtr eventListener;
};

TestSimulatedPlayer::TestSimulatedPlayer(const std::string& playerName) : mImpl(std::make_unique<Impl>()) {
    this->mImpl->name = playerName;

    this->mImpl->eventListener = ll::event::EventBus::getInstance().emplaceListener<IsTestSimulatedPlayerEvent>([this](IsTestSimulatedPlayerEvent& event) -> void {
        if (this->mImpl->alive.load(std::memory_order_acquire))
            event.cancel();
    });
}

TestSimulatedPlayer::~TestSimulatedPlayer() {
    if (this->mImpl->alive.load(std::memory_order_acquire))
        this->destroy();

    ll::event::EventBus::getInstance().removeListener(this->mImpl->eventListener);
}

bool TestSimulatedPlayer::create() {
    auto sp = SimulatedPlayer::create(this->mImpl->name, Vec3(0, 0, 0));
    if (!sp.has_value())
        return false;
    
    this->mImpl->simulatedPlayer = &sp.value();
    this->mImpl->alive.store(true, std::memory_order_release);
    return true;
}

bool TestSimulatedPlayer::destroy() {
    if (!this->mImpl->alive.load(std::memory_order_acquire))
        return false;

    this->mImpl->simulatedPlayer->disconnect();
    this->mImpl->simulatedPlayer->remove();
    this->mImpl->simulatedPlayer->setGameTestHelper(nullptr);

    this->mImpl->alive.store(false, std::memory_order_release);
    return true;
}

SimulatedPlayer* TestSimulatedPlayer::getPlayer() {
    return this->mImpl->simulatedPlayer;
}
