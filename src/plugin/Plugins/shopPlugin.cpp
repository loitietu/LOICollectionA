#include <map>
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
#include <ll/api/utils/StringUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/world/item/registry/ItemStack.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Include/APIUtils.h"
#include "Include/chatPlugin.h"

#include "Utils/toolUtils.h"
#include "Utils/JsonUtils.h"

#include "Include/shopPlugin.h"

using LOICollection::LOICollectionAPI::translateString;

namespace LOICollection::Plugins::shop {
    struct ShopOP {
        std::string uiName;
    };

    std::unique_ptr<JsonUtils> db;
    ll::Logger logger("LOICollectionA - Shop");

    namespace MainGui {
        void menu(void* player_ptr, nlohmann::ordered_json& data, bool type) {
            Player* player = static_cast<Player*>(player_ptr);

            std::vector<nlohmann::ordered_json> mItemLists;
            ll::form::SimpleForm form(translateString(data.at("title").get<std::string>(), player));
            form.setContent(translateString(data.at("content").get<std::string>(), player));
            for (auto& item : data.at("classiflcation")) {
                std::string mTitle = translateString(item.at("title").get<std::string>(), player);
                std::string mImage = item.at("image").get<std::string>();
                form.appendButton(mTitle, mImage, "path");
                mItemLists.push_back(item);
            }
            form.sendTo(*player, [data, mItemLists, type](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) {
                    pl.sendMessage(translateString(data.at("exit").get<std::string>(), &pl));
                    return;
                }
                std::map<std::string, std::string> options;
                options["exit"] = translateString(data.at("exit").get<std::string>(), &pl);
                if (type) options["score"] = translateString(data.at("NoScore").get<std::string>(), &pl);
                else {
                    options["title"] = translateString(data.at("NoTitle").get<std::string>(), &pl);
                    options["item"] = translateString(data.at("NoItem").get<std::string>(), &pl);
                }

                nlohmann::ordered_json mItem = mItemLists.at(id);
                if (mItem["type"] == "commodity")
                    MainGui::commodity(&pl, mItem, options, type);
                else if (mItem["type"] == "title")
                    MainGui::title(&pl, mItem, options, type);
                else if (mItem["type"] == "from")
                    MainGui::open(&pl, mItem.at("menu").get<std::string>());
            });
        }

        void commodity(void* player_ptr, nlohmann::ordered_json& data, std::map<std::string, std::string> options, bool type) {
            Player* player = static_cast<Player*>(player_ptr);

            std::string mScoreboardListsString;
            if (!data["scores"].empty()) {
                for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
                    mScoreboardListsString += it.key() + ":" + std::to_string(it.value().get<int>()) + ";";
                mScoreboardListsString.pop_back();
            } else mScoreboardListsString = "None";
            std::string mIntroduce = translateString(data.at("introduce").get<std::string>(), player);

            ll::form::CustomForm form(translateString(data.at("title").get<std::string>(), player));
            form.appendLabel(ll::string_utils::replaceAll(mIntroduce, "${scores}", mScoreboardListsString));
            form.appendSlider("slider", translateString(data.at("number").get<std::string>(), player), 1, 64);
            form.sendTo(*player, [data, options, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(options.at("exit"));
                    return;
                }
                int mNumber = (int) std::get<double>(dt->at("slider"));
                if (type) {
                    if (checkModifiedData(&pl, data, mNumber)) {
                        ItemStack itemStack(data.at("id").get<std::string>(), mNumber);
                        pl.add(itemStack);
                        pl.refreshInventory();
                        return;
                    }
                    pl.sendMessage(options.at("score"));
                    return;
                }
                ItemStack itemStack(data.at("id").get<std::string>(), mNumber);
                if (toolUtils::isItemPlayerInventory(&pl, &itemStack)) {
                    nlohmann::ordered_json mScoreboardBase = data.at("scores");
                    for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                        toolUtils::scoreboard::addScore(&pl, it.key(), (it.value().get<int>() * mNumber));
                    toolUtils::clearItem(&pl, &itemStack);
                    pl.refreshInventory();
                    return;
                }
                pl.sendMessage(options.at("item"));
            });
        }

        void title(void* player_ptr, nlohmann::ordered_json& data, std::map<std::string, std::string> options, bool type) {
            Player* player = static_cast<Player*>(player_ptr);

            std::string mScoreboardListsString;
            if (!data["scores"].empty()) {
                for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
                    mScoreboardListsString += it.key() + ":" + std::to_string(it.value().get<int>()) + ";";
                mScoreboardListsString.pop_back();
            } else mScoreboardListsString = "None";
            std::string mIntroduce = translateString(data.at("introduce").get<std::string>(), player);

            ll::form::ModalForm form;
            form.setTitle(translateString(data.at("title").get<std::string>(), player));
            form.setContent(ll::string_utils::replaceAll(mIntroduce, "${scores}", mScoreboardListsString));
            form.setUpperButton(translateString(data.at("confirmButton").get<std::string>(), player));
            form.setLowerButton(translateString(data.at("cancelButton").get<std::string>(), player));
            form.sendTo(*player, [data, options, type](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    std::string id = data.at("id").get<std::string>();
                    if (type) {
                        if (checkModifiedData(&pl, data, 1)) {
                            if (data.contains("time")) {
                                chat::addChat(&pl, id, data.at("time").get<int>());
                                return;
                            }
                            chat::addChat(&pl, id, 0);
                        }
                        return;
                    }
                    if (chat::isChat(&pl, id)) {
                        nlohmann::ordered_json mScoreboardBase = data.at("scores");
                        for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                            toolUtils::scoreboard::addScore(&pl, it.key(), it.value().get<int>());
                        chat::delChat(&pl, id);
                        return;
                    }
                    pl.sendMessage(options.at("title"));
                }
            });
        }

        void open(void* player_ptr, std::string uiName) {
            if (db->has(uiName)) {
                nlohmann::ordered_json data = db->toJson(uiName);
                if (data.at("type").get<std::string>() == "buy")
                    MainGui::menu(player_ptr, data, true);
                else if (data.at("type").get<std::string>() == "sell")
                    MainGui::menu(player_ptr, data, false);
                return;
            }
            logger.error("ShopUI {} reading failed.", uiName);
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("shop", "§e§lLOICollection -> §b服务器商店", CommandPermissionLevel::Any);
            command.overload<ShopOP>().text("gui").required("uiName").execute([](CommandOrigin const& origin, CommandOutput& output, ShopOP param) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player, param.uiName);
            });
            command.overload().text("buy").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player, "MainBuy");
            });
            command.overload().text("sell").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player, "MainSell");
            });
        }
    }

    bool checkModifiedData(void* player_ptr, nlohmann::ordered_json data, int number) {
        Player* player = static_cast<Player*>(player_ptr);
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if ((it.value().get<int>() * number) > toolUtils::scoreboard::getScore(player, it.key())) {
                return false;
            }
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            toolUtils::scoreboard::reduceScore(player, it.key(), (it.value().get<int>() * number));
        return true;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<JsonUtils>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
    }
}