#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/world/item/registry/ItemStack.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "../Utils/toolUtils.h"
#include "../Utils/JsonUtils.h"

#include "../Include/menuPlugin.h"

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
        void simple(void* player_ptr, nlohmann::ordered_json& data) {
            Player* player = static_cast<Player*>(player_ptr);
            std::vector<nlohmann::ordered_json> mButtonLists;
            
            ll::form::SimpleForm form(data.at("title").get<std::string>());
            form.setContent(data.at("content").get<std::string>());
            for (auto& button : data.at("button")) {
                std::string mTitle = button.at("title").get<std::string>();
                std::string mImage = button.at("image").get<std::string>();
                form.appendButton(mTitle, mImage, "path");
                mButtonLists.push_back(button);
            }
            form.sendTo(*player, [data, mButtonLists](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) {
                    pl.sendMessage(data.at("exit").get<std::string>());
                    return;
                }
                nlohmann::ordered_json mButton = mButtonLists.at(id);
                if (mButton.at("type").get<std::string>() == "button") {
                    if (!checkModifiedData(&pl, mButton)) {
                        pl.sendMessage(data.at("NoScore").get<std::string>());
                        return;
                    }
                    toolUtils::executeCommand(&pl, mButton.at("command").get<std::string>());
                } else if (mButton.at("type").get<std::string>() == "from") {
                    if (!checkModifiedData(&pl, mButton)) {
                        pl.sendMessage(data.at("NoScore").get<std::string>());
                        return;
                    }
                    open(&pl, mButton.at("menu").get<std::string>());
                } else if (mButton.at("type").get<std::string>() == "opbutton") {
                    if ((int)pl.getPlayerPermissionLevel() >= 2) {
                        toolUtils::executeCommand(&pl, mButton.at("command").get<std::string>());
                        return;
                    }
                    pl.sendMessage(data.at("NoPermission").get<std::string>());
                } else if (mButton.at("type").get<std::string>() == "opfrom") {
                    if ((int)pl.getPlayerPermissionLevel() >= 2) {
                        open(&pl, mButton.at("menu").get<std::string>());
                        return;
                    }
                    pl.sendMessage(data.at("NoPermission").get<std::string>());
                }
            });
        }

        void modal(void* player_ptr, nlohmann::ordered_json& data) {
            Player* player = static_cast<Player*>(player_ptr);

            ll::form::ModalForm form;
            form.setTitle(data.at("title").get<std::string>());
            form.setContent(data.at("content").get<std::string>());
            form.setUpperButton(data.at("confirmButton").at("title").get<std::string>());
            form.setLowerButton(data.at("cancelButton").at("title").get<std::string>());
            form.sendTo(*player, [data](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                nlohmann::ordered_json mButton;
                if (result == ll::form::ModalFormSelectedButton::Upper)
                    mButton = data.at("confirmButton");
                else mButton = data.at("cancelButton");
                if (mButton.at("type").get<std::string>() == "button") {
                    if (!checkModifiedData(&pl, mButton)) {
                        pl.sendMessage(data.at("NoScore").get<std::string>());
                        return;
                    }
                    toolUtils::executeCommand(&pl, mButton.at("command").get<std::string>());
                } else if (mButton.at("type").get<std::string>() == "from") {
                    if (!checkModifiedData(&pl, mButton)) {
                        pl.sendMessage(data.at("NoScore").get<std::string>());
                        return;
                    }
                    open(&pl, mButton.at("menu").get<std::string>());
                } else if (mButton.at("type").get<std::string>() == "opbutton") {
                    if ((int)pl.getPlayerPermissionLevel() >= 2) {
                        toolUtils::executeCommand(&pl, mButton.at("command").get<std::string>());
                        return;
                    }
                } else if (mButton.at("type").get<std::string>() == "opfrom") {
                    if ((int)pl.getPlayerPermissionLevel() >= 2) {
                        open(&pl, mButton.at("menu").get<std::string>());
                        return;
                    }
                }
            });
        }

        void open(void* player_ptr, std::string uiName) {
            if (db->has(uiName)) {
                nlohmann::ordered_json data = db->toJson(uiName);
                if (data["type"].get<std::string>() == "Simple")
                    simple(player_ptr, data);
                else if (data["type"].get<std::string>() == "Modal")
                    modal(player_ptr, data);
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
                if (param.uiName.empty()) {
                    output.success("The UI has been opened to player {}", player->getRealName());
                    MainGui::open(player, "main");
                    return;
                }
                if ((int)player->getPlayerPermissionLevel() >= 2) {
                    output.success("The UI has been opened to player {}", player->getRealName());
                    MainGui::open(player, param.uiName);
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
                    if (event.self().isSimulatedPlayer()) return;
                    if (event.item().getTypeName() == mItemId) {
                        MainGui::open(&event.self(), "main");
                    }
                }
            );
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer()) return;
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
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if (it.value().get<int>() > toolUtils::scoreboard::getScore(player, it.key())) {
                return false;
            }
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            toolUtils::scoreboard::reduceScore(player, it.key(), it.value().get<int>());
        return true;
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