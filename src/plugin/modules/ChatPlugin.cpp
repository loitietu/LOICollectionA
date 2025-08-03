#include <memory>
#include <string>
#include <vector>
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
#include "include/MutePlugin.h"

#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "ConfigPlugin.h"

#include "include/ChatPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::chat {
    struct ChatOP {
        CommandSelector<Player> Target;
        std::string Title;
        int Time = 0;
    };

    C_Config::C_Plugins::C_Chat options;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<SQLiteStorage> db2;
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

                addTitle(target, std::get<std::string>(dt->at("Input1")), SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0));
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

                delTitle(target, std::get<std::string>(dt->at("dropdown")));
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.add.label"));
            ll::service::getLevel()->forEachPlayer([&form](Player& mTarget) -> bool {
                if (mTarget.isSimulatedPlayer())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    MainGui::contentAdd(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.remove.label"));
            ll::service::getLevel()->forEachPlayer([&form](Player& mTarget) -> bool {
                if (mTarget.isSimulatedPlayer())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    MainGui::contentRemove(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void title(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(LOICollectionAPI::translateString(tr(mObjectLanguage, "chat.gui.setTitle.label"), player));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.setTitle.dropdown"), getTitles(player));
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::setting(pl);

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                db2->set("OBJECT$" + mObject, "Chat_Title", std::get<std::string>(dt->at("dropdown")));
                
                logger->info(LOICollectionAPI::translateString(tr({}, "chat.log1"), pl));
            });
        }

        void blacklistSet(Player& player, const std::string& target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            std::string mObjectLabel = tr(mObjectLanguage, "chat.gui.setBlacklist.set.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("Blacklist", mObject + "." + target + "_NAME", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(
                db->get("Blacklist", mObject + "." + target + "_TIME", "None"), "None"
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

            if (((int) getBlacklist(player).size()) >= options.BlacklistUpload) {
                return player.sendMessage(ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "chat.gui.setBlacklist.tips1"), "${size}", std::to_string(options.BlacklistUpload)
                ));
            }
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.setBlacklist.add.label"));
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
            command.overload<ChatOP>().text("add").required("Target").required("Title").optional("Time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    addTitle(*pl, param.Title, param.Time);

                    output.success(fmt::runtime(tr({}, "commands.chat.success.add")), param.Title, pl->getRealName());
                }
            });
            command.overload<ChatOP>().text("remove").required("Target").required("Title").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    delTitle(*pl, param.Title);

                    output.success(fmt::runtime(tr({}, "commands.chat.success.remove")), pl->getRealName(), param.Title);
                }
            });
            command.overload<ChatOP>().text("set").required("Target").required("Title").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    std::string mObject = pl->getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    
                    db2->set("OBJECT$" + mObject, "Chat_Title", param.Title);

                    output.success(fmt::runtime(tr({}, "commands.chat.success.set")), pl->getRealName(), param.Title);
                }
            });
            command.overload<ChatOP>().text("list").required("Target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& player : results) {
                    std::vector<std::string> mObjectList = getTitles(*player);
                    std::string result = std::accumulate(mObjectList.cbegin(), mObjectList.cend(), std::string(), [](std::string a, const std::string& b) -> std::string {
                        return a.append(a.empty() ? "" : ", ").append(b);
                    });

                    output.success(fmt::runtime(tr({}, "commands.chat.success.list")), player->getRealName(), result.empty() ? "None" : result);
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));

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

                if (!db2->has("OBJECT$" + mObject, "Chat_Title"))
                    db2->set("OBJECT$" + mObject, "Chat_Title", "None");
            });
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [](ll::event::PlayerChatEvent& event) -> void {
                    if (event.self().isSimulatedPlayer() || mute::isMute(event.self()))
                        return;

                    event.cancel();

                    std::string mChat = options.FormatText;
                    ll::string_utils::replaceAll(mChat, "${chat}", event.message());
                    
                    LOICollectionAPI::translateString(mChat, event.self());
                    TextPacket packet = TextPacket::createChat(
                        "", mChat, "", event.self().getXuid(), ""
                    );

                    ll::service::getLevel()->forEachPlayer([packet, &player = event.self()](Player& mTarget) -> bool {
                        if (!mTarget.isSimulatedPlayer() && !isBlacklist(mTarget, player))
                            packet.sendTo(mTarget);
                        return true;
                    });
                }, ll::event::EventPriority::Normal
            );
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
            eventBus.removeListener(PlayerChatEventListener);
        }
    }

    void addTitle(Player& player, const std::string& text, int time) {
        if (!isValid()) 
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        
        db->set("Titles", mObject + "." + text, time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time, "None") : "None");
        
        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log2"), "${title}", text), player
        ));
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
            ll::string_utils::replaceAll(tr({}, "chat.log5"), "${target}", mTargetObject), player
        ));
    }

    void delTitle(Player& player, const std::string& text) {
        if (!isValid())
             return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (db->has("Titles", mObject + "." + text))
            db->del("Titles", mObject + "." + text);

        if (db2->get("OBJECT$" + mObject, "Chat_Title", "None") == text)
            db2->set("OBJECT$" + mObject, "Chat_Title", "None");

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log3"), "${title}", text), player
        ));
    }

    void delBlacklist(Player& player, const std::string& target) {
        if (!isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (db->hasByPrefix("Blacklist", mObject + "." + target + "_TIME", 2))
            db->delByPrefix("Blacklist", mObject + "." + target);

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "chat.log6"), "${target}", target), player
        ));
    }

    std::string getTitle(Player& player) {
        if (!isValid())
            return "None";

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::string mTitle = db2->get("OBJECT$" + mObject, "Chat_Title", "None");
        
        if (SystemUtils::isReach(db->get("Titles", mObject + "." + mTitle, "None"))) {
            db2->set("OBJECT$" + mObject, "Chat_Title", "None");
            db->del("Titles", mObject + "." + mTitle);
            
            return "None";
        }

        return mTitle;
    }

    std::string getTitleTime(Player& player, const std::string& text) {
        if (!isValid())
            return "None";

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return db->get("Titles", mObject + "." + text, "None"); 
    }

    std::vector<std::string> getTitles(Player& player) {
        if (!isValid())
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::vector<std::string> mResult;
        for (auto& mTarget : db->listByPrefix("Titles", mObject + "."))
            mResult.push_back(mTarget.substr(mTarget.find_first_of('.') + 1));

        return mResult;
    }

    std::vector<std::string> getBlacklist(Player& player) {
        if (!isValid()) 
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::vector<std::string> mResult;
        for (auto& mTarget : db->listByPrefix("Blacklist", mObject + ".")) {
            std::string mKey = mTarget.substr(mTarget.find_first_of('.') + 1);

            mResult.push_back(mKey.substr(0, mKey.find_last_of('_')));
        }

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    bool isTitle(Player& player, const std::string& text) {
        if (!isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return db->has("Titles", mObject + "." + text);
    }

    bool isBlacklist(Player& player, Player& target) {
        if (!isValid()) 
            return false;

        std::string mObjcet = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObjcet.begin(), mObjcet.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        return db->has("Blacklist", mObjcet + "." + mTargetObject + "_TIME");
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database, void* setting) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        options = Config::GetBaseConfigContext().Plugins.Chat;

        db->create("Blacklist");
        db->create("Titles");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}
