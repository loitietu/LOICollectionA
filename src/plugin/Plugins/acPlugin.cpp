#include <memory>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Include/APIUtils.h"
#include "Include/languagePlugin.h"

#include "Utils/toolUtils.h"
#include "Utils/I18nUtils.h"
#include "Utils/JsonUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/acPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::announcement {
    std::unique_ptr<JsonUtils> db;
    std::shared_ptr<SQLiteStorage> db2;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - AnnounCement");

    namespace MainGui {
        void setting(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "announcement.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "announcement.gui.label"));
            form.appendToggle("Toggle1", tr(mObjectLanguage, "announcement.gui.setting.switch1"), isClose(player));
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                bool mObjectToggle1 = std::get<uint64>(dt->at("Toggle1"));
                
                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                db2->set("OBJECT$" + mObject, "AnnounCement_Toggle1", mObjectToggle1 ? "true" : "false");

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::setting(player);
                });
            });
        }

        void edit(void* player_ptr) {
            int index = 1;

            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            std::string mObjectLineString = tr(mObjectLanguage, "announcement.gui.line");
            nlohmann::ordered_json data = db->toJson("content");

            ll::form::CustomForm form(tr(mObjectLanguage, "announcement.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "announcement.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "announcement.gui.setTitle"), "", db->get<std::string>("title"));
            for (auto& item : data) {
                std::string mLineString = mObjectLineString;
                std::string mLine = ll::string_utils::replaceAll(mLineString, "${index}", std::to_string(index));
                form.appendInput("Input" + std::to_string(index), mLine, "", item);
                index++;
            }
            form.appendToggle("Toggle1", tr(mObjectLanguage, "announcement.gui.addLine"));
            form.appendToggle("Toggle2", tr(mObjectLanguage, "announcement.gui.removeLine"));
            form.sendTo(*player, [index](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string mTitle = std::get<std::string>(dt->at("Input"));
                nlohmann::ordered_json data = db->toJson("content");
                if (std::get<uint64>(dt->at("Toggle1"))) {
                    data.push_back("");
                    db->set("title", mTitle);
                    db->set("content", data);
                    db->save();
                    MainGui::edit(&pl);
                    return;
                } else if (std::get<uint64>(dt->at("Toggle2"))) {
                    data.erase(data.end() - 1);
                    db->set("title", mTitle);
                    db->set("content", data);
                    db->save();
                    MainGui::edit(&pl);
                    return;
                }
                for (int i = 1; i < index; i++)
                    data.at(i - 1) = std::get<std::string>(dt->at("Input" + std::to_string(i)));
                db->set("title", mTitle);
                db->set("content", data);
                db->save();

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::edit(player);
                });

                logger.info(LOICollection::LOICollectionAPI::translateString(tr(getLanguage(&pl), "announcement.log"), &pl));
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            nlohmann::ordered_json data = db->toJson("content");

            ll::form::CustomForm form(db->get<std::string>("title"));
            for (nlohmann::ordered_json::iterator it = data.begin(); it != data.end(); ++it)
                form.appendLabel(*it);
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("announcement", "§e§lLOICollection -> §a公告系统", CommandPermissionLevel::Any);
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
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                if ((int) player->getPlayerPermissionLevel() >= 2) {
                    output.success("The UI has been opened to player {}", player->getRealName());
                    MainGui::edit(player);
                    return;
                }
                output.error("No permission to open the ui.");
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
                    if (!db2->has("OBJECT$" + mObject))
                        db2->create("OBJECT$" + mObject);
                    if (!db2->has("OBJECT$" + mObject, "AnnounCement_Toggle1"))
                        db2->set("OBJECT$" + mObject, "AnnounCement_Toggle1", "false");
                    
                    if (isClose(&event.self()))
                        return;
                    MainGui::open(&event.self());
                }
            );
        }
    }

    bool isClose(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db2->has("OBJECT$" + mObject, "AnnounCement_Toggle1"))
            return db2->get("OBJECT$" + mObject, "AnnounCement_Toggle1") == "true";
        return false;
    }

    void registery(void* database, void* database2) {
        db = std::move(*static_cast<std::unique_ptr<JsonUtils>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(database2);
        logger.setFile("./logs/LOICollectionA.log");
        if (!db->has("title") || !db->has("content")) {
            db->set("title", std::string("测试公告123"));
            db->set("content", nlohmann::ordered_json::array({"这是一条测试公告，欢迎使用本插件！"}));
            db->save();
        }
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}