#include <memory>
#include <vector>
#include <string>

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
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/Minecraft.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CurrentCmdVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/ServerCommandOrigin.h>
#include <mc/server/commands/MinecraftCommands.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/Plugins/LanguagePlugin.h"

#include "utils/InventoryUtils.h"
#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "ConfigPlugin.h"

#include "include/Plugins/MenuPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::menu {
    struct MenuOP {
        std::string Id;
    };

    C_Config::C_Plugins::C_Menu options;
    
    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerUseItemEventListener;

    namespace MainGui {
        void editNewInfo(Player& player, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);  

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "menu.gui.button1.input1"), tr(mObjectLanguage, "menu.gui.button1.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), tr(mObjectLanguage, "menu.gui.button1.input2.placeholder"));

            switch (type) {
                case MenuType::Simple:
                    form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), tr(mObjectLanguage, "menu.gui.button1.input3.placeholder"));
                    form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input4.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), tr(mObjectLanguage, "menu.gui.button1.input5.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), tr(mObjectLanguage, "menu.gui.button1.input6.placeholder"));
                    break;
                case MenuType::Modal:
                    form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), tr(mObjectLanguage, "menu.gui.button1.input3.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input5.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input5"), tr(mObjectLanguage, "menu.gui.button1.input6.placeholder"));
                    break;
                case MenuType::Custom:
                    form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input4.placeholder"));
                    break;
            }

            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(player, [mObjectLanguage, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editNew(pl);
                
                std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

                if (mObjectTitle.empty() || mObjectId.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::editNew(pl);
                }

                nlohmann::ordered_json mData = {
                    { "title", mObjectTitle },
                    { "info", nlohmann::ordered_json::object() },
                    { "permission", std::to_string((int) std::get<double>(dt->at("Slider"))) }
                };

                switch (type) {
                    case MenuType::Simple: {
                        mData.update({
                            { "content", std::get<std::string>(dt->at("Input3")) },
                            { "customize", nlohmann::ordered_json::array() },
                            { "type", "Simple" }
                        });
                        mData["info"].update({
                            { "exit", std::get<std::string>(dt->at("Input4")) },
                            { "score", std::get<std::string>(dt->at("Input5")) },
                            { "permission", std::get<std::string>(dt->at("Input6")) }
                        });
                        break;
                    }
                    case MenuType::Modal: {
                        mData.update({
                            { "content", std::get<std::string>(dt->at("Input3")) },
                            { "confirmButton", nlohmann::ordered_json::object() },
                            { "cancelButton", nlohmann::ordered_json::object() },
                            { "type", "Modal" }
                        });
                        mData["info"].update({
                            { "score", std::get<std::string>(dt->at("Input5")) },
                            { "permission", std::get<std::string>(dt->at("Input6")) }
                        });
                        break;
                    }
                    case MenuType::Custom: {
                        mData.update({
                            { "customize", nlohmann::ordered_json::array() },
                            { "run", nlohmann::ordered_json::array() },
                            { "type", "Custom" }
                        });
                        mData["info"].update({
                            { "exit", std::get<std::string>(dt->at("Input4")) }
                        });
                        break;
                    }
                }

                if (!db->has(mObjectId))
                    db->set(mObjectId, mData);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log1"), pl)), mObjectId);
            });
        }

        void editNew(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.button1.label"));
            form.appendButton("Simple", [](Player& pl) {
                MainGui::editNewInfo(pl, MenuType::Simple);
            });
            form.appendButton("Modal", [](Player& pl) {
                MainGui::editNewInfo(pl, MenuType::Modal);
            });
            form.appendButton("Custom", [](Player& pl) {
                MainGui::editNewInfo(pl, MenuType::Custom);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::edit(pl);
            });
        }

        void editRemoveInfo(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "menu.gui.button2.content")), id),
                tr(mObjectLanguage, "menu.gui.button2.yes"), tr(mObjectLanguage, "menu.gui.button2.no")
            );
            form.sendTo(player, [id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result != ll::form::ModalFormSelectedButton::Upper)
                    return;

                db->remove(id);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log2"), pl)), id);
            });
        }

        void editRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) -> void {
                    MainGui::editRemoveInfo(pl, key);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void editAwardSetting(Player& player, const std::string& id, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), tr(mObjectLanguage, "menu.gui.button1.input2.placeholder"), db->get_ptr<std::string>("/" + id + "/title", ""));

            switch (type) {
                case MenuType::Simple:
                    form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), tr(mObjectLanguage, "menu.gui.button1.input3.placeholder"), db->get_ptr<std::string>("/" + id + "/content", ""));
                    form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input4.placeholder"), db->get_ptr<std::string>("/" + id + "/info/exit", ""));
                    form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), tr(mObjectLanguage, "menu.gui.button1.input5.placeholder"), db->get_ptr<std::string>("/" + id + "/info/score", ""));
                    form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), tr(mObjectLanguage, "menu.gui.button1.input6.placeholder"), db->get_ptr<std::string>("/" + id + "/info/permission", ""));
                    break;
                case MenuType::Modal:
                    form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), tr(mObjectLanguage, "menu.gui.button1.input3.placeholder"), db->get_ptr<std::string>("/" + id + "/content", ""));
                    form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), tr(mObjectLanguage, "menu.gui.button1.input5.placeholder"), db->get_ptr<std::string>("/" + id + "/info/score", ""));
                    form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), tr(mObjectLanguage, "menu.gui.button1.input6.placeholder"), db->get_ptr<std::string>("/" + id + "/info/permission", ""));
                    break;
                case MenuType::Custom:
                    form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input4.placeholder"), db->get_ptr<std::string>("/" + id + "/info/exit", ""));
                    break;
            };

            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(player, [mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, id, type);

                std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

                if (mObjectTitle.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::editAwardContent(pl, id, type);
                }

                db->set_ptr("/" + id + "/title", mObjectTitle);

                switch (type) {
                    case MenuType::Simple:
                        db->set_ptr("/" + id + "/content", std::get<std::string>(dt->at("Input3")));
                        db->set_ptr("/" + id + "/info/exit", std::get<std::string>(dt->at("Input4")));
                        db->set_ptr("/" + id + "/info/score", std::get<std::string>(dt->at("Input5")));
                        db->set_ptr("/" + id + "/info/permission", std::get<std::string>(dt->at("Input6")));
                        break;
                    case MenuType::Modal:
                        db->set_ptr("/" + id + "/content", std::get<std::string>(dt->at("Input3")));
                        db->set_ptr("/" + id + "/info/score", std::get<std::string>(dt->at("Input5")));
                        db->set_ptr("/" + id + "/info/permission", std::get<std::string>(dt->at("Input6")));
                        break;
                    case MenuType::Custom:
                        db->set_ptr("/" + id + "/info/exit", std::get<std::string>(dt->at("Input4")));
                        break;
                }

                db->set_ptr("/" + id + "/permission", (int) std::get<double>(dt->at("Slider")));
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log5"), pl)), id);
            });
        }

        void editAwardNew(Player& player, const std::string& id, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));

            switch (type) {
                case MenuType::Simple:
                    form.appendInput("Input1", tr(mObjectLanguage, "menu.gui.button3.new.input1"), tr(mObjectLanguage, "menu.gui.button3.new.input1.placeholder"));
                    form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button3.new.input2"), tr(mObjectLanguage, "menu.gui.button3.new.input2.placeholder"));
                    form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button3.new.input3"), tr(mObjectLanguage, "menu.gui.button3.new.input3.placeholder"));
                    form.appendDropdown("dropdown1", tr(mObjectLanguage, "menu.gui.button3.new.dropdown1"), { "button", "from" });
                    form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button3.new.input4"), tr(mObjectLanguage, "menu.gui.button3.new.input4.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button3.new.input5"), tr(mObjectLanguage, "menu.gui.button3.new.input5.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button3.new.input6"), tr(mObjectLanguage, "menu.gui.button3.new.input6.placeholder"));
                    form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button3.new.slider"), 0, 4, 1, 0);
                    break;
                case MenuType::Modal:
                    form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button3.new.input2"), tr(mObjectLanguage, "menu.gui.button3.new.input2.placeholder"));
                    form.appendDropdown("dropdown1", tr(mObjectLanguage, "menu.gui.button3.new.dropdown1"), { "button", "from" });
                    form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button3.new.input4"), tr(mObjectLanguage, "menu.gui.button3.new.input4.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button3.new.input5"), tr(mObjectLanguage, "menu.gui.button3.new.input5.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button3.new.input6"), tr(mObjectLanguage, "menu.gui.button3.new.input6.placeholder"));
                    form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button3.new.slider"), 0, 4, 1, 0);
                    form.appendDropdown("dropdown2", tr(mObjectLanguage, "menu.gui.button3.new.dropdown2"), { "Upper", "Lower" });
                    break;
                default:
                    break;
            }
            form.sendTo(player, [mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, id, type);

                std::string mObjectObjective = std::get<std::string>(dt->at("Input4"));
                std::string mObjectScore = std::get<std::string>(dt->at("Input5"));
                std::string mObjectRun = std::get<std::string>(dt->at("Input6"));

                auto mData = db->get_ptr<nlohmann::ordered_json>("/" + id);

                switch (type) {
                    case MenuType::Simple: {
                        nlohmann::ordered_json data;

                        std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                        std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
                        std::string mObjectImage = std::get<std::string>(dt->at("Input3"));

                        if (mObjectTitle.empty() || mObjectImage.empty() || mObjectId.empty()) {
                            pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                            return MainGui::editAwardContent(pl, id, type);
                        }

                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "id", mObjectId },
                            { "scores", nlohmann::ordered_json::object() },
                            { "run", mObjectRun },
                            { "type", std::get<std::string>(dt->at("dropdown1")) },
                            { "permission", (int) std::get<double>(dt->at("Slider")) }
                        });

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        mData["customize"].push_back(data);
                        
                        break;
                    }
                    case MenuType::Modal: {
                        nlohmann::ordered_json data;

                        std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

                        if (mObjectTitle.empty()) {
                            pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                            return MainGui::editAwardContent(pl, id, type);
                        }

                        data.update({
                            { "title", mObjectTitle },
                            { "scores", nlohmann::ordered_json::object() },
                            { "run", mObjectRun },
                            { "type", std::get<std::string>(dt->at("dropdown1")) },
                            { "permission", (int) std::get<double>(dt->at("Slider")) }
                        });

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        (std::get<std::string>(dt->at("dropdown2")) == "Upper" ? mData["confirmButton"] : mData["cancelButton"]) = data;

                        break;
                    }
                    default:
                        break;
                };

                db->set_ptr("/" + id, mData);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log6"), pl)), id);
            });
        }

        void editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "menu.gui.button3.remove.content")), packageid),
                tr(mObjectLanguage, "menu.gui.button3.remove.yes"), tr(mObjectLanguage, "menu.gui.button3.remove.no")
            );
            form.sendTo(player, [id, packageid](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result != ll::form::ModalFormSelectedButton::Upper) 
                    return;

                auto mContent = db->get_ptr<nlohmann::ordered_json>("/" + id + "/customize");
                for (int i = ((int) mContent.size() - 1); i >= 0; i--) {
                    if (mContent.at(i).value("id", "") == packageid)
                        mContent.erase(i);
                }

                db->set_ptr("/" + id + "/customize", mContent);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log3"), pl)), id, packageid);
            });
        }

        void editAwardRemove(Player& player, const std::string& id, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (nlohmann::ordered_json& item : db->get_ptr<nlohmann::ordered_json>("/" + id + "/customize")) {
                std::string mName = item.value("id", "");

                form.appendButton(mName, [mName, id](Player& pl) -> void {
                    MainGui::editAwardRemoveInfo(pl, id, mName);
                });
            }
            form.sendTo(player, [ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::editAwardContent(pl, ids, type);
            });
        }

        void editAwardCommand(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));

            std::string mObjectLine = tr(mObjectLanguage, "menu.gui.button3.command.line");

            auto content = db->get_ptr<nlohmann::ordered_json>("/" + id + "/run");
            for (int i = 0; i < (int) content.size(); i++) {
                std::string mLine = fmt::format(fmt::runtime(mObjectLine), i + 1);
                form.appendInput("Content" + std::to_string(i), mLine, "", content.at(i));
            }

            form.appendStepSlider("StepSlider", tr(mObjectLanguage, "menu.gui.button3.command.operation"), { "no", "add", "remove" });
            form.sendTo(player, [id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, id, MenuType::Custom);

                auto content = db->get_ptr<nlohmann::ordered_json>("/" + id + "/run");
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

                db->set_ptr("/" + id + "/run", content);
                db->save();

                MainGui::editAwardCommand(pl, id);

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log4"), pl)), id);
            });
        }

        void editAwardContent(Player& player, const std::string& id, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "menu.gui.button3.label")), id)
            );
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3.setting"), "textures/ui/icon_setting", "path", [id, type](Player& pl) -> void {
                MainGui::editAwardSetting(pl, id, type);
            });

            switch (type) {
                case MenuType::Simple:
                    form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [id](Player& pl) -> void {
                        MainGui::editAwardNew(pl, id, MenuType::Simple);
                    });
                    form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [id](Player& pl) -> void {
                        MainGui::editAwardRemove(pl, id, MenuType::Simple);
                    });
                    break;
                case MenuType::Modal:
                    form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [id](Player& pl) -> void {
                        MainGui::editAwardNew(pl, id, MenuType::Modal);
                    });
                    break;
                case MenuType::Custom:
                    form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [id](Player& pl) -> void {
                        MainGui::editAwardRemove(pl, id, MenuType::Custom);
                    });
                    form.appendButton(tr(mObjectLanguage, "menu.gui.button3.command"), "textures/ui/creative_icon", "path", [id](Player& pl) -> void {
                        MainGui::editAwardCommand(pl, id);
                    });
            };

            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::editAward(pl);
            });
        }

        void editAward(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) -> void {
                    auto mObjectType = db->get_ptr<std::string>("/" + key + "/type", "Custom");

                    switch (ll::hash_utils::doHash(mObjectType)) {
                        case ll::hash_utils::doHash("Simple"):
                            MainGui::editAwardContent(pl, key, MenuType::Simple);
                            break;
                        case ll::hash_utils::doHash("Modal"):
                            MainGui::editAwardContent(pl, key, MenuType::Modal);
                            break;
                        case ll::hash_utils::doHash("Custom"):
                            MainGui::editAwardContent(pl, key, MenuType::Custom);
                            break;
                    };
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void edit(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            form.appendButton(tr(mObjectLanguage, "menu.gui.button1"), "textures/ui/achievements", "path", [](Player& pl) -> void {
                MainGui::editNew(pl);
            });
            form.appendButton(tr(mObjectLanguage, "menu.gui.button2"), "textures/ui/world_glyph_color", "path", [](Player& pl) -> void {
                MainGui::editRemove(pl);
            });
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3"), "textures/ui/editIcon", "path", [](Player& pl) -> void {
                MainGui::editAward(pl);
            });
            form.sendTo(player);
        }

        void custom(Player& player, nlohmann::ordered_json& data) {
            nlohmann::ordered_json mCustomData{};

            ll::form::CustomForm form(LOICollectionAPI::translateString(data.value("title", ""), player));
            for (nlohmann::ordered_json& customize : data.value("customize", nlohmann::ordered_json())) {
                switch (ll::hash_utils::doHash(customize.value("type", ""))) {
                    case ll::hash_utils::doHash("header"):
                        form.appendHeader(LOICollectionAPI::translateString(customize.value("title", ""), player));
                        break;
                    case ll::hash_utils::doHash("Label"):
                        form.appendLabel(LOICollectionAPI::translateString(customize.value("title", ""), player));
                        break;
                    case ll::hash_utils::doHash("divider"):
                        form.appendDivider();
                        break;
                    case ll::hash_utils::doHash("Input"): {
                        form.appendInput(
                            customize.value("id", "ID"),
                            LOICollectionAPI::translateString(customize.value("title", ""), player), 
                            customize.value("placeholder", ""),
                            customize.value("defaultValue", "")
                        );

                        mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", "");
                        break;
                    }
                    case ll::hash_utils::doHash("Dropdown"): {
                        std::vector<std::string> mOptions = customize.value("options", std::vector<std::string>());
                        if (mOptions.size() < 1)
                            break;

                        form.appendDropdown(
                            customize.value("id", "ID"),
                            LOICollectionAPI::translateString(customize.value("title", ""), player), 
                            mOptions,
                            customize.value("defaultValue", 0)
                        );

                        mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                        break;
                    }
                    case ll::hash_utils::doHash("Toggle"): {
                        form.appendToggle(
                            customize.value("id", "ID"),
                            LOICollectionAPI::translateString(customize.value("title", ""), player),
                            customize.value("defaultValue", false)
                        );

                        mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", false);
                        break;
                    }
                    case ll::hash_utils::doHash("Slider"): {
                        form.appendSlider(
                            customize.value("id", "ID"), 
                            LOICollectionAPI::translateString(customize.value("title", ""), player),
                            customize.value("min", 0),
                            customize.value("max", 100),
                            customize.value("step", 1),
                            customize.value("defaultValue", 0)
                        );

                        mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", 0);
                        break;
                    }
                    case ll::hash_utils::doHash("StepSlider"): {
                        std::vector<std::string> mOptions = customize.value("options", std::vector<std::string>());
                        if (mOptions.size() < 2)
                            break;

                        form.appendStepSlider(
                            customize.value("id", "ID"),
                            LOICollectionAPI::translateString(customize.value("title", ""), player),
                            mOptions,
                            customize.value("defaultValue", 0)
                        );

                        mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                        break;
                    }
                }
            }
            form.sendTo(player, [mCustomData, data](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));

                nlohmann::ordered_json mCustom;
                for (auto& item : mCustomData.items()) {
                    if (item.value().is_string()) {
                        std::string result = std::get<std::string>(dt->at(item.key()));
                        if (!result.empty())
                            mCustom[item.key()] = result;
                    }
                    if (item.value().is_boolean()) mCustom[item.key()] = std::get<uint64>(dt->at(item.key())) ? "true" : "false";
                    if (item.value().is_number_integer()) mCustom[item.key()] = std::to_string((int) std::get<double>(dt->at(item.key())));
                }

                for (const auto& c_it : data.value("run", nlohmann::ordered_json())) {
                    std::string result = c_it.get<std::string>();
                    for (auto& item : mCustom.items()) {
                        if (result.find("{" + item.key() + "}") == std::string::npos)
                            continue;
                        ll::string_utils::replaceAll(result, "{" + item.key() + "}", item.value().get<std::string>());
                    }
                    executeCommand(pl, result);
                }
            });
        }

        void simple(Player& player, nlohmann::ordered_json& data) {
            ll::form::SimpleForm form(LOICollectionAPI::translateString(data.value("title", ""), player), LOICollectionAPI::translateString(data.value("content", ""), player));
            for (nlohmann::ordered_json& customize : data.value("customize", nlohmann::ordered_json())) {
                switch (ll::hash_utils::doHash(customize.value("type", ""))) {
                    case ll::hash_utils::doHash("button"):
                    case ll::hash_utils::doHash("from"):
                        form.appendButton(LOICollectionAPI::translateString(customize.value("title", ""), player), customize.value("image", ""), "path", [data, customize](Player& pl) -> void {
                            handleAction(pl, customize, data);
                        });
                        break;
                    case ll::hash_utils::doHash("header"):
                        form.appendHeader(LOICollectionAPI::translateString(customize.value("title", ""), player));
                        break;
                    case ll::hash_utils::doHash("label"): 
                        form.appendLabel(LOICollectionAPI::translateString(customize.value("title", ""), player));
                        break;
                    case ll::hash_utils::doHash("divider"):
                        form.appendDivider();
                        break;
                }
            }
            form.sendTo(player, [data](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));
            });
        }

        void modal(Player& player, nlohmann::ordered_json& data) {
            nlohmann::ordered_json mConfirmButton = data.value("confirmButton", nlohmann::ordered_json());
            nlohmann::ordered_json mCancelButton = data.value("cancelButton", nlohmann::ordered_json());
            if (mCancelButton.empty() || mConfirmButton.empty())
                return;

            ll::form::ModalForm form(
                LOICollectionAPI::translateString(data.value("title", ""), player),
                LOICollectionAPI::translateString(data.value("content", ""), player),
                LOICollectionAPI::translateString(mConfirmButton.value("title", ""), player),
                LOICollectionAPI::translateString(mCancelButton.value("title", ""), player)
            );
            form.sendTo(player, [data, mConfirmButton, mCancelButton](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result == ll::form::ModalFormSelectedButton::Upper) 
                    return handleAction(pl, mConfirmButton, data);
                handleAction(pl, mCancelButton, data);
            });
        }

        void open(Player& player, std::string id) {
            if (db->has(id)) {
                auto data = db->get_ptr<nlohmann::ordered_json>("/" + id);
                
                if (data.empty()) return;
                if (data.contains("permission")) {
                    if ((int) player.getCommandPermissionLevel() < data.value("permission", 0))
                        return executeCommand(player, data.value("info", nlohmann::ordered_json{}).value("permission", ""));
                }
                
                switch (ll::hash_utils::doHash(data.value("type", ""))) {
                    case ll::hash_utils::doHash("Custom"):
                        custom(player, data);
                        break;
                    case ll::hash_utils::doHash("Simple"):
                        simple(player, data);
                        break;
                    case ll::hash_utils::doHash("Modal"):
                        modal(player, data);
                        break;
                    default:
                        logger->error("Unknown UI type {}.", data.value("type", ""));
                        break;
                }

                return;
            }

            logger->error("MenuUI {} reading failed.", id);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("menu", tr({}, "commands.menu.description"), CommandPermissionLevel::Any);
            command.overload<MenuOP>().text("gui").optional("Id").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MenuOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::open(player, param.Id.empty() ? 
                    options.EntranceKey : param.Id
                );
                
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
            command.overload().text("clock").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                ItemStack itemStack(options.MenuItemId, 1, 0, nullptr);
                if (!itemStack || itemStack.isNull())
                    return output.error(tr({}, "commands.menu.error.item.null"));
                if (InventoryUtils::isItemInInventory(player, options.MenuItemId, 1))
                    return output.error(fmt::runtime(tr({}, "commands.menu.error.item.give")), player.getRealName());

                InventoryUtils::giveItem(player, itemStack, 1);
                player.refreshInventory();

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([](ll::event::PlayerUseItemEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;
                if (event.item().getTypeName() == options.MenuItemId)
                    MainGui::open(event.self(), "main");
            });
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                ItemStack itemStack(options.MenuItemId, 1, 0, nullptr);
                if (!itemStack || InventoryUtils::isItemInInventory(event.self(), options.MenuItemId, 1))
                    return;
                InventoryUtils::giveItem(event.self(), itemStack, 1);
                event.self().refreshInventory();
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerUseItemEventListener);
            eventBus.removeListener(PlayerJoinEventListener);
        }
    }

    void executeCommand(Player& player, std::string cmd) {
        ll::string_utils::replaceAll(cmd, "${player}", std::string(player.mName));

        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        Command* command = ll::service::getMinecraft()->mCommands->compileCommand(
            HashedString(cmd), origin, (CurrentCmdVersion)CommandVersion::CurrentVersion(),
            [&](std::string const& message) -> void {
                logger->error(message + " >> Menu");
            }
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    void handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original) {
        if (action.empty() || !isValid())
            return;

        if (action.contains("permission")) {
            if ((int) player.getCommandPermissionLevel() < action["permission"])
                return executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("permission", ""));
        }

        if (action.contains("scores")) {
            for (const auto& [key, value] : action["scores"].items()) {
                if (value.get<int>() > ScoreboardUtils::getScore(player, key))
                    return executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("score", ""));
            }
            for (const auto& [key, value] : action["scores"].items())
                ScoreboardUtils::reduceScore(player, key, value.get<int>());
        }

        if (action.value("type", "") == "button") {
            if (action["run"].is_string())
                return executeCommand(player, action["run"].get<std::string>());
            for (const auto& cmd : action["run"])
                executeCommand(player, cmd.get<std::string>());
            return;
        }
        if (action.value("type", "") == "from")
            MainGui::open(player, action.value("run", ""));
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {     
        db = std::move(*static_cast<std::unique_ptr<JsonStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        options = Config::GetBaseConfigContext().Plugins.Menu;
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->save();
    }
}