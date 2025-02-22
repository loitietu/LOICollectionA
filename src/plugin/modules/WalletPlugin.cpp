#include <map>
#include <string>
#include <variant>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/service/PlayerInfo.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "include/WalletPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::wallet {
    struct WalletOP {
        CommandSelector<Player> target;
        int score = 0;
    };

    std::map<std::string, std::variant<std::string, double>> mObjectOptions;
    
    std::shared_ptr<ll::io::Logger> logger;

    namespace MainGui {
        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);
            std::string mScore = std::get<std::string>(mObjectOptions.at("score"));

            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");
            ll::string_utils::replaceAll(mLabel, "${tax}", std::to_string(std::get<double>(mObjectOptions.at("tax")) * 100) + "%%");
            ll::string_utils::replaceAll(mLabel, "${money}", std::to_string(McUtils::scoreboard::getScore(player, mScore)));

            ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
            form.appendLabel(mLabel);
            form.appendInput("Input", tr(mObjectLanguage, "wallet.gui.transfer.input"), "", "100");
            form.sendTo(player, [&target, mScore](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::transfer(pl);

                int mMoney = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                int mTargetMoney = mMoney - (int)(mMoney * std::get<double>(mObjectOptions.at("tax")));
                if (McUtils::scoreboard::getScore(pl, mScore) < mMoney || mTargetMoney < 0)
                    return pl.sendMessage(tr(getLanguage(pl), "wallet.tips"));

                McUtils::scoreboard::reduceScore(pl, mScore, mMoney);
                McUtils::scoreboard::addScore(target, mScore, mTargetMoney);

                std::string logString = tr({}, "wallet.log");
                ll::string_utils::replaceAll(logString, "${player1}", pl.getRealName());
                ll::string_utils::replaceAll(logString, "${player2}", target.getRealName());
                ll::string_utils::replaceAll(logString, "${money}", std::to_string(mMoney));
                logger->info(logString);
            });
        }

        void transfer(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), tr(mObjectLanguage, "wallet.gui.transfer.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    MainGui::content(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void wealth(Player& player) {
            std::string mTipsString = ll::string_utils::replaceAll(tr(getLanguage(player), "wallet.showOff"), 
                "${money}", std::to_string(
                    McUtils::scoreboard::getScore(player,std::get<std::string>(mObjectOptions.at("score")))
                )
            );
            McUtils::broadcastText(LOICollection::LOICollectionAPI::translateString(mTipsString, player));
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");
            ll::string_utils::replaceAll(mLabel, "${tax}", std::to_string(std::get<double>(mObjectOptions.at("tax")) * 100) + "%%");
            ll::string_utils::replaceAll(mLabel, "${money}", std::to_string(McUtils::scoreboard::getScore(player, std::get<std::string>(mObjectOptions.at("score")))));

            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), mLabel);
            form.appendButton(tr(mObjectLanguage, "wallet.gui.transfer"), "textures/ui/MCoin", "path", [](Player& pl) -> void {
                MainGui::transfer(pl);
            });
            form.appendButton(tr(mObjectLanguage, "wallet.gui.wealth"), "textures/ui/creative_icon", "path", [](Player& pl) -> void {
                MainGui::wealth(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("wallet", tr({}, "commands.wallet.description"), CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload<WalletOP>().text("transfer").required("target").required("score").execute(
                [](CommandOrigin const& origin, CommandOutput& output, WalletOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                std::string mScore = std::get<std::string>(mObjectOptions.at("score"));
                if (McUtils::scoreboard::getScore(player, mScore) < (int)(results.size() * param.score) || param.score < 0)
                    return output.error(tr({}, "commands.wallet.error.score"));
                int mMoney = (param.score - (int)(param.score * std::get<double>(mObjectOptions.at("tax"))));
                for (Player*& target : results)
                    McUtils::scoreboard::addScore(*target, mScore, mMoney);
                McUtils::scoreboard::reduceScore(player, mScore, (int)(results.size() * param.score));

                output.success(fmt::runtime(tr({}, "commands.wallet.success.transfer")), param.score, results.size());
            });
            command.overload().text("wealth").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::wealth(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }
    }

    bool isValid() {
        return logger != nullptr;
    }

    void registery(std::map<std::string, std::variant<std::string, double>>& options) {
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        mObjectOptions = std::move(options);
        
        registerCommand();
    }
}