#include <map>
#include <string>
#include <stdexcept>
#include <variant>
#include <vector>

#include <ll/api/Logger.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "Include/APIUtils.h"
#include "Include/languagePlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/toolUtils.h"

#include "Include/walletPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::wallet {
    std::string mScore;
    std::map<std::string, std::variant<std::string, double>> mObjectOptions;
    ll::Logger logger("LOICollectionA - Wallet");

    namespace MainGui {
        void content(void* player_ptr, std::string target) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");

            ll::string_utils::replaceAll(mLabel, "${tax}", std::to_string(std::get<double>(mObjectOptions.at("tax")) * 100) + "%%");
            ll::string_utils::replaceAll(mLabel, "${money}", std::to_string(toolUtils::scoreboard::getScore(player, mScore)));

            ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
            form.appendLabel(mLabel);
            form.appendInput("Input", tr(mObjectLanguage, "wallet.gui.stepslider.input"), "", "100");
            form.sendTo(*player, [target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::transfer(&pl);
                    return;
                }
                int mMoney = toolUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                int mTargetMoney = mMoney - (int)(mMoney * std::get<double>(mObjectOptions.at("tax")));
                if (toolUtils::scoreboard::getScore(&pl, mScore) < mMoney || mTargetMoney < 0) {
                    pl.sendMessage(tr(getLanguage(&pl), "wallet.tips"));
                    return;
                }

                toolUtils::scoreboard::reduceScore(&pl, mScore, mMoney);
                toolUtils::scoreboard::addScore(toolUtils::getPlayerFromName(target), mScore, mTargetMoney);

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::transfer(player);
                });

                std::string logString = tr(getLanguage(&pl), "wallet.log");
                ll::string_utils::replaceAll(logString, "${player1}", pl.getRealName());
                ll::string_utils::replaceAll(logString, "${player2}", target);
                ll::string_utils::replaceAll(logString, "${money}", std::to_string(mMoney));
                logger.info(logString);
            });
        }

        void transfer(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), tr(mObjectLanguage, "wallet.gui.stepslider.label"));
            for (auto& mTarget : toolUtils::getAllPlayerName()) {
                form.appendButton(mTarget, [mTarget](Player& pl) {
                    MainGui::content(&pl, mTarget);
                });
            }
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(&pl);
            });
        }

        void wealth(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mTipsString = tr(getLanguage(player), "wallet.showOff");
            LOICollection::LOICollectionAPI::translateString(mTipsString, player);
            ll::string_utils::replaceAll(mTipsString, "${money}", std::to_string(toolUtils::scoreboard::getScore(player, std::get<std::string>(mObjectOptions.at("score")))));
            toolUtils::broadcastText(mTipsString);
            
            toolUtils::Gui::submission(player, [](Player* player) {
                return MainGui::open(player);
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");

            ll::string_utils::replaceAll(mLabel, "${tax}", std::to_string(std::get<double>(mObjectOptions.at("tax")) * 100) + "%%");
            ll::string_utils::replaceAll(mLabel, "${money}", std::to_string(toolUtils::scoreboard::getScore(player, mScore)));

            ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
            form.appendLabel(mLabel);
            form.appendStepSlider("stepslider", tr(mObjectLanguage, "wallet.gui.stepslider"), { "transfer", "wealth" });
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string mSelectString = std::get<std::string>(dt->at("stepslider"));
                if (mSelectString == "transfer")
                    MainGui::transfer(&pl);
                else if (mSelectString == "wealth")
                    MainGui::wealth(&pl);
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("wallet", "§e§lLOICollection -> §b个人钱包", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player);
            });
        }
    }

    void registery(std::map<std::string, std::variant<std::string, double>>& options) {
        mScore = std::get<std::string>(options.at("score"));
        mObjectOptions = options;

        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
    }
}