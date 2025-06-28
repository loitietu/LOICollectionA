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
#include <ll/api/utils/StringUtils.h>

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
        std::string uiName;
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

        void content(Player& player, const std::string& uiName) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "notice.gui.edit.title"), "", db->get_ptr<std::string>("/" + uiName + "/title", ""));

            auto content = db->get_ptr<nlohmann::ordered_json>("/" + uiName + "/content");
            for (int i = 0; i < (int) content.size(); i++) {
                std::string mLine = ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "notice.gui.edit.line"), "${index}", std::to_string(i + 1)
                );
                form.appendInput("Content" + std::to_string(i), mLine, "", content.at(i));
            }

            form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.edit.show"), db->get_ptr<bool>("/" + uiName + "/poiontout", false));
            form.appendToggle("Toggle2", tr(mObjectLanguage, "notice.gui.edit.add"), false);
            form.appendToggle("Toggle3", tr(mObjectLanguage, "notice.gui.edit.remove"), false);
            form.sendTo(player, [uiName](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::edit(pl);

                db->set_ptr("/" + uiName + "/title", std::get<std::string>(dt->at("Input1")));
                db->set_ptr("/" + uiName + "/poiontout", (bool) std::get<uint64>(dt->at("Toggle1")));

                auto content = db->get_ptr<nlohmann::ordered_json>("/" + uiName + "/content");
                if (std::get<uint64>(dt->at("Toggle2")))
                    content.push_back("");
                else if (std::get<uint64>(dt->at("Toggle3")))
                    content.erase(content.end() - 1);
                else {
                    for (int i = 0; i < (int) content.size(); i++)
                        content.at(i) = std::get<std::string>(dt->at("Content" + std::to_string(i)));
                }

                db->set_ptr("/" + uiName + "/content", content);
                db->save();

                MainGui::content(pl, uiName);

                logger->info(LOICollectionAPI::translateString(tr({}, "notice.log1"), pl));
            });
        }

        void contentAdd(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "notice.gui.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "notice.gui.add.input2"), "", "title");
            form.appendInput("Input3", tr(mObjectLanguage, "notice.gui.add.input3"), "", "0");
            form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.add.switch"), false);
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::edit(pl);

                std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                nlohmann::ordered_json mObject = {
                    { "title", std::get<std::string>(dt->at("Input2")) },
                    { "content", nlohmann::ordered_json::array() },
                    { "priority", SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 0) },
                    { "poiontout", (bool)std::get<uint64>(dt->at("Toggle1")) }
                };
                db->set(mObjectId, mObject);
                db->save();

                logger->info(LOICollectionAPI::translateString(
                    ll::string_utils::replaceAll(tr({}, "notice.log2"), "${notice}", mObjectId), pl
                ));
            });
        }

        void contentRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
            for (const std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    std::string mObjectLanguage = getLanguage(pl);

                    std::string mObjectContent = tr(mObjectLanguage, "notice.gui.remove.content");
                    ll::string_utils::replaceAll(mObjectContent, "${notice}", key);

                    ll::form::ModalForm form(tr(mObjectLanguage, "notice.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "notice.gui.remove.yes"), tr(mObjectLanguage, "notice.gui.remove.no")
                    );
                    form.sendTo(pl, [key](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                        if (result != ll::form::ModalFormSelectedButton::Upper) 
                            return;

                        db->remove(key);
                        db->save();

                        logger->info(LOICollectionAPI::translateString(
                            ll::string_utils::replaceAll(tr({}, "notice.log3"), "${notice}", key), pl
                        ));
                    });
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

        void notice(Player& player, const std::string& uiName) {
            if (!db->has(uiName)) {
                logger->error("NoticeUI {} reading failed.", uiName);
                return;
            }

            ll::form::CustomForm form(LOICollectionAPI::translateString(db->get_ptr<std::string>("/" + uiName + "/title", ""), player));
            for (const auto& line : db->get_ptr<nlohmann::ordered_json>("/" + uiName + "/content"))
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
                form.appendButton(LOICollectionAPI::translateString(data.at(pair.first).value("title", ""), player), [uiName = pair.first](Player& pl) {
                    MainGui::notice(pl, uiName);
                });
            }
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("notice", tr({}, "commands.notice.description"), CommandPermissionLevel::Any);
            command.overload<NoticeOP>().text("gui").optional("uiName").execute(
                [](CommandOrigin const& origin, CommandOutput& output, NoticeOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                if (param.uiName.empty())
                    MainGui::open(player);
                else MainGui::notice(player, param.uiName);

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
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) -> void {
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
                }
            );
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
        }
    }

    bool isClose(Player& player) {
        if (!isValid()) return false;

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