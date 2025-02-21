#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>

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

#include "data/SQLiteStorage.h"

#include "include/TpaPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::tpa {
    enum SelectorType : int {
        tpa = 0,
        tphere = 1
    };
    struct TpaOP {
        CommandSelector<Player> target;
        SelectorType type;
    };

    std::map<std::string, std::variant<std::string, int>> mObjectOptions;

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
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("OBJECT$" + mObject + "$PLAYER" + target, "name", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(
                db->get("OBJECT$" + mObject + "$PLAYER" + target, "time", "None")
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

            int mSize = std::get<int>(mObjectOptions.at("blacklist"));
            if (((int) getBlacklist(player).size()) >= mSize) {
                return player.sendMessage(ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "tpa.tips2"), "${size}", std::to_string(mSize)
                ));
            }
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.blacklist.add.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                if (mTarget->getUuid() == player.getUuid())
                    continue;
                
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    addBlacklist(pl, *mTarget);
                });
            }
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

            int mScore = std::get<int>(mObjectOptions.at("required"));
            std::string mScoreboard = std::get<std::string>(mObjectOptions.at("score"));
            if (mScore && McUtils::scoreboard::getScore(player, mScoreboard) < mScore) {
                player.sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.tips1"), "${score}", std::to_string(mScore)
                ));
                return;
            }
            McUtils::scoreboard::reduceScore(player, mScoreboard, mScore);

            ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"), (type == TpaType::tpa)
                ? LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.there"), player)
                : LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.here"), player),
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

                if (std::get<std::string>(dt->at("dropdown")) == "tpa")
                    return MainGui::tpa(pl, target, TpaType::tpa);
                MainGui::tpa(pl, target, TpaType::tphere);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.title"), tr(mObjectLanguage, "tpa.gui.label2"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                std::vector<std::string> mList = getBlacklist(*mTarget);
                if (std::find(mList.begin(), mList.end(), mObject) != mList.end())
                    continue;

                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    MainGui::content(pl, *mTarget);
                });
            }
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("tpa", tr({}, "commands.tpa.description"), CommandPermissionLevel::Any);
            command.overload<TpaOP>().text("invite").required("type").required("target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, TpaOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                std::string mObject = player.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                int mScore = std::get<int>(mObjectOptions.at("required"));
                std::string mScoreboard = std::get<std::string>(mObjectOptions.at("score"));
                for (Player*& pl : results) {
                    if (McUtils::scoreboard::getScore(*pl, mScoreboard) < mScore) {
                        output.error(fmt::runtime(tr({}, "commands.tpa.error.invite")), mScore);
                        break;
                    }

                    std::vector<std::string> mList = getBlacklist(*pl);
                    if (std::find(mList.begin(), mList.end(), mObject) != mList.end())
                        continue;

                    MainGui::tpa(player, *pl, param.type == SelectorType::tpa
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
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (!db2->has("OBJECT$" + mObject)) db2->create("OBJECT$" + mObject);
                    if (!db2->has("OBJECT$" + mObject, "Tpa_Toggle1"))
                        db2->set("OBJECT$" + mObject, "Tpa_Toggle1", "false");
                    if (!db->has("OBJECT$" + mObject + "$PLAYER"))
                        db->create("OBJECT$" + mObject + "$PLAYER");
                }
            );
        }
    }

    void addBlacklist(Player& player, Player& target) {
        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        db->set("OBJECT$" + mObject + "$PLAYER", mTargetObject, "blacklist");
        
        db->create("OBJECT$" + mObject + "$PLAYER" + mTargetObject);
        db->set("OBJECT$" + mObject + "$PLAYER" + mTargetObject, "name", target.getRealName());
        db->set("OBJECT$" + mObject + "$PLAYER" + mTargetObject, "time", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        logger->info(LOICollection::LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "tpa.log2"), "${target}", mTargetObject), player
        ));
    }

    void delBlacklist(Player& player, const std::string& target) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        db->del("OBJECT$" + mObject + "$PLAYER", target);
        db->remove("OBJECT$" + mObject + "$PLAYER" + target);

        logger->info(LOICollection::LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "tpa.log3"), "${target}", target), player
        ));
    }

    std::vector<std::string> getBlacklist(Player& player) {
        if (!isValid()) return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->list("OBJECT$" + mObject + "$PLAYER");
    }

    bool isInvite(Player& player) {
        if (!isValid()) return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db2->has("OBJECT$" + mObject))
            return db2->get("OBJECT$" + mObject, "Tpa_Toggle1") == "true";
        return false;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr && db2 != nullptr;
    }

    void registery(void* database, void* setting, std::map<std::string, std::variant<std::string, int>>& options) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
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