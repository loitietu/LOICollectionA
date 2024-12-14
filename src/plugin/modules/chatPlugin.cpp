#include <memory>
#include <string>
#include <stdexcept>
#include <numeric>

#include <ll/api/Logger.h>
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

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandOriginType.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"
#include "include/mutePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/chatPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

bool isPermissionFormCommandOrigin(CommandOrigin const& origin, int permissionLevel) {
    if (origin.getOriginType() == CommandOriginType::DedicatedServer) {
        return ((int) origin.getPermissionsLevel()) >= permissionLevel;
    } else {
        auto* entity = origin.getEntity();
        return ((entity != nullptr && entity->isPlayer()) && (int)(static_cast
            <Player*>(entity)->getPlayerPermissionLevel()) >= permissionLevel);
    }
}

namespace LOICollection::Plugins::chat {
    struct ChatOP {
        CommandSelector<Player> target;
        std::string titleName;
        int time = 0;
    };

    std::string mChatString;
    std::unique_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerChatEventListener;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - Chat");

    namespace MainGui {
        void contentAdd(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "chat.gui.manager.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "chat.gui.manager.add.input2"), "", "0");
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::add(pl);

                addChat(target, std::get<std::string>(dt->at("Input1")),
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0));

                McUtils::Gui::submission(pl, [](Player& player) {
                    return MainGui::add(player);
                });
            });
        }
        
        void contentRemove(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = target.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.remove.dropdown"), db->list("OBJECT$" + mObject + "$TITLE"));
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::remove(pl);

                delChat(target, std::get<std::string>(dt->at("dropdown")));

                McUtils::Gui::submission(pl, [](Player& player) {
                    return MainGui::remove(player);
                });
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.add.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) {
                    MainGui::contentAdd(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.remove.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) {
                    MainGui::contentRemove(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(pl);
            });
        }

        void title(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            
            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "chat.gui.setTitle.label"), player));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.setTitle.dropdown"), db->list("OBJECT$" + mObject + "$TITLE"));
            form.sendTo(player, [mObject](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::setting(pl);

                db->set("OBJECT$" + mObject, "title", std::get<std::string>(dt->at("dropdown")));
                
                McUtils::Gui::submission(pl, [](Player& player) {
                    return MainGui::title(player);
                });

                logger.info(LOICollection::LOICollectionAPI::translateString(tr({}, "chat.log1"), pl));
            });
        }

        void setting(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.setTitle"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::title(pl);
            });
            form.sendTo(player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(pl), "exit"));
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.manager.add"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::add(pl);
            });
            form.appendButton(tr(mObjectLanguage, "chat.gui.manager.remove"), "textures/ui/free_download_symbol", "path", [](Player& pl) {
                MainGui::remove(pl);
            });
            form.sendTo(player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(pl), "exit"));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("chat", "§e§lLOICollection -> §b个人称号", CommandPermissionLevel::Any);
            command.overload<ChatOP>().text("add").required("target").required("titleName").optional("time").execute([](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) {
                if (!isPermissionFormCommandOrigin(origin, 2))
                    return output.error("No permission to add chat.");
                for (auto& pl : param.target.results(origin)) {
                    addChat(*pl, param.titleName, param.time);

                    output.addMessage(fmt::format("Add Chat for Player {}.", 
                        pl->getRealName()), {}, CommandOutputMessageType::Success);
                }
            });
            command.overload<ChatOP>().text("remove").required("target").required("titleName").execute([](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) {
                if (!isPermissionFormCommandOrigin(origin, 2))
                    return output.error("No permission to remove chat.");
                for (auto& pl : param.target.results(origin)) {
                    delChat(*pl, param.titleName);

                    output.addMessage(fmt::format("Remove Chat for Player {}.",
                        pl->getRealName()), {}, CommandOutputMessageType::Success);
                }
            });
            command.overload<ChatOP>().text("list").required("target").execute([](CommandOrigin const& origin, CommandOutput& output, ChatOP const& param) {
                if (!isPermissionFormCommandOrigin(origin, 2))
                    return output.error("No permission to list chat.");
                for (auto& player : param.target.results(origin)) {
                    std::string mObject = player->getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');

                    std::vector<std::string> mObjectList = db->list("OBJECT$" + mObject + "$TITLE");
                    std::string result = std::accumulate(mObjectList.cbegin(), mObjectList.cend(), std::string(), 
                        [](const std::string& a, const std::string& b) {
                        return a + (a.empty() ? "" : ", ") + b;
                    });

                    output.addMessage(fmt::format("Player {}'s Chat List: {}", player->getRealName(),
                        result.empty() ? "None" : result), {}, CommandOutputMessageType::Success);
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                if ((int) player.getPlayerPermissionLevel() >= 2) {
                    MainGui::open(player);
                    output.success("The UI has been opened to player {}", player.getRealName());
                    return;
                }

                output.error("No permission to open the ui.");
            });
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::setting(player);

                output.success("The UI has been opened to player {}", player.getRealName());
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
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
                }
            );
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [](ll::event::PlayerChatEvent& event) {
                    if (event.self().isSimulatedPlayer() || mute::isMute(event.self()))
                        return;
                    event.cancel();
                    
                    std::string mChat = mChatString;
                    ll::string_utils::replaceAll(mChat, "${chat}", event.message());
                    LOICollection::LOICollectionAPI::translateString(mChat, event.self());
                    McUtils::broadcastText(mChat);
                }
            );
        }
    }

    void update(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE")) {
            for (auto& i : db->list("OBJECT$" + mObject + "$TITLE")) {
                if (i == "None") continue;
                
                std::string mTimeString = db->get("OBJECT$" + mObject + "$TITLE", i);
                if (SystemUtils::isReach(mTimeString)) {
                    db->del("OBJECT$" + mObject + "$TITLE", i);
                    logger.info(LOICollection::LOICollectionAPI::translateString(ll::string_utils::replaceAll(
                        tr({}, "chat.log4"), "${title}", i), player));
                }
            }
        }
        if (db->has("OBJECT$" + mObject)) {
            std::string mTitle = db->get("OBJECT$" + mObject, "title");
            if (!db->has("OBJECT$" + mObject + "$TITLE", mTitle))
                db->set("OBJECT$" + mObject, "title", "None");
        }
    }

    void addChat(Player& player, std::string text, int time) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject + "$TITLE"))
            db->create("OBJECT$" + mObject + "$TITLE");
        db->set("OBJECT$" + mObject + "$TITLE", text, time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time) : "0");

        logger.info(LOICollection::LOICollectionAPI::translateString(ll::string_utils::replaceAll(
            tr({}, "chat.log2"), "${title}", text), player));
    }

    void delChat(Player& player, std::string text) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject) && text != "None") {
            db->del("OBJECT$" + mObject + "$TITLE", text);
            update(player);
        }
        logger.info(LOICollection::LOICollectionAPI::translateString(ll::string_utils::replaceAll(
            tr({}, "chat.log3"), "${title}", text), player));
    }

    bool isChat(Player& player, std::string text) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE"))
            return db->has("OBJECT$" + mObject + "$TITLE", text);
        return false;
    }

    std::string getTitle(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "title");
        return "None";
    }

    std::string getTitleTime(Player& player, std::string text) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE")) {
            std::string mTimeString = db->get("OBJECT$" + mObject + "$TITLE", text);
            if (mTimeString != "0")
                return mTimeString;
        }
        return "None";
    }

    void registery(void* database, std::string chat) {
        mChatString = chat;
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        
        logger.setFile("./logs/LOICollectionA.log");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(PlayerChatEventListener);
    }
}