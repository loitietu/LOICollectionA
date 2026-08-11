#include <any>
#include <string>

#include <ll/api/Expected.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/builtin/mc/CommandBuiltin.h"

#include "LOICollectionA/utils/mc-server/CommandUtils.h"

using namespace LOICollection::frontend;

namespace CommandBuiltin {
    ll::Expected<TypedValue> runCmd(const CallbackTypeValues& args, const CallbackTypePlaces& placeholders) {
        std::reference_wrapper<Player> player = std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0));

        CommandUtils::executeCommand(player.get(), std::get<std::string>(args[0]));

        return TypedValue{};
    }

    void registerFunctions(const std::string& namespaces) {
        FunctionCall::getInstance().registerFunction(namespaces, "runCmd", runCmd, { ParamType::STRING });
    }
}

REGISTER_CALLBACK(mc, CommandBuiltin::registerFunctions)
