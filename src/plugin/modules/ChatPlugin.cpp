#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <numeric>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerChatEvent.h>
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
#include "include/MutePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/ChatPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::chat {
    struct ChatOP {
        CommandSelector<Player> target;
        std::string titleName;
        int time = 0;
    };

    std::map<std::string, std::variant<std::string, int>> mObjectOptions;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerChatEventListener;
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void contentAdd(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "chat.gui.manager.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "chat.gui.manager.add.input2"), "", "0");
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::add(pl);

                addChat(target, std::get<std::string>(dt->at("Input1")),
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0));
            });
        }
        
        void contentRemove(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = target.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.remove.dropdown"), db->list("OBJECT$" + mObject + "$TITLE"));
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::remove(pl);

                delChat(target, std::get<std::string>(dt->at("dropdown")));
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.add.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    MainGui::contentAdd(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.remove.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    MainGui::contentRemove(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void title(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            
            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(LOICollectionAPI::translateString(tr(mObjectLanguage, "chat.gui.setTitle.label"), player));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.setTitle.dropdown"), db->list("OBJECT$" + mObject + "$TITLE"));
            form.sendTo(player, [mObject](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::setting(pl);

                db->set("OBJECT$" + mObject, "title", std::get<std::string>(dt->at("dropdown")));
                
                logger->info(LOICollectionAPI::translateString(tr({}, "chat.log1"), pl));
            });
        }

        void blacklistSet(Player& player, const std::string& target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            std::string mObjectLabel = tr(mObjectLanguage, "chat.gui.setBlacklist.set.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("OBJECT$" + mObject + "$CHAT" + target, "name", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(
                db->get("OBJECT$" + mObject + "$CHAT" + target, "time", "None")
            ));

            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist.set.remove"), [target](Player& pl) -> void {
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
                    tr(mObjectLanguage, "chat.gui.setBlacklist.tips1"), "${size}", std::to_string(mSize)
                ));
            }
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.setBlacklist.add.label"));
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
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist.add"), [](Player& pl) -> void {
                MainGui::blacklistAdd(pl);
            });
            for (std::string& mTarget : getBlacklist(player)) {
                form.appendButton(mTarget, [mTarget](Player& pl) -> void {
                    MainGui::blacklistSet(pl, mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::setting(pl);
            });
        }

        void setting(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.setTitle"), "textures/ui/backup_replace", "path", [](Player& pl) -> void {
                MainGui::title(pl);
            });
            form.appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist"), "textures/ui/icon_book_writable", "path", [](Player& pl) -> void {
                MainGui::blacklist(pl);
            });
            form.sendTo(player);
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.manager.add"), "textures/ui/backup_replace", "path", [](Player& pl) -> void {
                MainGui::add(pl);
            });
            form.appendButton(tr(mObjectLanguage, "chat.gui.manager.remove"), "textures/ui/free_download_symbol", "path", [](Player& pl) -> void {
                MainGui::remove(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("chat", tr({}, "commands.chat.description"), CommandPermissionLevel::Any);
            command.overload<ChatOP>().text("add").required("target").required("titleName").optional("time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    addChat(*pl, param.titleName, param.time);

                    output.success(fmt::runtime(tr({}, "commands.chat.success.add")), param.titleName, pl->getRealName());
                }
            });
            command.overload<ChatOP>().text("remove").required("target").required("titleName").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    delChat(*pl, param.titleName);

                    output.success(fmt::runtime(tr({}, "commands.chat.success.remove")), pl->getRealName(), param.titleName);
                }
            });
            command.overload<ChatOP>().text("set").required("target").required("titleName").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    std::string mObject = pl->getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (db->has("OBJECT$" + mObject + "$TITLE", param.titleName))
                        db->set("OBJECT$" + mObject, "title", param.titleName);

                    output.success(fmt::runtime(tr({}, "commands.chat.success.set")), pl->getRealName(), param.titleName);
                }
            });
            command.overload<ChatOP>().text("list").required("target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& player : results) {
                    std::string mObject = player->getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');

                    std::vector<std::string> mObjectList = db->list("OBJECT$" + mObject + "$TITLE");
                    std::string result = std::accumulate(mObjectList.cbegin(), mObjectList.cend(), std::string(), 
                        [](std::string a, const std::string& b) -> std::string {
                        return a.append(a.empty() ? "" : ", ").append(b);
                    });

                    output.success(fmt::runtime(tr({}, "commands.chat.success.list")), player->getRealName(), result.empty() ? "None" : result);
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));

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
                    if (!db->has("OBJECT$" + mObject)) {
                        db->create("OBJECT$" + mObject);
                        db->set("OBJECT$" + mObject, "title", "None");
                    }
                    if (!db->has("OBJECT$" + mObject + "$TITLE")) {
                        db->create("OBJECT$" + mObject + "$TITLE");
                        db->set("OBJECT$" + mObject + "$TITLE", "None", "0");
                    }
                    if (!db->has("OBJECT$" + mObject + "$CHAT"))
                        db->create("OBJECT$" + mObject + "$CHAT");
                }
            );
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [](ll::event::PlayerChatEvent& event) -> void {
                    if (event.self().isSimulatedPlayer() || mute::isMute(event.self()))
                        return;
                    event.cancel();
                    
                    std::string mChat = std::get<std::string>(mObjectOptions.at("chat"));
                    ll::string_utils::replaceAll(mChat, "${chat}", event.message());

                    LOICollectionAPI::translateString(mChat, event.self());
                    McUtils::broadcastText(mChat, [&event](Player& player) -> bool {
                        std::string mObject = event.self().getUuid().asString();
                        std::replace(mObject.begin(), mObject.end(), '-', '_');

                        std::vector<std::string> mList = getBlacklist(player);
                        return std::find(mList.begin(), mList.end(), mObject) == mList.end();
                    });
                }, ll::event::EventPriority::Normal
            );
        }
    }

    void update(Player& player) {
        if (!isValid()) return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE")) {
            for (std::string& i : db->list("OBJECT$" + mObject + "$TITLE")) {
                if (i == "None") continue;
                
                std::string mTimeString = db->get("OBJECT$" + mObject + "$TITLE", i);
                if (SystemUtils::isReach(mTimeString)) {
                    db->del("OBJECT$" + mObject + "$TITLE", i);
                    logger->info(LOICollectionAPI::translateString(
                        ll::string_utils::replaceAll(tr({}, "chat.log4"), "${title}", i), player
                    ));
                }
            }
        }
        if (db->has("OBJECT$" + mObject)) {
            std::string mTitle = db->get("OBJECT$" + mObject, "title");
            if (!db->has("OBJECT$" + mObject + "$TITLE", mTitle))
                db->set("OBJECT$" + mObject, "title", "None");
        }
    }

    void addChat(Player& player, const std::string& text, int time) {
        if (!isValid()) return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject + "$TITLE"))
            db->create("OBJECT$" + mObject + "$TITLE");
        db->set("OBJECT$" + mObject + "$TITLE", text, time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time) : "0");

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log2"), "${title}", text), player
        ));
    }

    void addBlacklist(Player& player, Player& target) {
        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        db->set("OBJECT$" + mObject + "$CHAT", mTargetObject, "blacklist");
        
        db->create("OBJECT$" + mObject + "$CHAT" + mTargetObject);
        db->set("OBJECT$" + mObject + "$CHAT" + mTargetObject, "name", target.getRealName());
        db->set("OBJECT$" + mObject + "$CHAT" + mTargetObject, "time", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log5"), "${target}", mTargetObject), player
        ));
    }

    void delChat(Player& player, const std::string& text) {
        if (!isValid()) return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject) && text != "None") {
            db->del("OBJECT$" + mObject + "$TITLE", text);
            update(player);
        }
        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log3"), "${title}", text), player
        ));
    }

    void delBlacklist(Player& player, const std::string& target) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        db->del("OBJECT$" + mObject + "$CHAT", target);
        db->remove("OBJECT$" + mObject + "$CHAT" + target);

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log6"), "${target}", target), player
        ));
    }

    std::string getTitle(Player& player) {
        if (!isValid()) return "None";

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "title");
        return "None";
    }

    std::string getTitleTime(Player& player, const std::string& text) {
        if (!isValid()) return "None";

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE")) {
            std::string mTimeString = db->get("OBJECT$" + mObject + "$TITLE", text);
            if (mTimeString != "0")
                return mTimeString;
        }
        return "None";
    }

    std::vector<std::string> getBlacklist(Player& player) {
        if (!isValid()) return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->list("OBJECT$" + mObject + "$CHAT");
    }

    bool isChat(Player& player, const std::string& text) {
        if (!isValid()) return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE"))
            return db->has("OBJECT$" + mObject + "$TITLE", text);
        return false;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database, std::map<std::string, std::variant<std::string, int>> options) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        mObjectOptions = std::move(options);
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(PlayerChatEventListener);
    }
}