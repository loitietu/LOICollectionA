#include <map>
#include <memory>
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
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/WalletPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::wallet {
    struct WalletOP {
        CommandSelector<Player> target;
        int score = 0;
    };

    std::map<std::string, std::variant<std::string, double>> mObjectOptions;
    
    std::shared_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void content(Player& player, mce::UUID target, TransferType type) {
            std::string mObjectLanguage = getLanguage(player);
            std::string mScore = std::get<std::string>(mObjectOptions.at("score"));

            std::string mTargetName = type == TransferType::online ? 
                ll::service::getLevel()->getPlayer(target)->getRealName() : ll::service::PlayerInfo::getInstance().fromUuid(target)->name;

            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label") + "\n" + tr(mObjectLanguage, "wallet.gui.transfer.label2");
            ll::string_utils::replaceAll(mLabel, "${tax}", std::to_string(std::get<double>(mObjectOptions.at("tax")) * 100) + "%%");
            ll::string_utils::replaceAll(mLabel, "${money}", std::to_string(ScoreboardUtils::getScore(player, mScore)));
            ll::string_utils::replaceAll(mLabel, "${target}", mTargetName);

            ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
            form.appendLabel(mLabel);
            form.appendInput("Input", tr(mObjectLanguage, "wallet.gui.transfer.input"), "", "100");
            form.sendTo(player, [target, mTargetName, type, mScore](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::transfer(pl, type);

                int mMoney = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                if (ScoreboardUtils::getScore(pl, mScore) < mMoney || mMoney <= 0)
                    return pl.sendMessage(tr(getLanguage(pl), "wallet.tips"));

                wallet::transfer(pl, target, mMoney, type);

                std::string logString = tr({}, "wallet.log");
                ll::string_utils::replaceAll(logString, "${player1}", pl.getRealName());
                ll::string_utils::replaceAll(logString, "${player2}", mTargetName);
                ll::string_utils::replaceAll(logString, "${money}", std::to_string(mMoney));
                logger->info(logString);
            });
        }

        void transfer(Player& player, TransferType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), tr(mObjectLanguage, "wallet.gui.transfer.label1"));
            switch (type) {
                case TransferType::online:
                    ll::service::getLevel()->forEachPlayer([&form](Player& mTarget) -> bool {
                        if (mTarget.isSimulatedPlayer())
                            return true;

                        form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                            MainGui::content(pl, mTarget.getUuid(), TransferType::online);
                        });
                        return true;
                    });
                    break;
                case TransferType::offline:
                    for (const ll::service::PlayerInfo::PlayerInfoEntry& mTarget : ll::service::PlayerInfo::getInstance().entries()) {
                        form.appendButton(mTarget.name, [mTarget](Player& pl) -> void {
                            MainGui::content(pl, mTarget.uuid, TransferType::offline);
                        });
                    }
                    break;
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void wealth(Player& player) {
            std::string mTipsString = ll::string_utils::replaceAll(tr(getLanguage(player), "wallet.showOff"), 
                "${money}", std::to_string(ScoreboardUtils::getScore(player,std::get<std::string>(mObjectOptions.at("score"))))
            );
            TextPacket::createSystemMessage(LOICollectionAPI::translateString(mTipsString, player)).sendToClients();
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");
            ll::string_utils::replaceAll(mLabel, "${tax}", std::to_string(std::get<double>(mObjectOptions.at("tax")) * 100) + "%%");
            ll::string_utils::replaceAll(mLabel, "${money}", std::to_string(ScoreboardUtils::getScore(player, std::get<std::string>(mObjectOptions.at("score")))));

            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), mLabel);
            form.appendButton(tr(mObjectLanguage, "wallet.gui.transfer"), "textures/ui/MCoin", "path", [](Player& pl) -> void {
                MainGui::transfer(pl, TransferType::online);
            });
            form.appendButton(tr(mObjectLanguage, "wallet.gui.offlineTransfer"), "textures/ui/icon_best3", "path", [](Player& pl) -> void {
                MainGui::transfer(pl, TransferType::offline);
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
                if (ScoreboardUtils::getScore(player, mScore) < (int)(results.size() * param.score) || param.score < 0)
                    return output.error(tr({}, "commands.wallet.error.score"));
                int mMoney = (param.score - (int)(param.score * std::get<double>(mObjectOptions.at("tax"))));
                for (Player*& target : results)
                    ScoreboardUtils::addScore(*target, mScore, mMoney);
                ScoreboardUtils::reduceScore(player, mScore, (int)(results.size() * param.score));

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

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    
                    if (!db->has("OBJECT$" + mObject)) db->create("OBJECT$" + mObject);
                    if (!db->has("OBJECT$" + mObject, "Score"))
                        db->set("OBJECT$" + mObject, "Score", "0");

                    int mScore = SystemUtils::toInt(db->get("OBJECT$" + mObject, "Score"), 0);
                    if (mScore > 0) {
                        ScoreboardUtils::addScore(event.self(), std::get<std::string>(mObjectOptions.at("score")), mScore);
                        db->set("OBJECT$" + mObject, "Score", "0");
                    }
                }
            );
        }
    }

    void transfer(Player& player, mce::UUID target, int score, TransferType type, bool isReduce) {
        std::string mScore = std::get<std::string>(mObjectOptions.at("score"));
        int mTargetMoney = score - (int)(score * std::get<double>(mObjectOptions.at("tax")));

        if (isReduce) ScoreboardUtils::reduceScore(player, mScore, score);
        if (type == TransferType::online) {
            Player* mObject = ll::service::getLevel()->getPlayer(target);
            ScoreboardUtils::addScore(*mObject, mScore, mTargetMoney);
        } else {
            std::string mObject = target.asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            db->set("OBJECT$" + mObject, "Score", std::to_string(
                SystemUtils::toInt(db->get("OBJECT$" + mObject, "Score"), 0) + mTargetMoney
            ));
        }
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* setting, std::map<std::string, std::variant<std::string, double>>& options) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        mObjectOptions = std::move(options);
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}