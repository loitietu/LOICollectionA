#include <memory>
#include <vector>
#include <string>

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
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "include/menuPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;
using LOICollection::LOICollectionAPI::translateString;

namespace LOICollection::Plugins::menu {
    struct MenuOP {
        std::string uiName;
    };

    std::map<std::string, std::string> mObjectOptions;
    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerUseItemEventListener;

    namespace MainGui {
        void editNew(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "menu.gui.button1.input1"), "", "None");
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), "", "Menu Example");
            form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), "", "This is a menu example");
            form.appendDropdown("dropdown", tr(mObjectLanguage, "menu.gui.button1.dropdown"), { "Custom", "Simple", "Modal" });
            form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), "", "execute as ${player} run say Exit Menu");
            form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), "", "execute as ${player} run say You do not have permission to use this button");
            form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), "", "execute as ${player} run say You do not have enough score to use this button");
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::edit(pl);

                std::string mObjectInput1 = std::get<std::string>(dt->at("Input1"));
                std::string mObjectInput2 = std::get<std::string>(dt->at("Input2"));
                std::string mObjectInput3 = std::get<std::string>(dt->at("Input3"));
                std::string mObjectInput4 = std::get<std::string>(dt->at("Input4"));
                std::string mObjectInput5 = std::get<std::string>(dt->at("Input5"));
                std::string mObjectInput6 = std::get<std::string>(dt->at("Input6"));
                std::string mObjectDropdown = std::get<std::string>(dt->at("dropdown"));
                int mObjectSlider = (int) std::get<double>(dt->at("Slider"));

                nlohmann::ordered_json mData;
                switch (ll::hash_utils::doHash(mObjectDropdown)) {
                    case ll::hash_utils::doHash("Simple"):
                        mData["title"] = mObjectInput6;
                        mData["content"] = mObjectInput5;
                        mData["exit"] = mObjectInput2;
                        mData["NoPermission"] = mObjectInput3;
                        mData["NoScore"] = mObjectInput4;
                        mData["type"] = "Simple";
                        mData["button"] = nlohmann::ordered_json::array();
                        mData["permission"] = mObjectSlider;
                        break;
                    case ll::hash_utils::doHash("Modal"):
                        mData["title"] = mObjectInput6;
                        mData["content"] = mObjectInput5;
                        mData["NoPermission"] = mObjectInput3;
                        mData["NoScore"] = mObjectInput4;
                        mData["type"] = "Modal";
                        mData["confirmButton"] = nlohmann::ordered_json::object();
                        mData["cancelButton"] = nlohmann::ordered_json::object();
                        mData["permission"] = mObjectSlider;
                        break;
                    case ll::hash_utils::doHash("Custom"):
                        mData["title"] = mObjectInput6;
                        mData["exit"] = mObjectInput2;
                        mData["type"] = "Custom";
                        mData["customize"] = nlohmann::ordered_json::array();
                        mData["command"] = nlohmann::ordered_json::array();
                        mData["permission"] = mObjectSlider;
                        break;
                };
                if (!db->has(mObjectInput1))
                    db->set(mObjectInput1, mData);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log1"), "${menu}", mObjectInput1), pl));
            });
        }

        void editRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (auto& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    std::string mObjectLanguage = getLanguage(pl);
                    std::string mObjectContent = tr(mObjectLanguage, "menu.gui.button2.content");
                    
                    ll::string_utils::replaceAll(mObjectContent, "${menu}", key);
                    ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "menu.gui.button2.yes"), tr(mObjectLanguage, "menu.gui.button2.no"));
                    form.sendTo(pl, [key](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                        if (result == ll::form::ModalFormSelectedButton::Upper) {
                            db->remove(key);
                            db->save();

                            logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log2"), "${menu}", key), pl));
                        }
                    });
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void editAwardSetting(Player& player, std::string uiName, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);

            nlohmann::ordered_json data = db->toJson(uiName);
            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), "", data.at("title"));
            if (type != MenuType::Custom) {
                form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), "", data.at("content"));
                form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), "", data.at("NoScore"));
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), "", data.at("NoPermission"));
            }
            if (type != MenuType::Modal)
                form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), "", data.at("exit"));
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(player, [uiName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::editAwardContent(pl, uiName);

                nlohmann::ordered_json data = db->toJson(uiName);
                data["title"] = std::get<std::string>(dt->at("Input6"));
                switch (type) {
                    case MenuType::Simple:
                        data["content"] = std::get<std::string>(dt->at("Input5"));
                        data["exit"] = std::get<std::string>(dt->at("Input2"));
                        data["NoScore"] = std::get<std::string>(dt->at("Input4"));
                        data["NoPermission"] = std::get<std::string>(dt->at("Input3"));
                        break;
                    case MenuType::Modal:
                        data["content"] = std::get<std::string>(dt->at("Input5"));
                        data["NoScore"] = std::get<std::string>(dt->at("Input4"));
                        data["NoPermission"] = std::get<std::string>(dt->at("Input3"));
                        break;
                    case MenuType::Custom:
                        data["exit"] = std::get<std::string>(dt->at("Input2"));
                        break;
                }
                data["permission"] = (int) std::get<double>(dt->at("Slider"));
                db->set(uiName, data);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log5"), "${menu}", uiName), pl));
            });
        }

        void editAwardNew(Player& player, std::string uiName, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            if (type == MenuType::Simple)
                form.appendInput("Input1", tr(mObjectLanguage, "menu.gui.button3.new.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button3.new.input2"), "", "Title");
            if (type == MenuType::Simple)
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button3.new.input3"), "", "textures/items/diamond");
            form.appendDropdown("dropdown1", tr(mObjectLanguage, "menu.gui.button3.new.dropdown1"), { "button", "from" });
            form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button3.new.input4"), "", "money");
            form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button3.new.input5"), "", "100");
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button3.new.input6"), "", "");
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button3.new.slider"), 0, 4, 1, 0);
            if (type == MenuType::Modal)
                form.appendDropdown("dropdown2", tr(mObjectLanguage, "menu.gui.button3.new.dropdown2"), { "Upper", "Lower" });
            form.sendTo(player, [uiName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::editAwardContent(pl, uiName);

                std::string mObjectInput4 = std::get<std::string>(dt->at("Input4"));
                std::string mObjectInput5 = std::get<std::string>(dt->at("Input5"));
                std::string mObjectType = std::get<std::string>(dt->at("dropdown1"));

                nlohmann::ordered_json data;
                data["title"] = std::get<std::string>(dt->at("Input2"));
                if (type == MenuType::Simple) {
                    std::string mObjectInput1 = std::get<std::string>(dt->at("Input1"));
                    data["image"] = std::get<std::string>(dt->at("Input3"));
                    data["id"] = mObjectInput1.empty() ? "ID" : mObjectInput1;
                }
                data["scores"] = nlohmann::ordered_json::object();
                if (!mObjectInput4.empty())
                    data["scores"][mObjectInput4] = SystemUtils::toInt((mObjectInput5.empty() ? "100" : mObjectInput5), 0);
                (mObjectType == "button" ? data["command"] : data["menu"]) = std::get<std::string>(dt->at("Input6"));
                data["type"] = mObjectType;
                data["permission"] = (int) std::get<double>(dt->at("Slider"));

                nlohmann::ordered_json mContent = db->toJson(uiName);
                type == MenuType::Simple ? mContent["button"].push_back(data)
                    : (void)((std::get<std::string>(dt->at("dropdown2")) == "Upper" ? mContent["confirmButton"] : mContent["cancelButton"]) = data);
                db->set(uiName, mContent);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log6"), "${menu}", uiName), pl));
            });
        }

        void editAwardRemove(Player& player, std::string uiName) {
            std::string mObjectLanguage = getLanguage(player);

            nlohmann::ordered_json data = db->toJson(uiName);
            nlohmann::ordered_json mContent = (data.at("type").get<std::string>() == "Custom" ? data.at("customize") : data.at("button"));
            
            std::vector<std::string> mObjectItems;
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (auto& item : mContent) {
                std::string mName = item.at("id").get<std::string>();
                form.appendButton(mName);
                mObjectItems.push_back(mName);
            }
            form.sendTo(player, [uiName, mObjectItems](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) return MainGui::editAwardContent(pl, uiName);

                std::string mId = mObjectItems.at(id);
                std::string mObjectLanguage = getLanguage(pl);
                std::string mObjectContent = tr(mObjectLanguage, "menu.gui.button3.remove.content");
                ll::string_utils::replaceAll(mObjectContent, "${customize}", mId);
                ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                    tr(mObjectLanguage, "menu.gui.button3.remove.yes"), tr(mObjectLanguage, "menu.gui.button3.remove.no"));
                form.sendTo(pl, [uiName, mId](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                    if (result != ll::form::ModalFormSelectedButton::Upper) 
                        return;
                    nlohmann::ordered_json data = db->toJson(uiName);
                    nlohmann::ordered_json mContent = (data.at("type").get<std::string>() == "Custom" ? data.at("customize") : data.at("button"));
                    for (int i = 0; i < (int) mContent.size(); i++) {
                        if (mContent.at(i).at("id").get<std::string>() == mId)
                            mContent.erase(i);
                    }
                    data.at("type").get<std::string>() == "Custom" ? data.at("customize") = mContent : data.at("button") = mContent;
                    db->set(uiName, data);
                    db->save();

                    std::string logString = tr({}, "menu.log3");
                    ll::string_utils::replaceAll(logString, "${menu}", uiName);
                    ll::string_utils::replaceAll(logString, "${customize}", mId);
                    logger->info(translateString(logString, pl));
                });
            });
        }

        void editAwardCommand(Player& player, std::string uiName) {
            int index = 1;

            std::string mObjectLanguage = getLanguage(player); 
            std::string mObjectLineString = tr(mObjectLanguage, "menu.gui.button3.command.line");
            nlohmann::ordered_json data = db->toJson(uiName);

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            for (auto& item : data.at("command")) {
                std::string mLineString = mObjectLineString;
                std::string mLine = ll::string_utils::replaceAll(mLineString, "${index}", std::to_string(index));
                form.appendInput("Input" + std::to_string(index), mLine, "", item);
                index++;
            }
            form.appendToggle("Toggle1", tr(mObjectLanguage, "menu.gui.button3.command.addLine"));
            form.appendToggle("Toggle2", tr(mObjectLanguage, "menu.gui.button3.command.removeLine"));
            form.sendTo(player, [index, uiName](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::editAwardContent(pl, uiName);

                nlohmann::ordered_json data = db->toJson(uiName);
                nlohmann::ordered_json mCommand = data.at("command");
                if (std::get<uint64>(dt->at("Toggle1"))) {
                    mCommand.push_back("");
                    data["command"] = mCommand;
                    db->set(uiName, data);
                    db->save();
                    return MainGui::editAwardCommand(pl, uiName);
                } else if (std::get<uint64>(dt->at("Toggle2"))) {
                    mCommand.erase(mCommand.end() - 1);
                    data["command"] = mCommand;
                    db->set(uiName, data);
                    db->save();
                    return MainGui::editAwardCommand(pl, uiName);
                }
                for (int i = 1; i < index; i++)
                    mCommand.at(i - 1) = std::get<std::string>(dt->at("Input" + std::to_string(i)));
                data["command"] = mCommand;
                db->set(uiName, data);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log4"), "${menu}", uiName), pl));
            });
        }

        void editAwardContent(Player& player, std::string uiName) {
            std::string mObjectLanguage = getLanguage(player);
            std::string mObjectLabel = tr(mObjectLanguage, "menu.gui.button3.label");
            std::string mObjectType = db->toJson(uiName).at("type").get<std::string>();

            ll::string_utils::replaceAll(mObjectLabel, "${menu}", uiName);
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3.setting"), "textures/ui/icon_setting", "path", [uiName, mObjectType](Player& pl) {
                switch (ll::hash_utils::doHash(mObjectType)) {
                    case ll::hash_utils::doHash("Simple"):
                        MainGui::editAwardSetting(pl, uiName, MenuType::Simple);
                        break;
                    case ll::hash_utils::doHash("Modal"):
                        MainGui::editAwardSetting(pl, uiName, MenuType::Modal);
                        break;
                    case ll::hash_utils::doHash("Custom"):
                        MainGui::editAwardSetting(pl, uiName, MenuType::Custom);
                        break;
                };
            });
            if (mObjectType != "Custom") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [uiName, mObjectType](Player& pl) {
                    if (mObjectType == "Simple")
                        return MainGui::editAwardNew(pl, uiName, MenuType::Simple);
                    MainGui::editAwardNew(pl, uiName, MenuType::Modal);
                });
            }
            if (mObjectType != "Modal") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [uiName](Player& pl) {
                    MainGui::editAwardRemove(pl, uiName);
                });
            }
            if (mObjectType == "Custom") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.command"), "textures/ui/creative_icon", "path", [uiName](Player& pl) {
                    MainGui::editAwardCommand(pl, uiName);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::editAward(pl);
            });
        }

        void editAward(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (auto& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    MainGui::editAwardContent(pl, key);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void edit(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            form.appendButton(tr(mObjectLanguage, "menu.gui.button1"), "textures/ui/achievements", "path", [](Player& pl) {
                MainGui::editNew(pl);
            });
            form.appendButton(tr(mObjectLanguage, "menu.gui.button2"), "textures/ui/world_glyph_color", "path", [](Player& pl) {
                MainGui::editRemove(pl);
            });
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3"), "textures/ui/editIcon", "path", [](Player& pl) {
                MainGui::editAward(pl);
            });
            form.sendTo(player);
        }

        void custom(Player& player, nlohmann::ordered_json& data) {
            nlohmann::ordered_json mCustomData;
            ll::form::CustomForm form(translateString(data.at("title").get<std::string>(), player));
            for (auto& customize : data.at("customize")) {
                switch (ll::hash_utils::doHash(customize.at("type").get<std::string>())) {
                    case ll::hash_utils::doHash("Label"):
                        form.appendLabel(translateString(customize.at("title").get<std::string>(), player));
                        break;
                    case ll::hash_utils::doHash("Input"): {
                        form.appendInput(customize.at("id").get<std::string>(),
                            translateString(customize.at("title").get<std::string>(), player), 
                            customize.at("placeholder").get<std::string>(), customize.at("defaultValue").get<std::string>());
                        mCustomData[customize.at("id").get<std::string>()] = customize.at("defaultValue").get<std::string>();
                        break;
                    }
                    case ll::hash_utils::doHash("Dropdown"): {
                        std::vector<std::string> mOptions = customize.at("options").get<std::vector<std::string>>();
                        if (mOptions.size() < 1)
                            break;
                        form.appendDropdown(customize.at("id").get<std::string>(),
                            translateString(customize.at("title").get<std::string>(), player), 
                            mOptions, customize.at("defaultValue").get<int>());
                        mCustomData[customize.at("id").get<std::string>()] = mOptions.at(customize.at("defaultValue").get<int>());
                        break;
                    }
                    case ll::hash_utils::doHash("Toggle"): {
                        form.appendToggle(customize.at("id").get<std::string>(),
                            translateString(customize.at("title").get<std::string>(), player),
                            customize.at("defaultValue").get<bool>());
                        mCustomData[customize.at("id").get<std::string>()] = customize.at("defaultValue").get<bool>();
                        break;
                    }
                    case ll::hash_utils::doHash("Slider"): {
                        form.appendSlider(customize.at("id").get<std::string>(), translateString(customize.at("title").get<std::string>(), player),
                        customize.at("min").get<int>(), customize.at("max").get<int>(), customize.at("step").get<int>(), customize.at("defaultValue").get<int>());
                        mCustomData[customize.at("id").get<std::string>()] = customize.at("defaultValue").get<int>();
                        break;
                    }
                    case ll::hash_utils::doHash("StepSlider"): {
                        std::vector<std::string> mOptions = customize.at("options").get<std::vector<std::string>>();
                        if (mOptions.size() < 2)
                            break;
                        form.appendStepSlider(customize.at("id").get<std::string>(),
                            translateString(customize.at("title").get<std::string>(), player),
                        mOptions, customize.at("defaultValue").get<int>());
                        mCustomData[customize.at("id").get<std::string>()] = mOptions.at(customize.at("defaultValue").get<int>());
                        break;
                    }
                }
            }
            form.sendTo(player, [mCustomData, data](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return McUtils::executeCommand(pl, data.at("exit").get<std::string>());

                nlohmann::ordered_json mCustom;
                for (auto& [key, value] : mCustomData.items()) {
                    if (value.is_string()) {
                        std::string result = std::get<std::string>(dt->at(key));
                        if (!result.empty())
                            mCustom[key] = result;
                    }
                    if (value.is_boolean()) mCustom[key] = std::get<uint64>(dt->at(key)) ? "true" : "false";
                    if (value.is_number_integer()) mCustom[key] = std::to_string((int) std::get<double>(dt->at(key)));
                }
                for (auto c_it = data["command"].begin(); c_it != data["command"].end(); ++c_it) {
                    std::string result = *c_it;
                    for (auto& [key, value] : mCustom.items()) {
                        if (result.find("{" + key + "}") == std::string::npos)
                            continue;
                        ll::string_utils::replaceAll(result, "{" + key + "}", value.get<std::string>());
                    }
                    McUtils::executeCommand(pl, result);
                }
            });
        }

        void simple(Player& player, nlohmann::ordered_json& data) {
            ll::form::SimpleForm form(translateString(data.at("title").get<std::string>(), player),
                translateString(data.at("content").get<std::string>(), player));
            for (auto& button : data.at("button")) {
                form.appendButton(translateString(button.at("title").get<std::string>(), player), 
                    button.at("image").get<std::string>(), "path", [data, button](Player& pl) {
                    nlohmann::ordered_json mButton = button;
                    logicalExecution(pl, mButton, data);
                });
            }
            form.sendTo(player, [data](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) return McUtils::executeCommand(pl, data.at("exit").get<std::string>());
            });
        }

        void modal(Player& player, nlohmann::ordered_json& data) {
            if (data.at("confirmButton").empty() || data.at("cancelButton").empty())
                return;

            ll::form::ModalForm form(translateString(data.at("title").get<std::string>(), player),
                translateString(data.at("content").get<std::string>(), player),
                translateString(data.at("confirmButton").at("title").get<std::string>(), player),
                translateString(data.at("cancelButton").at("title").get<std::string>(), player));
            form.sendTo(player, [data](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    nlohmann::ordered_json mButton = data.at("confirmButton");
                    logicalExecution(pl, mButton, data);
                    return;
                }
                nlohmann::ordered_json mButton = data.at("cancelButton");
                logicalExecution(pl, mButton, data);
            });
        }

        void open(Player& player, std::string uiName) {
            if (db->has(uiName)) {
                nlohmann::ordered_json data = db->toJson(uiName);
                if (data.empty()) return;
                if (data.contains("permission")) {
                    if ((int) player.getPlayerPermissionLevel() < data["permission"].get<int>())
                        return McUtils::executeCommand(player, data.at("NoPermission").get<std::string>());
                }
                
                switch (ll::hash_utils::doHash(data.at("type").get<std::string>())) {
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
                        logger->error("Unknown UI type {}.", data.at("type").get<std::string>());
                        break;
                }
                return;
            }
            logger->error("MenuUI {} reading failed.", uiName);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("menu", "§e§lLOICollection -> §b服务器菜单", CommandPermissionLevel::Any);
            command.overload<MenuOP>().text("gui").optional("uiName").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MenuOP param) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player, param.uiName.empty() ? 
                    mObjectOptions.at("EntranceKey") : param.uiName
                );
                
                output.success("The UI has been opened to player {}", player.getRealName());
            });
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                if ((int) player.getPlayerPermissionLevel() >= 2) {
                    MainGui::edit(player);
                    output.success("The UI has been opened to player {}", player.getRealName());
                    return;
                }

                output.error("No permission to open the ui.");
            });
            command.overload().text("clock").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                ItemStack itemStack(mObjectOptions.at("MenuItemId"), 1, 0, nullptr);

                if (!itemStack || itemStack.isNull())
                    return output.error("Failed to give the MenuItem to player {}", player.getRealName());
                if (McUtils::isItemPlayerInventory(player, mObjectOptions.at("MenuItemId"), 1))
                    return output.error("The MenuItem has already been given to player {}", player.getRealName());

                player.add(itemStack);
                player.refreshInventory();

                output.success("The MenuItem has been given to player {}", player.getRealName());
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>(
                [](ll::event::PlayerUseItemEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    if (event.item().getTypeName() == mObjectOptions.at("MenuItemId"))
                        MainGui::open(event.self(), "main");
                }
            );
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    ItemStack itemStack(mObjectOptions.at("MenuItemId"), 1, 0, nullptr);
                    if (!itemStack || McUtils::isItemPlayerInventory(event.self(), mObjectOptions.at("MenuItemId"), 1))
                        return;
                    event.self().add(itemStack);
                    event.self().refreshInventory();
                }
            );
        }
    }

    void logicalExecution(Player& player, nlohmann::ordered_json& data, nlohmann::ordered_json original) {
        if (data.empty() || !isValid()) 
            return;

        if (data.contains("permission")) {
            if ((int) player.getPlayerPermissionLevel() < data["permission"].get<int>())
                return McUtils::executeCommand(player, original.at("NoPermission").get<std::string>());
        }
        if (data.at("type").get<std::string>() == "button") {
            if (!checkModifiedData(player, data))
                return McUtils::executeCommand(player, original.at("NoScore").get<std::string>());
            if (data.at("command").is_string())
                return McUtils::executeCommand(player, data.at("command").get<std::string>());
            for (auto& command : data.at("command"))
                McUtils::executeCommand(player, command.get<std::string>());
        } else if (data.at("type").get<std::string>() == "from") {
            if (!checkModifiedData(player, data))
                return McUtils::executeCommand(player, original.at("NoScore").get<std::string>());
            MainGui::open(player, data.at("menu").get<std::string>());
        }
    }

    bool checkModifiedData(Player& player, nlohmann::ordered_json& data) {
        if (!data.contains("scores") || !isValid()) 
            return true;

        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if (it.value().get<int>() > McUtils::scoreboard::getScore(player, it.key()))
                return false;
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            McUtils::scoreboard::reduceScore(player, it.key(), it.value().get<int>());
        return true;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database, std::map<std::string, std::string>& options) {     
        db = std::move(*static_cast<std::unique_ptr<JsonStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        mObjectOptions = options;
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerUseItemEventListener);
        eventBus.removeListener(PlayerJoinEventListener);
    }
}