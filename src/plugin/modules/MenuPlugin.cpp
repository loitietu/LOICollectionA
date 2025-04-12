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
#include "include/LanguagePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "include/MenuPlugin.h"

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
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
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
            for (std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) -> void {
                    std::string mObjectLanguage = getLanguage(pl);
                    std::string mObjectContent = tr(mObjectLanguage, "menu.gui.button2.content");
                    
                    ll::string_utils::replaceAll(mObjectContent, "${menu}", key);
                    ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "menu.gui.button2.yes"), tr(mObjectLanguage, "menu.gui.button2.no"));
                    form.sendTo(pl, [key](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                        if (result == ll::form::ModalFormSelectedButton::Upper) {
                            db->remove(key);
                            db->save();

                            logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log2"), "${menu}", key), pl));
                        }
                    });
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void editAwardSetting(Player& player, const std::string& uiName, MenuType type) {
            std::string mObjectLanguage = getLanguage(player);

            nlohmann::ordered_json data = db->toJson(uiName);
            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), "", data.value("title", ""));
            if (type != MenuType::Custom) {
                form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), "", data.value("content", ""));
                form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), "", data.value("NoScore", ""));
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), "", data.value("NoPermission", ""));
            }
            if (type != MenuType::Modal)
                form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), "", data.value("exit", ""));
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(player, [uiName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
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

        void editAwardNew(Player& player, const std::string& uiName, MenuType type) {
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
            form.sendTo(player, [uiName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
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
                    : (void)((std::get<std::string>(dt->at("dropdown2")) == "Upper" ?
                        mContent.at("confirmButton") : mContent.at("cancelButton")) = data);
                db->set(uiName, mContent);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log6"), "${menu}", uiName), pl));
            });
        }

        void editAwardRemove(Player& player, const std::string& uiName) {
            std::string mObjectLanguage = getLanguage(player);

            nlohmann::ordered_json defaultJson{};
            nlohmann::ordered_json data = db->toJson(uiName);
            nlohmann::ordered_json mContent = (data.value("type", "") == "Custom" ?
                data.value("customize", defaultJson) : data.value("button", defaultJson));
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (nlohmann::ordered_json& item : mContent) {
                std::string mName = item.value("id", "");
                form.appendButton(mName, [mName, uiName](Player& pl) -> void {
                    std::string mObjectLanguage = getLanguage(pl);

                    std::string mObjectContent = tr(mObjectLanguage, "menu.gui.button3.remove.content");
                    ll::string_utils::replaceAll(mObjectContent, "${customize}", mName);
                    
                    ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "menu.gui.button3.remove.yes"),
                        tr(mObjectLanguage, "menu.gui.button3.remove.no")
                    );
                    form.sendTo(pl, [uiName, mName](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                        if (result != ll::form::ModalFormSelectedButton::Upper) 
                            return;
                        nlohmann::ordered_json defaultJson{};
                        nlohmann::ordered_json data = db->toJson(uiName);
                        nlohmann::ordered_json mContent = (data.value("type", "") == "Custom" ?
                            data.value("customize", defaultJson) : data.value("button", defaultJson));
                        for (int i = ((int) mContent.size() - 1); i >= 0; i--) {
                            if (mContent.at(i).value("id", "") == mName)
                                mContent.erase(i);
                        }
                        (data.value("type", "") == "Custom" ? data["customize"] : data["button"]) = mContent;
                        db->set(uiName, data);
                        db->save();

                        std::string logString = tr({}, "menu.log3");
                        ll::string_utils::replaceAll(logString, "${menu}", uiName);
                        ll::string_utils::replaceAll(logString, "${customize}", mName);
                        logger->info(translateString(logString, pl));
                    });
                });
            }
            form.sendTo(player, [uiName](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::editAwardContent(pl, uiName);
            });
        }

        void editAwardCommand(Player& player, const std::string& uiName) {
            std::string mObjectLanguage = getLanguage(player); 

            nlohmann::ordered_json data = db->toJson(uiName);
            nlohmann::ordered_json content = data.value("command", nlohmann::ordered_json());

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            for (int i = 0; i < (int) content.size(); i++) {
                std::string mLine = ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "menu.gui.button3.command.line"), "${index}", std::to_string(i + 1)
                );
                form.appendInput("Content" + std::to_string(i), mLine, "", content.at(i));
            }
            form.appendToggle("Toggle1", tr(mObjectLanguage, "menu.gui.button3.command.addLine"));
            form.appendToggle("Toggle2", tr(mObjectLanguage, "menu.gui.button3.command.removeLine"));
            form.sendTo(player, [uiName](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, uiName);

                nlohmann::ordered_json data = db->toJson(uiName);
                nlohmann::ordered_json content = data.value("command", nlohmann::ordered_json());

                if (std::get<uint64>(dt->at("Toggle1")))
                    content.push_back("");
                else if (std::get<uint64>(dt->at("Toggle2")))
                    content.erase(content.end() - 1);
                else {
                    for (int i = 0; i < (int) content.size(); i++)
                        content.at(i) = std::get<std::string>(dt->at("Content" + std::to_string(i)));
                }
                data["command"] = content;

                db->set(uiName, data);
                db->save();

                MainGui::editAwardCommand(pl, uiName);

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log4"), "${menu}", uiName), pl));
            });
        }

        void editAwardContent(Player& player, const std::string& uiName) {
            std::string mObjectLanguage = getLanguage(player);
            std::string mObjectLabel = tr(mObjectLanguage, "menu.gui.button3.label");
            std::string mObjectType = db->toJson(uiName).at("type").get<std::string>();

            ll::string_utils::replaceAll(mObjectLabel, "${menu}", uiName);
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3.setting"), "textures/ui/icon_setting", "path", [uiName, mObjectType](Player& pl) -> void {
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
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [uiName, mObjectType](Player& pl) -> void {
                    if (mObjectType == "Simple")
                        return MainGui::editAwardNew(pl, uiName, MenuType::Simple);
                    MainGui::editAwardNew(pl, uiName, MenuType::Modal);
                });
            }
            if (mObjectType != "Modal") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [uiName](Player& pl) -> void {
                    MainGui::editAwardRemove(pl, uiName);
                });
            }
            if (mObjectType == "Custom") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.command"), "textures/ui/creative_icon", "path", [uiName](Player& pl) -> void {
                    MainGui::editAwardCommand(pl, uiName);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::editAward(pl);
            });
        }

        void editAward(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) -> void {
                    MainGui::editAwardContent(pl, key);
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
            ll::form::CustomForm form(translateString(data.value("title", ""), player));
            for (nlohmann::ordered_json& customize : data.value("customize", nlohmann::ordered_json())) {
                switch (ll::hash_utils::doHash(customize.value("type", ""))) {
                    case ll::hash_utils::doHash("Label"):
                        form.appendLabel(translateString(customize.value("title", ""), player));
                        break;
                    case ll::hash_utils::doHash("Input"): {
                        form.appendInput(customize.value("id", "ID"),
                            translateString(customize.value("title", ""), player), 
                            customize.value("placeholder", ""), customize.value("defaultValue", ""));
                        mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", "");
                        break;
                    }
                    case ll::hash_utils::doHash("Dropdown"): {
                        std::vector<std::string> mOptions = customize.value("options", std::vector<std::string>());
                        if (mOptions.size() < 1)
                            break;
                        form.appendDropdown(customize.value("id", "ID"),
                            translateString(customize.value("title", ""), player), 
                            mOptions, customize.value("defaultValue", 0));
                        mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                        break;
                    }
                    case ll::hash_utils::doHash("Toggle"): {
                        form.appendToggle(customize.value("id", "ID"),
                            translateString(customize.value("title", ""), player),
                            customize.value("defaultValue", false));
                        mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", false);
                        break;
                    }
                    case ll::hash_utils::doHash("Slider"): {
                        form.appendSlider(customize.value("id", "ID"), 
                        translateString(customize.value("title", ""), player),
                        customize.value("min", 0), customize.value("max", 100),
                        customize.value("step", 1), customize.value("defaultValue", 0));
                        mCustomData[customize.value("id", "ID")] = customize.value("defaultValue", 0);
                        break;
                    }
                    case ll::hash_utils::doHash("StepSlider"): {
                        std::vector<std::string> mOptions = customize.value("options", std::vector<std::string>());
                        if (mOptions.size() < 2)
                            break;
                        form.appendStepSlider(customize.value("id", "ID"),
                            translateString(customize.value("title", ""), player),
                            mOptions, customize.value("defaultValue", 0));
                        mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                        break;
                    }
                }
            }
            form.sendTo(player, [mCustomData, data](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return McUtils::executeCommand(pl, data.value("exit", ""));

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
                for (const auto& c_it : data.value("command", nlohmann::ordered_json())) {
                    std::string result = c_it.get<std::string>();
                    for (auto& item : mCustom.items()) {
                        if (result.find("{" + item.key() + "}") == std::string::npos)
                            continue;
                        ll::string_utils::replaceAll(result, "{" + item.key() + "}", item.value().get<std::string>());
                    }
                    McUtils::executeCommand(pl, result);
                }
            });
        }

        void simple(Player& player, nlohmann::ordered_json& data) {
            ll::form::SimpleForm form(translateString(data.value("title", ""), player),
                translateString(data.value("content", ""), player));
            for (nlohmann::ordered_json& button : data.value("button", nlohmann::ordered_json())) {
                form.appendButton(translateString(button.value("title", ""), player), button.value("image", ""), "path", [data, button](Player& pl) -> void {
                    logicalExecution(pl, button, data);
                });
            }
            form.sendTo(player, [data](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return McUtils::executeCommand(pl, data.value("exit", ""));
            });
        }

        void modal(Player& player, nlohmann::ordered_json& data) {
            nlohmann::ordered_json mConfirmButton = data.value("confirmButton", nlohmann::ordered_json());
            nlohmann::ordered_json mCancelButton = data.value("cancelButton", nlohmann::ordered_json());
            if (mCancelButton.empty() || mConfirmButton.empty())
                return;

            ll::form::ModalForm form(translateString(data.value("title", ""), player),
                translateString(data.value("content", ""), player),
                translateString(mConfirmButton.value("title", ""), player),
                translateString(mCancelButton.value("title", ""), player));
            form.sendTo(player, [data, mConfirmButton, mCancelButton](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result == ll::form::ModalFormSelectedButton::Upper) 
                    return logicalExecution(pl, mConfirmButton, data);
                logicalExecution(pl, mCancelButton, data);
            });
        }

        void open(Player& player, std::string uiName) {
            if (db->has(uiName)) {
                nlohmann::ordered_json data = db->toJson(uiName);
                if (data.empty()) return;
                if (data.contains("permission")) {
                    if ((int) player.getCommandPermissionLevel() < data.value("permission", 0))
                        return McUtils::executeCommand(player, data.value("NoPermission", ""));
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
            logger->error("MenuUI {} reading failed.", uiName);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("menu", tr({}, "commands.menu.description"), CommandPermissionLevel::Any);
            command.overload<MenuOP>().text("gui").optional("uiName").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MenuOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player, param.uiName.empty() ? 
                    mObjectOptions.at("EntranceKey") : param.uiName
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
                ItemStack itemStack(mObjectOptions.at("MenuItemId"), 1, 0, nullptr);

                if (!itemStack || itemStack.isNull())
                    return output.error(tr({}, "commands.menu.error.item.null"));
                if (McUtils::isItemPlayerInventory(player, mObjectOptions.at("MenuItemId"), 1))
                    return output.error(fmt::runtime(tr({}, "commands.menu.error.item.give")), player.getRealName());

                McUtils::giveItem(player, itemStack, 1);
                player.refreshInventory();

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>(
                [](ll::event::PlayerUseItemEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    if (event.item().getTypeName() == mObjectOptions.at("MenuItemId"))
                        MainGui::open(event.self(), "main");
                }
            );
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    ItemStack itemStack(mObjectOptions.at("MenuItemId"), 1, 0, nullptr);
                    if (!itemStack || McUtils::isItemPlayerInventory(event.self(), mObjectOptions.at("MenuItemId"), 1))
                        return;
                    McUtils::giveItem(event.self(), itemStack, 1);
                    event.self().refreshInventory();
                }
            );
        }
    }

    void logicalExecution(Player& player, nlohmann::ordered_json data, const nlohmann::ordered_json& original) {
        if (data.empty() || !isValid()) 
            return;

        if (data.contains("permission")) {
            if ((int) player.getCommandPermissionLevel() < data.value("permission", 0))
                return McUtils::executeCommand(player, original.value("NoPermission", ""));
        }

        nlohmann::ordered_json defaultJson{};
        if (data.value("type", "") == "button") {
            if (!checkModifiedData(player, data))
                return McUtils::executeCommand(player, original.value("NoScore", ""));
            if (data.value("command", defaultJson).is_string())
                return McUtils::executeCommand(player, data.value("command", ""));
            for (nlohmann::ordered_json& command : data.value("command", defaultJson))
                McUtils::executeCommand(player, command.get<std::string>());
        } else if (data.value("type", "") == "from") {
            if (!checkModifiedData(player, data))
                return McUtils::executeCommand(player, original.value("NoScore", ""));
            MainGui::open(player, data.value("menu", ""));
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
        mObjectOptions = std::move(options);
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerUseItemEventListener);
        eventBus.removeListener(PlayerJoinEventListener);
    }
}