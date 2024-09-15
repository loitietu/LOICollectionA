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

#include "Include/languagePlugin.h"
#include "Include/blacklistPlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/JsonUtils.h"

#include "Include/acPlugin.h"

using I18nUtils::tr;
using languagePlugin::getLanguage;

namespace announcementPlugin {
    std::unique_ptr<JsonUtils> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - AnnounCement");

    namespace MainGui {
        void setting(void* player_ptr) {
            int index = 1;

            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            std::string mLineString = tr(mObjectLanguage, "announcement.gui.line");
            nlohmann::ordered_json data = db->toJson("content");

            ll::form::CustomForm form(tr(mObjectLanguage, "announcement.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "announcement.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "announcement.gui.setTitle"), "", db->getString("title"));
            for (nlohmann::ordered_json::iterator it = data.begin(); it != data.end(); ++it) {
                std::string mLine = ll::string_utils::replaceAll(mLineString, "${index}", std::to_string(index));
                form.appendInput("Input" + std::to_string(index), mLine, "", *it);
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
                    MainGui::setting(&pl);
                } else if (std::get<uint64>(dt->at("Toggle2"))) {
                    data.erase(data.end() -1);
                    db->set("title", mTitle);
                    db->set("content", data);
                    db->save();
                    MainGui::setting(&pl);
                } else {
                    nlohmann::ordered_json mDataNewList = nlohmann::ordered_json::array();
                    for (int i = 1; i < index; i++)
                        mDataNewList.push_back(std::get<std::string>(dt->at("Input" + std::to_string(i))));
                    db->set("title", mTitle);
                    db->set("content", mDataNewList);
                    db->save();
                }
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            nlohmann::ordered_json data = db->toJson("content");

            ll::form::CustomForm form(db->getString("title"));
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
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
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
                if ((int)player->getPlayerPermissionLevel() >= 2) {
                    output.success("The UI has been opened to player {}", player->getRealName());
                    MainGui::setting(player);
                    return;
                }
                output.error("No permission to open the ui.");
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (!blacklistPlugin::isBlacklist(&event.self())) {
                        MainGui::open(&event.self());
                    }
                }
            );
        }
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<JsonUtils>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        if (!db->has("title") || !db->has("content")) {
            nlohmann::ordered_json mEmptyArray = nlohmann::ordered_json::array();
            mEmptyArray.push_back("这是一条测试公告，欢迎使用本插件！");
            db->set("title", std::string("测试公告123"));
            db->set("content", mEmptyArray);
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