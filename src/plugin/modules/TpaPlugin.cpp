#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

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

#include "ConfigPlugin.h"

#include "include/TpaPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::tpa {
    enum SelectorType : int {
        tpa = 0,
        tphere = 1
    };
    struct TpaOP {
        CommandSelector<Player> Target;
        SelectorType Type;
    };

    C_Config::C_Plugins::C_Tpa options;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<SQLiteStorage> db2;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void generic(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.setting.title"));
            form.appendLabel(tr(mObjectLanguage, "tpa.gui.setting.label"));
            form.appendToggle("Toggle1", tr(mObjectLanguage, "tpa.gui.setting.generic.switch1"), isInvite(player));
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::setting(pl);

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                
                db2->set("OBJECT$" + mObject, "Tpa_Toggle1",
                    std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
                );
            });
        };

        void blacklistSet(Player& player, const std::string& target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            std::string mObjectLabel = tr(mObjectLanguage, "tpa.gui.setting.blacklist.set.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("Mute", mObject + "." + target + "_NAME", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(
                db->get("Mute", mObject + "." + target + "_TIME", "None"), "None"
            ));

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist.set.remove"), [target](Player& pl) -> void {
                delBlacklist(pl, target);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::blacklist(pl);
            });
        }

        void blacklistAdd(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            if (((int) getBlacklist(player).size()) >= options.BlacklistUpload) {
                return player.sendMessage(ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "tpa.tips2"), "${size}", std::to_string(options.BlacklistUpload)
                ));
            }
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.blacklist.add.label"));
            ll::service::getLevel()->forEachPlayer([&form, &player](Player& mTarget) -> bool {
                if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    addBlacklist(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::blacklist(pl);
            });
        }

        void blacklist(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.label"));
            form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist.add"), "textures/ui/editIcon", "path", [](Player& pl) -> void {
                MainGui::blacklistAdd(pl);
            });
            for (std::string& mTarget : getBlacklist(player)) {
                form.appendButton(mTarget, [mTarget](Player& pl) -> void {
                    MainGui::blacklistSet(pl, mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::setting(pl);
            });
        }

        void setting(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.label"));
            form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.generic"), "textures/ui/icon_setting", "path", [](Player& pl) -> void {
                MainGui::generic(pl);
            });
            form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist"), "textures/ui/icon_lock", "path", [](Player& pl) -> void {
                MainGui::blacklist(pl);
            });
            form.sendTo(player);
        }

        void tpa(Player& player, Player& target, TpaType type) {
            std::string mObjectLanguage = getLanguage(target);

            if (isInvite(target)) {
                player.sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.no.tips"), "${player}", target.getRealName()
                ));
                return;
            }
            
            if (options.RequestRequired && ScoreboardUtils::getScore(player, options.TargetScoreboard) < options.RequestRequired) {
                player.sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.tips1"), "${score}", std::to_string(options.RequestRequired)
                ));
                return;
            }
            ScoreboardUtils::reduceScore(player, options.TargetScoreboard, options.RequestRequired);

            ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"), 
                (type == TpaType::tpa)
                ? LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.there"), player)
                : LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.here"), player),
                tr(mObjectLanguage, "tpa.yes"), tr(mObjectLanguage, "tpa.no"));
            form.sendTo(target, [type, &player](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    std::string logString = tr({}, "tpa.log1");

                    if (type == TpaType::tpa) {
                        player.teleport(pl.getPosition(), pl.getDimensionId());
                        ll::string_utils::replaceAll(logString, "${player1}", pl.getRealName());
                        ll::string_utils::replaceAll(logString, "${player2}", player.getRealName());
                    } else {
                        pl.teleport(player.getPosition(), player.getDimensionId());
                        ll::string_utils::replaceAll(logString, "${player1}", player.getRealName());
                        ll::string_utils::replaceAll(logString, "${player2}", pl.getRealName());
                    }

                    logger->info(logString);
                    return;
                }
                
                player.sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.no.tips"), "${player}", pl.getRealName()
                ));
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "tpa.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "tpa.gui.dropdown"), { "tpa", "tphere" });
            form.sendTo(player, [&target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::open(pl);

                MainGui::tpa(pl, target, 
                    std::get<std::string>(dt->at("dropdown")) == "tpa" ? TpaType::tpa : TpaType::tphere
                );
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.title"), tr(mObjectLanguage, "tpa.gui.label2"));
            ll::service::getLevel()->forEachPlayer([&form, mObject](Player& mTarget) -> bool {
                std::vector<std::string> mList = getBlacklist(mTarget);
                if (mTarget.isSimulatedPlayer() || std::find(mList.begin(), mList.end(), mObject) != mList.end())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    MainGui::content(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("tpa", tr({}, "commands.tpa.description"), CommandPermissionLevel::Any);
            command.overload<TpaOP>().text("invite").required("Type").required("Target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, TpaOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                std::string mObject = player.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                int mScore = options.RequestRequired;
                std::string mScoreboard = options.TargetScoreboard;
                for (Player*& pl : results) {
                    if (ScoreboardUtils::getScore(*pl, mScoreboard) < mScore) {
                        output.error(fmt::runtime(tr({}, "commands.tpa.error.invite")), mScore);
                        break;
                    }

                    std::vector<std::string> mList = getBlacklist(*pl);
                    if (std::find(mList.begin(), mList.end(), mObject) != mList.end())
                        continue;

                    MainGui::tpa(player, *pl, param.Type == SelectorType::tpa
                        ? TpaType::tpa : TpaType::tphere
                    );

                    output.success(fmt::runtime(tr({}, "commands.tpa.success.invite")), pl->getRealName());
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::setting(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                std::string mObject = event.self().getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                
                if (!db2->has("OBJECT$" + mObject, "Tpa_Toggle1"))
                    db2->set("OBJECT$" + mObject, "Tpa_Toggle1", "false");
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
        }
    }

    void addBlacklist(Player& player, Player& target) {
        if (!isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        db->set("Blacklist", mObject + "." + mTargetObject + "_NAME", target.getRealName());
        db->set("Blacklist", mObject + "." + mTargetObject + "_TIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "tpa.log2"), "${target}", mTargetObject), player
        ));
    }

    void delBlacklist(Player& player, const std::string& target) {
        if (!isValid()) 
            return;
        
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (db->hasByPrefix("Blacklist", mObject + "." + target, 2))
            db->delByPrefix("Blacklist", mObject + "." + target);

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "tpa.log3"), "${target}", target), player
        ));
    }

    std::vector<std::string> getBlacklist(Player& player) {
        if (!isValid()) 
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        
        std::vector<std::string> mResult;
        for (auto& mTarget : db->listByPrefix("Blacklist", mObject + ".")) {
            std::string mKey = mTarget.substr(mTarget.find_first_of('.') + 1);

            mResult.push_back(mKey.substr(0, mKey.find_first_of('_')));
        }

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    bool isInvite(Player& player) {
        if (!isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        
        if (db2->has("OBJECT$" + mObject))
            return db2->get("OBJECT$" + mObject, "Tpa_Toggle1") == "true";
        return false;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr && db2 != nullptr;
    }

    void registery(void* database, void* setting) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        options = Config::GetBaseConfigContext().Plugins.Tpa;

        db->create("Blacklist");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}

/*
    Database:
    -> Blacklist
        UUID.UUID_NAME: NAME
        UUID.UUID_TIME: TIME
*/