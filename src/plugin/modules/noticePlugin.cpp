#include <memory>
#include <string>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
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
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"

#include "utils/McUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"
#include "data/SQLiteStorage.h"

#include "include/noticePlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::notice {
    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<SQLiteStorage> db2;
    std::shared_ptr<ll::io::Logger> logger;
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void setting(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
            form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.setting.switch1"), isClose(player));
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return;

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                db2->set("OBJECT$" + mObject, "notice_Toggle1", 
                    std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
                );
            });
        }

        void edit(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "notice.gui.setTitle"), "", db->get<std::string>("title"));
            
            nlohmann::ordered_json data = db->toJson("content");
            for (int i = 1; i < (int)(data.size() + 1); i++) {
                std::string mLine = tr(mObjectLanguage, "notice.gui.line");
                ll::string_utils::replaceAll(mLine, "${index}", std::to_string(i));
                form.appendInput("Input" + std::to_string(i), mLine, "", data.at(i - 1).get<std::string>());
            }

            form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.addLine"));
            form.appendToggle("Toggle2", tr(mObjectLanguage, "notice.gui.removeLine"));
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return;

                std::string mTitle = std::get<std::string>(dt->at("Input"));
                nlohmann::ordered_json data = db->toJson("content");
                if (std::get<uint64>(dt->at("Toggle1")))
                    data.push_back("");
                else if (std::get<uint64>(dt->at("Toggle2")))
                    data.erase(data.end() - 1);
                else {
                    for (int i = 1; i < (int)(data.size() + 1); i++)
                        data.at(i - 1) = std::get<std::string>(dt->at("Input" + std::to_string(i)));
                }
                db->set("title", mTitle);
                db->set("content", data);
                db->save();

                MainGui::edit(pl);

                logger->info(LOICollection::LOICollectionAPI::translateString(tr({}, "notice.log"), pl));
            });
        }

        void open(Player& player) {
            nlohmann::ordered_json data = db->toJson("content");

            ll::form::CustomForm form(db->get<std::string>("title"));
            for (nlohmann::ordered_json::iterator it = data.begin(); it != data.end(); ++it)
                form.appendLabel(*it);
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("notice", "§e§lLOICollection -> §a公告系统", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success("The UI has been opened to player {}", player.getRealName());
            });
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error("You do not have permission to use this command.");

                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::edit(player);

                output.success("The UI has been opened to player {}", player.getRealName());
            });
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::setting(player);

                output.success("The UI has been opened to player {}", player.getRealName());
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
                    if (!db2->has("OBJECT$" + mObject, "notice_Toggle1"))
                        db2->set("OBJECT$" + mObject, "notice_Toggle1", "false");
                    
                    if (isClose(event.self()))
                        return;
                    MainGui::open(event.self());
                }
            );
        }
    }

    bool isClose(Player& player) {
        if (!isValid()) return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db2->has("OBJECT$" + mObject, "notice_Toggle1"))
            return db2->get("OBJECT$" + mObject, "notice_Toggle1") == "true";
        return false;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr && db2 != nullptr;
    }

    void registery(void* database, void* config) {
        db = std::move(*static_cast<std::unique_ptr<JsonStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(config);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        if (!db->has("title") || !db->has("content")) {
            db->set("title", std::string("Test Bulletin 123"));
            db->set("content", nlohmann::ordered_json::array({"This is a test announcement, welcome to this plugin!"}));
            db->save();
        }
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}