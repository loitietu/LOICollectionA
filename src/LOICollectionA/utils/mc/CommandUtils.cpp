#include <string>

#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/Minecraft.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/Command.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/MinecraftCommands.h>
#include <mc/server/commands/ServerCommandOrigin.h>
#include <mc/server/commands/PlayerCommandOrigin.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <mc/deps/core/utility/MCRESULT.h>
#include <mc/deps/core/string/HashedString.h>

#include "LOICollectionA/utils/mc/CommandUtils.h"

namespace CommandUtils {
    void executeCommand(Player& player, const std::string& command) {
        if (command.find("${player}") == std::string::npos) {
            CommandContext context = CommandContext(
                command,
                std::make_unique<PlayerCommandOrigin>(ll::service::getLevel(), player.getOrCreateUniqueID()),
                CommandVersion::CurrentVersion()
            );

            ll::service::getMinecraft()->mCommands->executeCommand(context, false);

            return;
        }

        std::string mCommand = command;

        ll::string_utils::replaceAll(mCommand, "${player}", std::string(player.mName));

        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        Command* mCommandObject = ll::service::getMinecraft()->mCommands->compileCommand(
            HashedString(mCommand), origin, static_cast<CurrentCmdVersion>(CommandVersion::CurrentVersion()),
            [](std::string const&) -> void {
                
            }
        );

        if (mCommandObject) {
            CommandOutput output(CommandOutputType::AllOutput);
            mCommandObject->run(origin, output);
        }
    }
}