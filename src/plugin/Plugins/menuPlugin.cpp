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

#include "Utils/toolUtils.h"
#include "Utils/JsonUtils.h"

#include "Include/menuPlugin.h"

using LOICollectionAPI::translateString;

namespace menuPlugin {
    struct MenuOP {
        std::string uiName;
    };

    std::string mItemId;
    std::unique_ptr<JsonUtils> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerUseItemEventListener;
    ll::Logger logger("LOICollectionA - Menu");

    namespace MainGui {
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
                    toolUtils::executeCommand(&pl, result);
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
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
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
                if (toolUtils::isItemPlayerInventory(player, &itemStack)) {
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
                    if (!itemStack || toolUtils::isItemPlayerInventory(&event.self(), &itemStack))
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
            if (it.value().get<int>() > toolUtils::scoreboard::getScore(player, it.key())) {
                return false;
            }
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            toolUtils::scoreboard::reduceScore(player, it.key(), it.value().get<int>());
        return true;
    }

    void logicalExecution(void* player_ptr, nlohmann::ordered_json& data, nlohmann::ordered_json original) {
        Player* player = static_cast<Player*>(player_ptr);
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
                toolUtils::executeCommand(player, data.at("command").get<std::string>());
                return;
            }
            for (auto& command : data.at("command"))
                toolUtils::executeCommand(player, command.get<std::string>());
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