#include <memory>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
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
#include "Include/languagePlugin.h"
#include "Include/chatPlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/toolUtils.h"
#include "Utils/JsonUtils.h"

#include "Include/cdkPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::cdk {
    struct CDKOP {
        std::string convertString;
        bool setting = false;
    };

    std::unique_ptr<JsonUtils> db;
    ll::Logger logger("LOICollectionA - CDK");

    namespace MainGui {
        void convert(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "cdk.gui.convert.input"), "", "convert");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string convertString = std::get<std::string>(dt->at("Input"));
                cdkConvert(&pl, convertString.empty() ? "default" : convertString);
            });
        }

        void cdkNew(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.new.input1"), "", "cdk");
            form.appendToggle("Toggle", tr(mObjectLanguage, "cdk.gui.new.switch"));
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.new.input2"), "", "0");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::open(&pl);
                    return;
                }
                std::string logString = tr(getLanguage(&pl), "cdk.log1");
                std::string mObjectCdk = std::get<std::string>(dt->at("Input1"));
                if (!db->has(mObjectCdk)) {
                    bool mObjectToggle = std::get<uint64>(dt->at("Toggle"));
                    int time = toolUtils::System::toInt(std::get<std::string>(dt->at("Input2")), 0);
                    std::string mObjectTime = toolUtils::System::timeCalculate(toolUtils::System::getNowTime(), time);
                    nlohmann::ordered_json mEmptyObject = nlohmann::ordered_json::object();
                    nlohmann::ordered_json mEmptyArray = nlohmann::ordered_json::array();
                    nlohmann::ordered_json dataList = {
                        {"personal", mObjectToggle},
                        {"player", mEmptyArray},
                        {"scores", mEmptyObject},
                        {"item", mEmptyObject},
                        {"title", mEmptyObject},
                        {"time", mObjectTime}
                    };
                    db->set(mObjectCdk, dataList);
                    db->save();
                }
                
                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::cdkNew(player);
                });

                logger.info(ll::string_utils::replaceAll(logString, "${cdk}", mObjectCdk));
            });
        }

        void cdkRemove(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            if (db->isEmpty()) {
                player->sendMessage(tr(mObjectLanguage, "cdk.tips"));
                MainGui::open(player);
                return;
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::open(&pl);
                    return;
                }
                std::string logString = tr(getLanguage(&pl), "cdk.log2");
                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));
                db->remove(mObjectCdk);
                db->save();

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::cdkRemove(player);
                });

                logger.info(ll::string_utils::replaceAll(logString, "${cdk}", mObjectCdk));
            });
        }

        void cdkAwardScore(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            if (db->isEmpty()) {
                player->sendMessage(tr(mObjectLanguage, "cdk.tips"));
                MainGui::cdkAward(player);
                return;
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.score.input1"), "", "money");
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.score.input2"), "", "100");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::cdkAward(&pl);
                    return;
                }
                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));
                int mObjectScore = toolUtils::System::toInt(std::get<std::string>(dt->at("Input2")), 0);
                nlohmann::ordered_json mObjectData = db->toJson(mObjectCdk);
                mObjectData["scores"][std::get<std::string>(dt->at("Input1"))] = mObjectScore;
                db->set(mObjectCdk, mObjectData);
                db->save();

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::cdkAwardScore(player);
                });
            });
        }

        void cdkAwardItem(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            if (db->isEmpty()) {
                player->sendMessage(tr(mObjectLanguage, "cdk.tips"));
                MainGui::cdkAward(player);
                return;
            }
            
            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.item.input1"), "", "minecraft:apple");
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.item.input2"), "", "apple");
            form.appendInput("Input3", tr(mObjectLanguage, "cdk.gui.award.item.input3"), "", "1");
            form.appendInput("Input4", tr(mObjectLanguage, "cdk.gui.award.item.input4"), "", "0");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::cdkAward(&pl);
                    return;
                }
                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));
                std::string mObjectName = std::get<std::string>(dt->at("Input2"));
                int mObjectCount = toolUtils::System::toInt(std::get<std::string>(dt->at("Input3")), 1);
                int mObjectAux = toolUtils::System::toInt(std::get<std::string>(dt->at("Input4")), 0);
                nlohmann::ordered_json mObjectData = db->toJson(mObjectCdk);
                mObjectData["item"][std::get<std::string>(dt->at("Input1"))] = {
                    {"name", mObjectName},
                    {"quantity", mObjectCount},
                    {"specialvalue", mObjectAux}
                };
                db->set(mObjectCdk, mObjectData);
                db->save();

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::cdkAwardItem(player);
                });
            });
        }

        void cdkAwardTitle(void *player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            if (db->isEmpty()) {
                player->sendMessage(tr(mObjectLanguage, "cdk.tips"));
                MainGui::cdkAward(player);
                return;
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.title.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.title.input2"), "", "0");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::cdkAward(&pl);
                    return;
                }
                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));
                std::string mObjectName = std::get<std::string>(dt->at("Input1"));
                int mObjectTime = toolUtils::System::toInt(std::get<std::string>(dt->at("Input2")), 0);
                nlohmann::ordered_json mObjectData = db->toJson(mObjectCdk);
                if (!mObjectData.contains("title"))
                    mObjectData["title"] = {};
                mObjectData["title"][mObjectName] = mObjectTime;
                db->set(mObjectCdk, mObjectData);
                db->save();

                toolUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::cdkAwardTitle(player);
                });
            });
        }

        void cdkAward(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.score"), "textures/items/diamond_sword", "path", [](Player& pl) {
                MainGui::cdkAwardScore(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.item"), "textures/items/diamond", "path", [](Player& pl) {
                MainGui::cdkAwardItem(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.title"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::cdkAwardTitle(&pl);
            });
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(&pl);
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
            form.appendButton(tr(mObjectLanguage, "cdk.gui.addCdk"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) {
                MainGui::cdkNew(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.removeCdk"), "textures/ui/cancel", "path", [](Player& pl) {
                MainGui::cdkRemove(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.addAward"), "textures/ui/color_picker", "path", [](Player& pl) {
                MainGui::cdkAward(&pl);
            });
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("cdk", "§e§lLOICollection -> §b总换码", CommandPermissionLevel::Any);
            command.overload<CDKOP>().text("convert").required("convertString").execute([](CommandOrigin const& origin, CommandOutput& output, CDKOP const& param) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                cdkConvert(player, param.convertString);
                output.success("The player {} has been converted to cdk: {}", player->getRealName(), param.convertString);
            });
            command.overload<CDKOP>().text("gui").optional("setting").execute([](CommandOrigin const& origin, CommandOutput& output, CDKOP const& param) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                if (param.setting) {
                    if ((int) player->getPlayerPermissionLevel() >= 2) {
                        output.success("The UI has been opened to player {}", player->getRealName());
                        MainGui::open(player);
                        return;
                    }
                    output.error("No permission to open the Setting.");
                    return;
                }
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::convert(player);
            });
        }
    }

    void cdkConvert(void* player_ptr, std::string convertString) {
        Player* player = static_cast<class Player*>(player_ptr);
        std::string mObjectLanguage = getLanguage(player);
        if (db->has(convertString)) {
            nlohmann::ordered_json cdkJson = db->toJson(convertString);
            if (cdkJson.contains("time")) {
                if (toolUtils::System::isReach(cdkJson["time"].get<std::string>())) {
                    player->sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
                    db->remove(convertString);
                    return;
                }
            }
            nlohmann::ordered_json mPlayerList = cdkJson.at("player");
            if (std::find(mPlayerList.begin(), mPlayerList.end(), player->getUuid().asString()) != mPlayerList.end()) {
                player->sendMessage(tr(mObjectLanguage, "cdk.convert.tips2"));
                return;
            }
            if (cdkJson.contains("title")) {
                nlohmann::ordered_json mTitleList = cdkJson.at("title");
                for (nlohmann::ordered_json::iterator it = mTitleList.begin(); it != mTitleList.end(); ++it)
                    chat::addChat(player, it.key(), it.value().get<int>());
            }
            nlohmann::ordered_json mItemList = cdkJson.at("item");
            nlohmann::ordered_json mScoreboardList = cdkJson.at("scores");
            for (nlohmann::ordered_json::iterator it = mScoreboardList.begin(); it != mScoreboardList.end(); ++it)
                toolUtils::scoreboard::addScore(player, it.key(), it.value().get<int>());
            for (nlohmann::ordered_json::iterator it = mItemList.begin(); it != mItemList.end(); ++it) {
                ItemStack itemStack(it.key(), it.value()["quantity"].get<int>());
                itemStack.setAuxValue(it.value()["specialvalue"].get<short>());
                itemStack.setCustomName(it.value()["name"].get<std::string>());
                player->add(itemStack);
                player->refreshInventory();
            }
            if (cdkJson["personal"].get<bool>()) db->remove(convertString);
            else {
                mPlayerList.push_back(player->getUuid().asString());
                cdkJson["player"] = mPlayerList;
                db->set(convertString, cdkJson);
            }
            std::string logString = tr(mObjectLanguage, "cdk.log3");
            ll::string_utils::replaceAll(logString, "${cdk}", convertString);
            logger.info(LOICollection::LOICollectionAPI::translateString(logString, player));
            player->sendMessage(tr(mObjectLanguage, "cdk.convert.tips3"));
            db->save();
            return;
        }
        player->sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<JsonUtils>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
    }
}