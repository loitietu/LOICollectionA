#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
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

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/world/item/registry/ItemStack.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Include/APIUtils.h"
#include "Include/languagePlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/toolUtils.h"
#include "Utils/JsonUtils.h"

#include "Include/menuPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;
using LOICollection::LOICollectionAPI::translateString;

namespace LOICollection::Plugins::menu {
    struct MenuOP {
        std::string uiName;
    };

    std::string mItemId;
    std::unique_ptr<JsonUtils> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerUseItemEventListener;
    ll::Logger logger("LOICollectionA - Menu");

    namespace MainGui {
        void editNew(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "menu.gui.button1.input1"), "", "None");
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), "", "None");
            form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), "", "None");
            form.appendDropdown("dropdown", tr(mObjectLanguage, "menu.gui.button1.dropdown"), { "Custom", "Simple", "Modal" });
            form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), "", "None");
            form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), "", "None");
            form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), "", "None");
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::edit(&pl);
                    return;
                }
                std::string mObjectInput1 = std::get<std::string>(dt->at("Input1"));
                std::string mObjectInput2 = std::get<std::string>(dt->at("Input2"));
                std::string mObjectInput3 = std::get<std::string>(dt->at("Input3"));
                std::string mObjectInput4 = std::get<std::string>(dt->at("Input4"));
                std::string mObjectInput5 = std::get<std::string>(dt->at("Input5"));
                std::string mObjectInput6 = std::get<std::string>(dt->at("Input6"));
                std::string mObjectDropdown = std::get<std::string>(dt->at("dropdown"));
                int mObjectSlider = (int) std::get<double>(dt->at("Slider"));

                if (mObjectInput1.empty()) mObjectInput1 = "Menu";
                if (mObjectInput2.empty()) mObjectInput2 = "Exit";
                if (mObjectInput3.empty()) mObjectInput3 = "Exit";
                if (mObjectInput4.empty()) mObjectInput4 = "Exit";
                if (mObjectInput5.empty()) mObjectInput5 = "Content";
                if (mObjectInput6.empty()) mObjectInput6 = "Title";

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

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::editNew(player);
                });

                std::string logString = tr(getLanguage(&pl), "menu.log1");
                ll::string_utils::replaceAll(logString, "${menu}", mObjectInput1);
                logger.info(translateString(logString, &pl));
            });
        }

        void editRemove(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (auto& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    std::string mObjectLanguage = getLanguage(&pl);
                    std::string mObjectContent = tr(mObjectLanguage, "menu.gui.button2.content");
                    ll::string_utils::replaceAll(mObjectContent, "${menu}", key);
                    ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "menu.gui.button2.yes"), tr(mObjectLanguage, "menu.gui.button2.no"));
                    form.sendTo(pl, [key](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                        if (result == ll::form::ModalFormSelectedButton::Upper) {
                            db->remove(key);
                            db->save();

                            std::string logString = tr(getLanguage(&pl), "menu.log2");
                            ll::string_utils::replaceAll(logString, "${menu}", key);
                            logger.info(translateString(logString, &pl));
                        }
                        toolUtils::Gui::submission(&pl, [](Player* player) {
                            return MainGui::editRemove(player);
                        });
                    });
                });
            }
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::edit(&pl);
            });
        }

        void editAwardSetting(void* player_ptr, std::string uiName) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            nlohmann::ordered_json data = db->toJson(uiName);
            std::string mObjectType = data.at("type").get<std::string>();

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), "", data.at("title"));
            if (mObjectType != "Custom") {
                form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), "", data.at("content"));
                form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), "", data.at("NoScore"));
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), "", data.at("NoPermission"));
            }
            if (mObjectType != "Modal")
                form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), "", data.at("exit"));
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
            form.sendTo(*player, [uiName, mObjectType](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::editAwardContent(&pl, uiName);
                    return;
                }
                nlohmann::ordered_json data = db->toJson(uiName);
                data["title"] = std::get<std::string>(dt->at("Input6"));
                switch (ll::hash_utils::doHash(mObjectType)) {
                    case ll::hash_utils::doHash("Simple"):
                        data["content"] = std::get<std::string>(dt->at("Input5"));
                        data["exit"] = std::get<std::string>(dt->at("Input2"));
                        data["NoScore"] = std::get<std::string>(dt->at("Input4"));
                        data["NoPermission"] = std::get<std::string>(dt->at("Input3"));
                        break;
                    case ll::hash_utils::doHash("Modal"):
                        data["content"] = std::get<std::string>(dt->at("Input5"));
                        data["NoScore"] = std::get<std::string>(dt->at("Input4"));
                        data["NoPermission"] = std::get<std::string>(dt->at("Input3"));
                        break;
                    case ll::hash_utils::doHash("Custom"):
                        data["exit"] = std::get<std::string>(dt->at("Input2"));
                        break;
                }
                data["permission"] = (int) std::get<double>(dt->at("Slider"));
                db->set(uiName, data);
                db->save();

                toolUtils::Gui::submission(&pl, [uiName](Player* player) {
                    return MainGui::editAwardContent(player, uiName);
                });

                std::string logString = tr(getLanguage(&pl), "menu.log5");
                ll::string_utils::replaceAll(logString, "${menu}", uiName);
                logger.info(translateString(logString, &pl));
            });
        }

        void editAwardNew(void* player_ptr, std::string uiName, int mType) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
            if (mType == SIMPLE_TYPE)
                form.appendInput("Input1", tr(mObjectLanguage, "menu.gui.button3.new.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button3.new.input2"), "", "None");
            if (mType == SIMPLE_TYPE)
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button3.new.input3"), "", "textures/items/diamond");
            form.appendDropdown("dropdown1", tr(mObjectLanguage, "menu.gui.button3.new.dropdown1"), { "button", "from" });
            form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button3.new.input4"), "", "money");
            form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button3.new.input5"), "", "100");
            form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button3.new.input6"), "", "");
            form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button3.new.slider"), 0, 4, 1, 0);
            if (mType == MODAL_TYPE)
                form.appendDropdown("dropdown2", tr(mObjectLanguage, "menu.gui.button3.new.dropdown2"), { "Upper", "Lower" });
            form.sendTo(*player, [uiName, mType](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::editAwardContent(&pl, uiName);
                    return;
                }
                std::string mObjectInput2 = std::get<std::string>(dt->at("Input2"));
                std::string mObjectInput4 = std::get<std::string>(dt->at("Input4"));
                std::string mObjectInput5 = std::get<std::string>(dt->at("Input5"));
                std::string mObjectInput6 = std::get<std::string>(dt->at("Input6"));
                std::string mObjectType = std::get<std::string>(dt->at("dropdown1"));

                nlohmann::ordered_json data;
                data["title"] = mObjectInput2.empty() ? "Title" : mObjectInput2;
                if (mType == SIMPLE_TYPE) {
                    std::string mObjectInput1 = std::get<std::string>(dt->at("Input1"));
                    data["image"] = std::get<std::string>(dt->at("Input3"));
                    data["id"] = mObjectInput1.empty() ? "ID" : mObjectInput1;
                }
                data["scores"] = nlohmann::ordered_json::object();
                if (!mObjectInput4.empty())
                    data["scores"][mObjectInput4] = toolUtils::System::toInt((mObjectInput5.empty() ? "100" : mObjectInput5), 0);
                mObjectType == "button" ? data["command"] = mObjectInput6 : data["menu"] = mObjectInput6;
                data["type"] = mObjectType;
                data["permission"] = (int) std::get<double>(dt->at("Slider"));

                nlohmann::ordered_json mContent = db->toJson(uiName);
                if (mType == SIMPLE_TYPE)
                    mContent["button"].push_back(data);
                else std::get<std::string>(dt->at("dropdown2")) == "Upper" ? mContent["confirmButton"] = data : mContent["cancelButton"] = data;
                db->set(uiName, mContent);
                db->save();

                toolUtils::Gui::submission(&pl, [uiName](Player* player) {
                    return MainGui::editAwardContent(player, uiName);
                });

                std::string logString = tr(getLanguage(&pl), "menu.log6");
                ll::string_utils::replaceAll(logString, "${menu}", uiName);
                logger.info(translateString(logString, &pl));
            });
        }

        void editAwardRemove(void* player_ptr, std::string uiName) {
            Player* player = static_cast<Player*>(player_ptr);
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
            form.sendTo(*player, [uiName, mObjectItems](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) {
                    MainGui::editAwardContent(&pl, uiName);
                    return;
                }
                std::string mId = mObjectItems.at(id);
                std::string mObjectLanguage = getLanguage(&pl);
                std::string mObjectContent = tr(mObjectLanguage, "menu.gui.button3.remove.content");
                ll::string_utils::replaceAll(mObjectContent, "${customize}", mId);
                ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                    tr(mObjectLanguage, "menu.gui.button3.remove.yes"), tr(mObjectLanguage, "menu.gui.button3.remove.no"));
                form.sendTo(pl, [uiName, mId](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                    if (result == ll::form::ModalFormSelectedButton::Upper) {
                        nlohmann::ordered_json data = db->toJson(uiName);
                        nlohmann::ordered_json mContent = (data.at("type").get<std::string>() == "Custom" ? data.at("customize") : data.at("button"));
                        for (int i = 0; i < (int) mContent.size(); i++) {
                            if (mContent.at(i).at("id").get<std::string>() == mId)
                                mContent.erase(i);
                        }
                        data.at("type").get<std::string>() == "Custom" ? data.at("customize") = mContent : data.at("button") = mContent;
                        db->set(uiName, data);
                        db->save();

                        std::string logString = tr(getLanguage(&pl), "menu.log3");
                        ll::string_utils::replaceAll(logString, "${menu}", uiName);
                        ll::string_utils::replaceAll(logString, "${customize}", mId);
                        logger.info(translateString(logString, &pl));
                    }
                    toolUtils::Gui::submission(&pl, [uiName](Player* player) {
                        return MainGui::editAwardContent(player, uiName);
                    });
                });
            });
        }

        void editAwardCommand(void* player_ptr, std::string uiName) {
            int index = 1;

            Player* player = static_cast<Player*>(player_ptr);
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
            form.sendTo(*player, [index, uiName](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::editAwardContent(&pl, uiName);
                    return;
                }
                nlohmann::ordered_json data = db->toJson(uiName);
                nlohmann::ordered_json mCommand = data.at("command");
                if (std::get<uint64>(dt->at("Toggle1"))) {
                    mCommand.push_back("");
                    data["command"] = mCommand;
                    db->set(uiName, data);
                    db->save();
                    MainGui::editAwardCommand(&pl, uiName);
                    return;
                } else if (std::get<uint64>(dt->at("Toggle2"))) {
                    mCommand.erase(mCommand.end() - 1);
                    data["command"] = mCommand;
                    db->set(uiName, data);
                    db->save();
                    MainGui::editAwardCommand(&pl, uiName);
                    return;
                }
                for (int i = 1; i < index; i++)
                    mCommand.at(i - 1) = std::get<std::string>(dt->at("Input" + std::to_string(i)));
                data["command"] = mCommand;
                db->set(uiName, data);
                db->save();

                toolUtils::Gui::submission(&pl, [uiName](Player* player) {
                    return MainGui::editAwardContent(player, uiName);
                });

                std::string logString = tr(getLanguage(&pl), "menu.log4");
                ll::string_utils::replaceAll(logString, "${menu}", uiName);
                logger.info(translateString(logString, &pl));
            });
        }

        void editAwardContent(void* player_ptr, std::string uiName) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            std::string mObjectLabel = tr(mObjectLanguage, "menu.gui.button3.label");
            std::string mObjectType = db->toJson(uiName).at("type").get<std::string>();

            ll::string_utils::replaceAll(mObjectLabel, "${menu}", uiName);
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3.setting"), "textures/ui/icon_setting", "path", [uiName](Player& pl) {
                MainGui::editAwardSetting(&pl, uiName);
            });
            if (mObjectType != "Custom") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [uiName, mObjectType](Player& pl) {
                    if (mObjectType == "Simple") {
                        MainGui::editAwardNew(&pl, uiName, SIMPLE_TYPE);
                        return;
                    }
                    MainGui::editAwardNew(&pl, uiName, MODAL_TYPE);
                });
            }
            if (mObjectType != "Modal") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [uiName](Player& pl) {
                    MainGui::editAwardRemove(&pl, uiName);
                });
            }
            if (mObjectType == "Custom") {
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.command"), "textures/ui/creative_icon", "path", [uiName](Player& pl) {
                    MainGui::editAwardCommand(&pl, uiName);
                });
            }
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::editAward(&pl);
            });
        }

        void editAward(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            for (auto& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) {
                    MainGui::editAwardContent(&pl, key);
                });
            }
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::edit(&pl);
            });
        }

        void edit(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
            form.appendButton(tr(mObjectLanguage, "menu.gui.button1"), "textures/ui/achievements", "path", [](Player& pl) {
                MainGui::editNew(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "menu.gui.button2"), "textures/ui/world_glyph_color", "path", [](Player& pl) {
                MainGui::editRemove(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "menu.gui.button3"), "textures/ui/editIcon", "path", [](Player& pl) {
                MainGui::editAward(&pl);
            });
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }

        void custom(void* player_ptr, nlohmann::ordered_json& data) {
            Player* player = static_cast<Player*>(player_ptr);

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
            form.sendTo(*player, [mCustomData, data](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(translateString(data.at("exit").get<std::string>(), &pl));
                    return;
                }
                nlohmann::ordered_json mCustom;
                for (auto& [key, value] : mCustomData.items()) {
                    if (value.is_string()) {
                        std::string result = std::get<std::string>(dt->at(key));
                        if (!result.empty())
                            mCustom[key] = result;
                    }
                    if (value.is_boolean()) mCustom[key] = std::get<uint64>(dt->at(key)) ? "true" : "false";
                    if (value.is_number_integer()) mCustom[key] = std::to_string(std::get<double>(dt->at(key)));
                }
                for (auto c_it = data["command"].begin(); c_it != data["command"].end(); ++c_it) {
                    std::string result = *c_it;
                    for (auto& [key, value] : mCustom.items()) {
                        if (result.find("{" + key + "}") == std::string::npos)
                            continue;
                        ll::string_utils::replaceAll(result, "{" + key + "}", value.get<std::string>());
                    }
                    toolUtils::Mc::executeCommand(&pl, result);
                }
            });
        }

        void simple(void* player_ptr, nlohmann::ordered_json& data) {
            Player* player = static_cast<Player*>(player_ptr);

            std::vector<nlohmann::ordered_json> mButtonLists;
            ll::form::SimpleForm form(translateString(data.at("title").get<std::string>(), player));
            form.setContent(translateString(data.at("content").get<std::string>(), player));
            for (auto& button : data.at("button")) {
                std::string mTitle = translateString(button.at("title").get<std::string>(), player);
                std::string mImage = button.at("image").get<std::string>();
                form.appendButton(mTitle, mImage, "path");
                mButtonLists.push_back(button);
            }
            form.sendTo(*player, [data, mButtonLists](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) {
                    pl.sendMessage(translateString(data.at("exit").get<std::string>(), &pl));
                    return;
                }
                nlohmann::ordered_json mButton = mButtonLists.at(id);
                logicalExecution(&pl, mButton, data);
            });
        }

        void modal(void* player_ptr, nlohmann::ordered_json& data) {
            if (data.at("confirmButton").empty() || data.at("cancelButton").empty()) return;

            Player* player = static_cast<Player*>(player_ptr);

            ll::form::ModalForm form;
            form.setTitle(translateString(data.at("title").get<std::string>(), player));
            form.setContent(translateString(data.at("content").get<std::string>(), player));
            form.setUpperButton(translateString(data.at("confirmButton").at("title").get<std::string>(), player));
            form.setLowerButton(translateString(data.at("cancelButton").at("title").get<std::string>(), player));
            form.sendTo(*player, [data](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    nlohmann::ordered_json mButton = data.at("confirmButton");
                    logicalExecution(&pl, mButton, data);
                    return;
                }
                nlohmann::ordered_json mButton = data.at("cancelButton");
                logicalExecution(&pl, mButton, data);
            });
        }

        void open(void* player_ptr, std::string uiName) {
            if (db->has(uiName)) {
                Player* player = static_cast<Player*>(player_ptr);
                nlohmann::ordered_json data = db->toJson(uiName);
                if (data.empty()) return;
                if (data.contains("permission")) {
                    if ((int) player->getPlayerPermissionLevel() < data["permission"].get<int>()) {
                        player->sendMessage(translateString(data.at("NoPermission").get<std::string>(), player));
                        return;
                    }
                }
                switch (ll::hash_utils::doHash(data.at("type").get<std::string>())) {
                    case ll::hash_utils::doHash("Custom"):
                        custom(player_ptr, data);
                        break;
                    case ll::hash_utils::doHash("Simple"):
                        simple(player_ptr, data);
                        break;
                    case ll::hash_utils::doHash("Modal"):
                        modal(player_ptr, data);
                        break;
                    default:
                        logger.error("Unknown UI type {}.", data.at("type").get<std::string>());
                        break;
                }
                return;
            }
            logger.error("MenuUI {} reading failed.", uiName);
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("menu", "§e§lLOICollection -> §b服务器菜单", CommandPermissionLevel::Any);
            command.overload<MenuOP>().text("gui").optional("uiName").execute([](CommandOrigin const& origin, CommandOutput& output, MenuOP param) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                if (param.uiName.empty()) {
                    MainGui::open(player, "main");
                    return;
                }
                MainGui::open(player, param.uiName);
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
            command.overload().text("clock").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                ItemStack itemStack(mItemId, 1);
                if (!itemStack) {
                    output.error("Failed to give the MenuItem to player {}", player->getRealName());
                    return;
                }
                if (toolUtils::Mc::isItemPlayerInventory(player, &itemStack)) {
                    output.error("The MenuItem has already been given to player {}", player->getRealName());
                    return;
                }
                player->add(itemStack);
                player->refreshInventory();
                output.success("The MenuItem has been given to player {}", player->getRealName());
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>(
                [](ll::event::PlayerUseItemEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    if (event.item().getTypeName() == mItemId)
                        MainGui::open(&event.self(), "main");
                }
            );
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    ItemStack itemStack(mItemId, 1);
                    if (!itemStack || toolUtils::Mc::isItemPlayerInventory(&event.self(), &itemStack))
                        return;
                    event.self().add(itemStack);
                    event.self().refreshInventory();
                }
            );
        }
    }

    bool checkModifiedData(void* player_ptr, nlohmann::ordered_json& data) {
        Player* player = static_cast<Player*>(player_ptr);
        if (!data.contains("scores")) 
            return true;
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if (it.value().get<int>() > toolUtils::scoreboard::getScore(player, it.key()))
                return false;
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            toolUtils::scoreboard::reduceScore(player, it.key(), it.value().get<int>());
        return true;
    }

    void logicalExecution(void* player_ptr, nlohmann::ordered_json& data, nlohmann::ordered_json original) {
        Player* player = static_cast<Player*>(player_ptr);
        if (data.empty()) return;
        if (data.contains("permission")) {
            if ((int) player->getPlayerPermissionLevel() < data["permission"].get<int>()) {
                player->sendMessage(translateString(original.at("NoPermission").get<std::string>(), player));
                return;
            }
        }
        if (data.at("type").get<std::string>() == "button") {
            if (!checkModifiedData(player, data)) {
                player->sendMessage(translateString(original.at("NoScore").get<std::string>(), player));
                return;
            }
            if (data.at("command").is_string()) {
                toolUtils::Mc::executeCommand(player, data.at("command").get<std::string>());
                return;
            }
            for (auto& command : data.at("command"))
                toolUtils::Mc::executeCommand(player, command.get<std::string>());
        } else if (data.at("type").get<std::string>() == "from") {
            if (!checkModifiedData(player, data)) {
                player->sendMessage(translateString(original.at("NoScore").get<std::string>(), player));
                return;
            }
            MainGui::open(player, data.at("menu").get<std::string>());
        }
    }

    void registery(void* database, std::string itemid) {
        mItemId = itemid;
        
        db = std::move(*static_cast<std::unique_ptr<JsonUtils>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerUseItemEventListener);
        eventBus.removeListener(PlayerJoinEventListener);
    }
}