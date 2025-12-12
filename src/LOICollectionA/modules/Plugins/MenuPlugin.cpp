#include <memory>
#include <vector>
#include <string>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/utils/StringUtils.h>
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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/InventoryUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/MenuPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    enum class MenuObject;

    constexpr inline auto MenuObjectName = ll::command::enum_name_v<MenuObject>;

    struct MenuPlugin::operation {
        ll::command::SoftEnum<MenuObject> Object;
    };

    struct MenuPlugin::Impl {
        C_Config::C_Plugins::C_Menu options;
        
        std::unique_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerUseItemEventListener;
    };

    MenuPlugin::MenuPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    MenuPlugin::~MenuPlugin() = default;

    MenuPlugin& MenuPlugin::getInstance() {
        static MenuPlugin instance;
        return instance;
    }

    JsonStorage* MenuPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* MenuPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void MenuPlugin::gui::editNewInfo(Player& player, MenuType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);  

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
        form.sendTo(player, [this, mObjectLanguage, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editNew(pl);
            
            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

            if (mObjectTitle.empty() || mObjectId.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->editNew(pl);
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

            this->mParent.create(mObjectId, mData);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log1"), pl)), mObjectId);
        });
    }

    void MenuPlugin::gui::editNew(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.button1.label"));
        form.appendButton("Simple", [this](Player& pl) {
            this->editNewInfo(pl, MenuType::Simple);
        });
        form.appendButton("Modal", [this](Player& pl) {
            this->editNewInfo(pl, MenuType::Modal);
        });
        form.appendButton("Custom", [this](Player& pl) {
            this->editNewInfo(pl, MenuType::Custom);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->edit(pl);
        });
    }

    void MenuPlugin::gui::editRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "menu.gui.button2.content")), id),
            tr(mObjectLanguage, "menu.gui.button2.yes"), tr(mObjectLanguage, "menu.gui.button2.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper)
                return;

            this->mParent.remove(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log2"), pl)), id);
        });
    }

    void MenuPlugin::gui::editRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
        for (std::string& key : this->mParent.getDatabase()->keys()) {
            form.appendButton(key, [this, key](Player& pl) -> void {
                this->editRemoveInfo(pl, key);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->edit(pl);
        });
    }

    void MenuPlugin::gui::editAwardSetting(Player& player, const std::string& id, MenuType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));
        form.appendInput("Input2", tr(mObjectLanguage, "menu.gui.button1.input2"), tr(mObjectLanguage, "menu.gui.button1.input2.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""));

        switch (type) {
            case MenuType::Simple:
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), tr(mObjectLanguage, "menu.gui.button1.input3.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/content", ""));
                form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input4.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/exit", ""));
                form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), tr(mObjectLanguage, "menu.gui.button1.input5.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/score", ""));
                form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), tr(mObjectLanguage, "menu.gui.button1.input6.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/permission", ""));
                break;
            case MenuType::Modal:
                form.appendInput("Input3", tr(mObjectLanguage, "menu.gui.button1.input3"), tr(mObjectLanguage, "menu.gui.button1.input3.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/content", ""));
                form.appendInput("Input5", tr(mObjectLanguage, "menu.gui.button1.input5"), tr(mObjectLanguage, "menu.gui.button1.input5.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/score", ""));
                form.appendInput("Input6", tr(mObjectLanguage, "menu.gui.button1.input6"), tr(mObjectLanguage, "menu.gui.button1.input6.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/permission", ""));
                break;
            case MenuType::Custom:
                form.appendInput("Input4", tr(mObjectLanguage, "menu.gui.button1.input4"), tr(mObjectLanguage, "menu.gui.button1.input4.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/exit", ""));
                break;
        };

        form.appendSlider("Slider", tr(mObjectLanguage, "menu.gui.button1.slider"), 0, 4, 1, 0);
        form.sendTo(player, [this, mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editAwardContent(pl, id, type);

            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

            if (mObjectTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->editAwardContent(pl, id, type);
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/title", mObjectTitle);

            switch (type) {
                case MenuType::Simple:
                    this->mParent.getDatabase()->set_ptr("/" + id + "/content", std::get<std::string>(dt->at("Input3")));
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/exit", std::get<std::string>(dt->at("Input4")));
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/score", std::get<std::string>(dt->at("Input5")));
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/permission", std::get<std::string>(dt->at("Input6")));
                    break;
                case MenuType::Modal:
                    this->mParent.getDatabase()->set_ptr("/" + id + "/content", std::get<std::string>(dt->at("Input3")));
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/score", std::get<std::string>(dt->at("Input5")));
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/permission", std::get<std::string>(dt->at("Input6")));
                    break;
                case MenuType::Custom:
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/exit", std::get<std::string>(dt->at("Input4")));
                    break;
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/permission", (int) std::get<double>(dt->at("Slider")));
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log5"), pl)), id);
        });
    }

    void MenuPlugin::gui::editAwardNew(Player& player, const std::string& id, MenuType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

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
        form.sendTo(player, [this, mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editAwardContent(pl, id, type);

            std::string mObjectObjective = std::get<std::string>(dt->at("Input4"));
            std::string mObjectScore = std::get<std::string>(dt->at("Input5"));
            std::string mObjectRun = std::get<std::string>(dt->at("Input6"));

            auto mData = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

            switch (type) {
                case MenuType::Simple: {
                    nlohmann::ordered_json data;

                    std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                    std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
                    std::string mObjectImage = std::get<std::string>(dt->at("Input3"));

                    if (mObjectTitle.empty() || mObjectImage.empty() || mObjectId.empty()) {
                        pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                        return this->editAwardContent(pl, id, type);
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
                        return this->editAwardContent(pl, id, type);
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

            this->mParent.getDatabase()->set_ptr("/" + id, mData);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log6"), pl)), id);
        });
    }

    void MenuPlugin::gui::editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "menu.gui.button3.remove.content")), packageid),
            tr(mObjectLanguage, "menu.gui.button3.remove.yes"), tr(mObjectLanguage, "menu.gui.button3.remove.no")
        );
        form.sendTo(player, [this, id, packageid](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper) 
                return;

            auto mContent = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/customize");
            for (int i = ((int) mContent.size() - 1); i >= 0; i--) {
                if (mContent.at(i).value("id", "") == packageid)
                    mContent.erase(i);
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/customize", mContent);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log3"), pl)), id, packageid);
        });
    }

    void MenuPlugin::gui::editAwardRemove(Player& player, const std::string& id, MenuType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
        for (nlohmann::ordered_json& item : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/customize")) {
            std::string mName = item.value("id", "");

            form.appendButton(mName, [this, mName, id](Player& pl) -> void {
                this->editAwardRemoveInfo(pl, id, mName);
            });
        }
        form.sendTo(player, [this, ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->editAwardContent(pl, ids, type);
        });
    }

    void MenuPlugin::gui::editAwardCommand(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "menu.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "menu.gui.label"));

        std::string mObjectLine = tr(mObjectLanguage, "menu.gui.button3.command.line");

        auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/run");
        for (int i = 0; i < (int) content.size(); i++) {
            std::string mLine = fmt::format(fmt::runtime(mObjectLine), i + 1);
            form.appendInput("Content" + std::to_string(i), mLine, "", content.at(i));
        }

        form.appendStepSlider("StepSlider", tr(mObjectLanguage, "menu.gui.button3.command.operation"), { "no", "add", "remove" });
        form.sendTo(player, [this, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editAwardContent(pl, id, MenuType::Custom);

            auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/run");
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

            this->mParent.getDatabase()->set_ptr("/" + id + "/run", content);
            this->mParent.getDatabase()->save();

            this->editAwardCommand(pl, id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "menu.log4"), pl)), id);
        });
    }

    void MenuPlugin::gui::editAwardContent(Player& player, const std::string& id, MenuType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "menu.gui.button3.label")), id)
        );
        form.appendButton(tr(mObjectLanguage, "menu.gui.button3.setting"), "textures/ui/icon_setting", "path", [this, id, type](Player& pl) -> void {
            this->editAwardSetting(pl, id, type);
        });

        switch (type) {
            case MenuType::Simple:
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [this, id](Player& pl) -> void {
                    this->editAwardNew(pl, id, MenuType::Simple);
                });
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [this, id](Player& pl) -> void {
                    this->editAwardRemove(pl, id, MenuType::Simple);
                });
                break;
            case MenuType::Modal:
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.new"), "textures/ui/icon_sign", "path", [this, id](Player& pl) -> void {
                    this->editAwardNew(pl, id, MenuType::Modal);
                });
                break;
            case MenuType::Custom:
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.remove"), "textures/ui/icon_trash", "path", [this, id](Player& pl) -> void {
                    this->editAwardRemove(pl, id, MenuType::Custom);
                });
                form.appendButton(tr(mObjectLanguage, "menu.gui.button3.command"), "textures/ui/creative_icon", "path", [this, id](Player& pl) -> void {
                    this->editAwardCommand(pl, id);
                });
        };

        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->editAward(pl);
        });
    }

    void MenuPlugin::gui::editAward(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
        for (std::string& key : this->mParent.getDatabase()->keys()) {
            form.appendButton(key, [this, key](Player& pl) -> void {
                auto mObjectType = this->mParent.getDatabase()->get_ptr<std::string>("/" + key + "/type", "Custom");

                switch (ll::hash_utils::doHash(mObjectType)) {
                    case ll::hash_utils::doHash("Simple"):
                        this->editAwardContent(pl, key, MenuType::Simple);
                        break;
                    case ll::hash_utils::doHash("Modal"):
                        this->editAwardContent(pl, key, MenuType::Modal);
                        break;
                    case ll::hash_utils::doHash("Custom"):
                        this->editAwardContent(pl, key, MenuType::Custom);
                        break;
                };
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->edit(pl);
        });
    }

    void MenuPlugin::gui::edit(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "menu.gui.title"), tr(mObjectLanguage, "menu.gui.label"));
        form.appendButton(tr(mObjectLanguage, "menu.gui.button1"), "textures/ui/achievements", "path", [this](Player& pl) -> void {
            this->editNew(pl);
        });
        form.appendButton(tr(mObjectLanguage, "menu.gui.button2"), "textures/ui/world_glyph_color", "path", [this](Player& pl) -> void {
            this->editRemove(pl);
        });
        form.appendButton(tr(mObjectLanguage, "menu.gui.button3"), "textures/ui/editIcon", "path", [this](Player& pl) -> void {
            this->editAward(pl);
        });
        form.sendTo(player);
    }

    void MenuPlugin::gui::custom(Player& player, const std::string& id) {
        nlohmann::ordered_json mCustomData{};

        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

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
                        customize.value("defaultValue", ""),
                        customize.value("tooltip", "")
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
                        customize.value("defaultValue", 0),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                    break;
                }
                case ll::hash_utils::doHash("Toggle"): {
                    form.appendToggle(
                        customize.value("id", "ID"),
                        LOICollectionAPI::translateString(customize.value("title", ""), player),
                        customize.value("defaultValue", false),
                        customize.value("tooltip", "")
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
                        customize.value("defaultValue", 0),
                        customize.value("tooltip", "")
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
                        customize.value("defaultValue", 0),
                        customize.value("tooltip", "")
                    );

                    mCustomData[customize.value("id", "ID")] = mOptions.at(customize.value("defaultValue", 0));
                    break;
                }
            }
        }

        if (data.contains("submit"))
            form.setSubmitButton(LOICollectionAPI::translateString(data.value("submit", ""), player));

        form.sendTo(player, [this, mCustomData, data](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->mParent.executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));

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
                this->mParent.executeCommand(pl, result);
            }
        });
    }

    void MenuPlugin::gui::simple(Player& player, const std::string& id) {
        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

        ll::form::SimpleForm form(LOICollectionAPI::translateString(data.value("title", ""), player), LOICollectionAPI::translateString(data.value("content", ""), player));
        for (nlohmann::ordered_json& customize : data.value("customize", nlohmann::ordered_json())) {
            switch (ll::hash_utils::doHash(customize.value("type", ""))) {
                case ll::hash_utils::doHash("button"):
                case ll::hash_utils::doHash("from"):
                    form.appendButton(LOICollectionAPI::translateString(customize.value("title", ""), player), customize.value("image", ""), "path", [this, data, customize](Player& pl) -> void {
                        this->mParent.handleAction(pl, customize, data);
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
        form.sendTo(player, [this, data](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->mParent.executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));
        });
    }

    void MenuPlugin::gui::modal(Player& player, const std::string& id) {
        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

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
        form.sendTo(player, [this, data, mConfirmButton, mCancelButton](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) 
                return this->mParent.handleAction(pl, mConfirmButton, data);
            this->mParent.handleAction(pl, mCancelButton, data);
        });
    }

    void MenuPlugin::gui::open(Player& player, const std::string& id) {
        if (this->mParent.getDatabase()->has(id)) {
            auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);
            
            if (data.empty()) return;
            if (data.contains("permission")) {
                if ((int) player.getCommandPermissionLevel() < data.value("permission", 0))
                    return this->mParent.executeCommand(player, data.value("info", nlohmann::ordered_json{}).value("permission", ""));
            }
            
            switch (ll::hash_utils::doHash(data.value("type", ""))) {
                case ll::hash_utils::doHash("Custom"):
                    this->custom(player, id);
                    break;
                case ll::hash_utils::doHash("Simple"):
                    this->simple(player, id);
                    break;
                case ll::hash_utils::doHash("Modal"):
                    this->modal(player, id);
                    break;
                default:
                    this->mParent.getLogger()->error("Unknown UI type {}.", data.value("type", ""));
                    break;
            }

            return;
        }

        this->mParent.getLogger()->error("MenuUI {} reading failed.", id);
    }

    void MenuPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance().tryRegisterSoftEnum(MenuObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("menu", tr({}, "commands.menu.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").optional("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player, param.Object.empty() ? 
                this->mImpl->options.EntranceKey : (std::string)param.Object
            );
            
            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->edit(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("clock").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            ItemStack itemStack(this->mImpl->options.MenuItemId, 1, 0, nullptr);
            if (!itemStack || itemStack.isNull())
                return output.error(tr({}, "commands.menu.error.item.null"));
            if (InventoryUtils::isItemInInventory(player, this->mImpl->options.MenuItemId, 1))
                return output.error(fmt::runtime(tr({}, "commands.menu.error.item.give")), player.getRealName());

            InventoryUtils::giveItem(player, itemStack, 1);
            player.refreshInventory();

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void MenuPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([this](ll::event::PlayerUseItemEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            if (event.item().getTypeName() == this->mImpl->options.MenuItemId)
                this->mGui->open(event.self(), "main");
        });
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            ItemStack itemStack(this->mImpl->options.MenuItemId, 1, 0, nullptr);
            if (!itemStack || InventoryUtils::isItemInInventory(event.self(), this->mImpl->options.MenuItemId, 1))
                return;
            InventoryUtils::giveItem(event.self(), itemStack, 1);
            event.self().refreshInventory();
        });
    }

    void MenuPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerUseItemEventListener);
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
    }

    void MenuPlugin::create(const std::string& id, const nlohmann::ordered_json& data) {
        if (!this->isValid())
            return;

        if (!this->getDatabase()->has(id))
            this->getDatabase()->set(id, data);
        this->getDatabase()->save();

        ll::command::CommandRegistrar::getInstance().addSoftEnumValues(MenuObjectName, { id });
    }

    void MenuPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->remove(id);
        this->getDatabase()->save();

        ll::command::CommandRegistrar::getInstance().removeSoftEnumValues(MenuObjectName, { id });
    }

    void MenuPlugin::executeCommand(Player& player, std::string cmd) {
        if (!this->isValid())
            return;

        ll::string_utils::replaceAll(cmd, "${player}", std::string(player.mName));

        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        Command* command = ll::service::getMinecraft()->mCommands->compileCommand(
            HashedString(cmd), origin, (CurrentCmdVersion)CommandVersion::CurrentVersion(),
            [this](std::string const& message) -> void {
                this->getLogger()->error("Command error: {}", message);
            }
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    void MenuPlugin::handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original) {
        if (!this->isValid() || action.empty())
            return;

        if (action.contains("permission")) {
            if ((int) player.getCommandPermissionLevel() < action["permission"])
                return executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("permission", ""));
        }

        if (action.contains("scores")) {
            for (const auto& [key, value] : action["scores"].items()) {
                if (value.get<int>() > ScoreboardUtils::getScore(player, key))
                    return this->executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("score", ""));
            }
            for (const auto& [key, value] : action["scores"].items())
                ScoreboardUtils::reduceScore(player, key, value.get<int>());
        }

        if (action.value("type", "") == "button") {
            if (action["run"].is_string())
                return this->executeCommand(player, action["run"].get<std::string>());
            
            for (const auto& cmd : action["run"])
                this->executeCommand(player, cmd.get<std::string>());
            return;
        }

        this->mGui->open(player, action.value("run", ""));
    }

    bool MenuPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool MenuPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Menu.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_unique<JsonStorage>(mDataPath / "menu.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Menu;

        return true;
    }

    bool MenuPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        return true;
    }

    bool MenuPlugin::registry() {     
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->registeryCommand();
        this->listenEvent();

        return true;
    }

    bool MenuPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->unlistenEvent();

        this->getDatabase()->save();

        return true;
    }
}

REGISTRY_HELPER("MenuPlugin", LOICollection::Plugins::MenuPlugin, LOICollection::Plugins::MenuPlugin::getInstance())
