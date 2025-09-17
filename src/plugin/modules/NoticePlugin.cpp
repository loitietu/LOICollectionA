#include <memory>
#include <string>
#include <vector>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"
#include "data/SQLiteStorage.h"

#include "include/NoticePlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::notice {
    struct NoticeOP {
        std::string Id;
    };

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

                db2->set("OBJECT$" + mObject, "Notice_Toggle1", 
                    std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
                );
            });
        }

        void content(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "notice.gui.edit.title"), "", db->get_ptr<std::string>("/" + id + "/title", ""));

            std::string mObjectLine = tr(mObjectLanguage, "notice.gui.edit.line");

            auto content = db->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
            for (int i = 0; i < (int) content.size(); i++) {
                std::string mLine = fmt::format(fmt::runtime(mObjectLine), i + 1);
                form.appendInput("Content" + std::to_string(i), mLine, "", content.at(i));
            }

            form.appendToggle("Toggle", tr(mObjectLanguage, "notice.gui.edit.show"), db->get_ptr<bool>("/" + id + "/poiontout", false));
            form.appendStepSlider("StepSlider", tr(mObjectLanguage, "notice.gui.edit.operation"), { "no", "add", "remove" });
            form.sendTo(player, [id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::edit(pl);

                db->set_ptr("/" + id + "/title", std::get<std::string>(dt->at("Input")));
                db->set_ptr("/" + id + "/poiontout", (bool) std::get<uint64>(dt->at("Toggle")));

                auto content = db->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
                switch (ll::hash_utils::doHash(std::get<std::string>(dt->at("StepSlider")))) {
                    case ll::hash_utils::doHash("add"): 
                        content.push_back("");
                        break;
                    case ll::hash_utils::doHash("remove"): 
                        content.erase(content.end() - 1);
                        break;
                    default:
                        for (int i = 0; i < (int) content.size(); i++)
                            content.at(i) = std::get<std::string>(dt->at("Content" + std::to_string(i)));
                }

                db->set_ptr("/" + id + "/content", content);
                db->save();

                MainGui::content(pl, id);

                logger->info(LOICollectionAPI::getVariableString(tr({}, "notice.log1"), pl));
            });
        }

        void contentAdd(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "notice.gui.add.input1"), tr(mObjectLanguage, "notice.gui.add.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "notice.gui.add.input2"), tr(mObjectLanguage, "notice.gui.add.input2.placeholder"));
            form.appendInput("Input3", tr(mObjectLanguage, "notice.gui.add.input3"), tr(mObjectLanguage, "notice.gui.add.input3.placeholder"));
            form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.add.switch"), false);
            form.sendTo(player, [mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::edit(pl);

                std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

                if (mObjectId.empty() || mObjectTitle.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::edit(pl);
                }

                nlohmann::ordered_json mObject = {
                    { "title", mObjectTitle },
                    { "content", nlohmann::ordered_json::array() },
                    { "priority", SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 0) },
                    { "poiontout", (bool)std::get<uint64>(dt->at("Toggle1")) }
                };

                db->set(mObjectId, mObject);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "notice.log2"), pl)), mObjectId);
            });
        }
        
        void contentRemoveInfo(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::ModalForm form(tr(mObjectLanguage, "notice.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "notice.gui.remove.content")), id),
                tr(mObjectLanguage, "notice.gui.remove.yes"), tr(mObjectLanguage, "notice.gui.remove.no")
            );
            form.sendTo(player, [id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result != ll::form::ModalFormSelectedButton::Upper) 
                    return;

                db->remove(id);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "notice.log3"), pl)), id);
            });
        }

        void contentRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
            for (const std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    MainGui::contentRemoveInfo(pl, key);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void edit(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
            form.appendButton(tr(mObjectLanguage, "notice.gui.add"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) -> void {
                MainGui::contentAdd(pl);
            });
            form.appendButton(tr(mObjectLanguage, "notice.gui.remove"), "textures/ui/book_trash_default", "path", [](Player& pl) -> void {
                MainGui::contentRemove(pl);
            });
            for (const std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    MainGui::content(pl, key);
                });
            }
            form.sendTo(player);
        }

        void notice(Player& player) {
            nlohmann::ordered_json data = db->get();

            std::vector<std::pair<std::string, int>> mContent;
            for (auto it = data.begin(); it != data.end(); ++it) {
                if (!it.value().value("poiontout", false))
                    continue;
                mContent.emplace_back(it.key(), it.value().value("priority", 0));
            }

            std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });

            for (const auto& pair : mContent) {
                nlohmann::ordered_json mObject = data.at(pair.first);
                
                ll::form::CustomForm form(LOICollectionAPI::translateString(mObject.value("title", ""), player));
                for (const auto& line : mObject.value("content", nlohmann::ordered_json::array()))
                    form.appendLabel(LOICollectionAPI::translateString(line, player));
                form.sendTo(player);
            }
        }

        void notice(Player& player, const std::string& id) {
            if (!db->has(id)) {
                logger->error("NoticeUI {} reading failed.", id);
                return;
            }

            ll::form::CustomForm form(LOICollectionAPI::translateString(db->get_ptr<std::string>("/" + id + "/title", ""), player));
            for (const auto& line : db->get_ptr<nlohmann::ordered_json>("/" + id + "/content"))
                form.appendLabel(LOICollectionAPI::translateString(line, player));
            form.sendTo(player);
        }

        void open(Player& player) {
            nlohmann::ordered_json data = db->get();

            std::vector<std::pair<std::string, int>> mContent;
            for (auto it = data.begin(); it != data.end(); ++it)
                mContent.emplace_back(it.key(), it.value().value("priority", 0));

            std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });

            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
            for (const auto& pair : mContent) {
                form.appendButton(LOICollectionAPI::translateString(data.at(pair.first).value("title", ""), player), [id = pair.first](Player& pl) {
                    MainGui::notice(pl, id);
                });
            }
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("notice", tr({}, "commands.notice.description"), CommandPermissionLevel::Any);
            command.overload<NoticeOP>().text("gui").optional("Id").execute(
                [](CommandOrigin const& origin, CommandOutput& output, NoticeOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                if (param.Id.empty())
                    MainGui::open(player);
                else
                    MainGui::notice(player, param.Id);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr({}, "commands.generic.permission"));

                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::edit(player);

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

                if (!db2->has("OBJECT$" + mObject)) db2->create("OBJECT$" + mObject);
                if (!db2->has("OBJECT$" + mObject, "Notice_Toggle1"))
                    db2->set("OBJECT$" + mObject, "Notice_Toggle1", "false");
                
                if (isClose(event.self()))
                    return;
                MainGui::notice(event.self());
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
        }
    }

    bool isClose(Player& player) {
        if (!isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db2->has("OBJECT$" + mObject, "Notice_Toggle1"))
            return db2->get("OBJECT$" + mObject, "Notice_Toggle1") == "true";
        return false;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr && db2 != nullptr;
    }

    void registery(void* database, void* setting) {
        db = std::move(*static_cast<std::unique_ptr<JsonStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->save();
    }
}