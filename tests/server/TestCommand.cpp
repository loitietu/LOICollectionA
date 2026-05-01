#include <gtest/gtest.h>

#include <chrono>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>

#include <mc/world/level/Level.h>

#include <mc/scripting/ServerScriptManager.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>

#include <mc/server/SimulatedPlayer.h>

LL_AUTO_TYPE_INSTANCE_HOOK(
    registerBuiltinCommands,
    ll::memory::HookPriority::Normal,
    ServerScriptManager,
    &ServerScriptManager::$onServerThreadStarted,
    EventResult,
    ServerInstance& ins
) {
    auto result = origin(ins);

    testing::InitGoogleTest();

    auto& cmd = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand("test", "LOICollectionA -> test command");
    cmd.overload().text("all").execute([](CommandOrigin const&, CommandOutput& output) -> void {
        ll::coro::keepThis([output]() -> ll::coro::CoroTask<> {
            auto sp = SimulatedPlayer::create("test_player", ll::service::getLevel()->getSharedSpawnPos());
            if (!sp)
                co_return;

            sp->mXuid = "114514677";
            
            co_await std::chrono::seconds(1);

            (void)RUN_ALL_TESTS();

            sp->disconnect();
            sp->remove();
            sp->setGameTestHelper(nullptr);
        }).launch(ll::thread::ServerThreadExecutor::getDefault());
    });

    return result;
}
