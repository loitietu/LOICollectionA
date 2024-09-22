#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

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
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "Include/APIUtils.h"
#include "Include/languagePlugin.h"
#include "Include/blacklistPlugin.h"
#include "Include/mutePlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/toolUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/chatPlugin.h"

using I18nUtils::tr;
using languagePlugin::getLanguage;

namespace chatPlugin {
    std::string mChatString;
    std::unique_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerChatEventListener;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - Chat");

    namespace MainGui {
        void add(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "chat.gui.manager.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "chat.gui.manager.add.input2"), "", "0");
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.add.dropdown"), toolUtils::getAllPlayerName());
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::open(&pl);
                    return;
                }
                std::string PlayerSelectName = std::get<std::string>(dt->at("dropdown"));
                std::string PlayerInputTitle = std::get<std::string>(dt->at("Input1"));
                int time = toolUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                
                Player* pl2 = toolUtils::getPlayerFromName(PlayerSelectName);
                std::string mObject = pl2->getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                db->set("OBJECT$" + mObject + "$TITLE", PlayerInputTitle, toolUtils::timeCalculate(time));

                toolUtils::Gui::submission(&pl, [](void* player_ptr) {
                    MainGui::add(player_ptr);
                });
            });
        }

        void remove(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.remove.dropdown1"), toolUtils::getAllPlayerName());
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::open(&pl);
                    return;
                }
                Player* pl2 = toolUtils::getPlayerFromName(std::get<std::string>(dt->at("dropdown")));
                std::string mObject = pl2->getUuid().asString();
                std::string mObjectLanguage = getLanguage(pl2);
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                std::vector<std::string> list = db->list("OBJECT$" + mObject + "$TITLE");

                ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
                form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
                form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.remove.dropdown2"), list);
                form.sendTo(*pl2, [mObject](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                    if (!dt) {
                        MainGui::remove(&pl);
                        return;
                    }
                    std::string PlayerSelectTitle = std::get<std::string>(dt->at("dropdown"));
                    if (PlayerSelectTitle != "None") {
                        db->del("OBJECT$" + mObject + "$TITLE", PlayerSelectTitle);
                        update(&pl);
                    }

                    toolUtils::Gui::submission(&pl, [](void* player_ptr) {
                        MainGui::remove(player_ptr);
                    });
                });
            });
        }

        void title(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObject = player->getUuid().asString();
            std::string mObjectLanguage = getLanguage(player);
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            std::vector<std::string> list = db->list("OBJECT$" + mObject + "$TITLE");
            
            ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
            form.appendLabel(LOICollectionAPI::translateString(tr(mObjectLanguage, "chat.gui.setTitle.label"), player));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.setTitle.dropdown"), list);
            form.sendTo(*player, [mObject](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::setting(&pl);
                    return;
                }
                std::string PlayerSelectTitle = std::get<std::string>(dt->at("dropdown"));
                db->set("OBJECT$" + mObject, "title", PlayerSelectTitle);
                
                toolUtils::Gui::submission(&pl, [](void* player_ptr) {
                    MainGui::title(player_ptr);
                });
            });
        }

        void setting(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.setTitle"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::title(&pl);
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
            form.appendButton(tr(mObjectLanguage, "chat.gui.manager.add"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::add(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "chat.gui.manager.remove"), "textures/ui/free_download_symbol", "path", [](Player& pl) {
                MainGui::remove(&pl);
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
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
                .getOrCreateCommand("chat", "§e§lLOICollection -> §b个人称号", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                if ((int)player->getPlayerPermissionLevel() >= 2) {
                    output.success("The UI has been opened to player {}", player->getRealName());
                    MainGui::open(player);
                    return;
                }
                output.error("No permission to open the ui.");
            });
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::setting(player);
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    if (!blacklistPlugin::isBlacklist(&event.self())) {
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
                }
            );
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [](ll::event::PlayerChatEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    if (!mutePlugin::isMute(&event.self())) {
                        std::string mChat = mChatString;
                        
                        LOICollectionAPI::translateString(mChat, &event.self());
                        ll::string_utils::replaceAll(mChat, "${chat}", event.message());
                        toolUtils::broadcastText(mChat);
                        event.cancel();
                    }
                }
            );
        }
    }

    void update(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE")) {
            for (auto& i : db->list("OBJECT$" + mObject + "$TITLE")) {
                if (i == "None")
                    continue;
                std::string mTimeString = db->get("OBJECT$" + mObject + "$TITLE", i);
                if (toolUtils::isReach(mTimeString))
                    db->del("OBJECT$" + mObject + "$TITLE", i);
            }
        }
        if (db->has("OBJECT$" + mObject)) {
            std::string mTitle = db->get("OBJECT$" + mObject, "title");
            if (!db->has("OBJECT$" + mObject + "$TITLE", mTitle))
                db->set("OBJECT$" + mObject, "title", "None");
        }
    }

    void addChat(void* player_ptr, std::string text, int time) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject + "$TITLE"))
            db->create("OBJECT$" + mObject + "$TITLE");
        db->set("OBJECT$" + mObject + "$TITLE", text, toolUtils::timeCalculate(time));
    }

    void delChat(void* player_ptr, std::string text) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject)) {
            db->del("OBJECT$" + mObject + "$TITLE", text);
            update(player);
        }
    }

    bool isChat(void* player_ptr, std::string text) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject + "$TITLE"))
            return db->has("OBJECT$" + mObject + "$TITLE", text);
        return false;
    }

    std::string getTitle(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "title");
        return "None";
    }

    std::string getTitleTime(void* player_ptr, std::string text) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
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