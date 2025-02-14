#include <memory>
#include <string>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <mc/safety/RedactableString.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/ChatPlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "include/CdkPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::cdk {
    struct CDKOP {
        std::string convertString;
    };

    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    namespace MainGui {
        void convert(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input", tr(mObjectLanguage, "cdk.gui.convert.input"), "", "convert");
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return;

                std::string convertString = std::get<std::string>(dt->at("Input"));
                cdkConvert(pl, convertString.empty() ? "default" : convertString);
            });
        }

        void cdkNew(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.new.input1"), "", "cdk");
            form.appendToggle("Toggle", tr(mObjectLanguage, "cdk.gui.new.switch"));
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.new.input2"), "", "0");
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::open(pl);

                std::string mObjectCdk = std::get<std::string>(dt->at("Input1"));
                if (!db->has(mObjectCdk)) {
                    int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                    std::string mObjectTime = time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time) : "0";
                    nlohmann::ordered_json dataList = {
                        { "personal", (bool)std::get<uint64>(dt->at("Toggle")) },
                        { "player", nlohmann::ordered_json::array() },
                        { "scores", nlohmann::ordered_json::object() },
                        { "item", nlohmann::ordered_json::object() },
                        { "title", nlohmann::ordered_json::object() },
                        { "time", mObjectTime }
                    };
                    db->set(mObjectCdk, dataList);
                    db->save();
                }

                logger->info(ll::string_utils::replaceAll(tr({}, "cdk.log1"), "${cdk}", mObjectCdk));
            });
        }

        void cdkRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            if (db->isEmpty()) {
                player.sendMessage(tr(mObjectLanguage, "cdk.tips"));
                return MainGui::open(player);
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::open(pl);

                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));
                db->remove(mObjectCdk);
                db->save();

                logger->info(ll::string_utils::replaceAll(tr({}, "cdk.log2"), "${cdk}", mObjectCdk));
            });
        }

        void cdkAwardScore(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            if (db->isEmpty()) {
                player.sendMessage(tr(mObjectLanguage, "cdk.tips"));
                return MainGui::cdkAward(player);
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.score.input1"), "", "money");
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.score.input2"), "", "100");
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::cdkAward(pl);

                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));

                nlohmann::ordered_json mObjectData = db->toJson(mObjectCdk);
                mObjectData["scores"][std::get<std::string>(dt->at("Input1"))] = 
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                db->set(mObjectCdk, mObjectData);
                db->save();
            });
        }

        void cdkAwardItem(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            if (db->isEmpty()) {
                player.sendMessage(tr(mObjectLanguage, "cdk.tips"));
                return MainGui::cdkAward(player);
            }
            
            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.item.input1"), "", "minecraft:apple");
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.item.input2"), "", "apple");
            form.appendInput("Input3", tr(mObjectLanguage, "cdk.gui.award.item.input3"), "", "1");
            form.appendInput("Input4", tr(mObjectLanguage, "cdk.gui.award.item.input4"), "", "0");
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::cdkAward(pl);

                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));

                nlohmann::ordered_json mObjectData = db->toJson(mObjectCdk);
                mObjectData["item"][std::get<std::string>(dt->at("Input1"))] = {
                    { "name", std::get<std::string>(dt->at("Input2")) },
                    { "quantity", SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 1) },
                    { "specialvalue", SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0) }
                };
                db->set(mObjectCdk, mObjectData);
                db->save();
            });
        }

        void cdkAwardTitle(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            if (db->isEmpty()) {
                player.sendMessage(tr(mObjectLanguage, "cdk.tips"));
                return MainGui::cdkAward(player);
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.remove.dropdown"), db->keys());
            form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.title.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.title.input2"), "", "0");
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::cdkAward(pl);

                std::string mObjectCdk = std::get<std::string>(dt->at("dropdown"));

                nlohmann::ordered_json mObjectData = db->toJson(mObjectCdk);
                !mObjectData.contains("title") ? mObjectData["title"] = {} : nullptr;
                mObjectData["title"][std::get<std::string>(dt->at("Input1"))] =
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                db->set(mObjectCdk, mObjectData);
                db->save();
            });
        }

        void cdkAward(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.score"), "textures/items/diamond_sword", "path", [](Player& pl) -> void {
                MainGui::cdkAwardScore(pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.item"), "textures/items/diamond", "path", [](Player& pl) -> void {
                MainGui::cdkAwardItem(pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.award.title"), "textures/ui/backup_replace", "path", [](Player& pl) -> void {
                MainGui::cdkAwardTitle(pl);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
            form.appendButton(tr(mObjectLanguage, "cdk.gui.addCdk"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) -> void {
                MainGui::cdkNew(pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.removeCdk"), "textures/ui/cancel", "path", [](Player& pl) -> void {
                MainGui::cdkRemove(pl);
            });
            form.appendButton(tr(mObjectLanguage, "cdk.gui.addAward"), "textures/ui/color_picker", "path", [](Player& pl) -> void {
                MainGui::cdkAward(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("cdk", tr({}, "commands.cdk.description"), CommandPermissionLevel::Any);
            command.overload<CDKOP>().text("convert").required("convertString").execute(
                [](CommandOrigin const& origin, CommandOutput& output, CDKOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                cdkConvert(player, param.convertString);
                
                output.success(fmt::runtime(tr({}, "commands.cdk.success.convert")), player.getRealName(), param.convertString);
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::convert(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));
                
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }
    }

    void cdkConvert(Player& player, const std::string& convertString) {
        if (!isValid()) return;

        std::string mObjectLanguage = getLanguage(player);
        if (db->has(convertString)) {
            nlohmann::ordered_json cdkJson = db->toJson(convertString);
            if (cdkJson.contains("time")) {
                if (SystemUtils::isReach(cdkJson["time"].get<std::string>())) {
                    player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
                    db->remove(convertString);
                    return;
                }
            }
            nlohmann::ordered_json mPlayerList = cdkJson.at("player");
            if (std::find(mPlayerList.begin(), mPlayerList.end(), player.getUuid().asString()) != mPlayerList.end())
                return player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips2"));
            if (cdkJson.contains("title")) {
                nlohmann::ordered_json mTitleList = cdkJson.at("title");
                for (nlohmann::ordered_json::iterator it = mTitleList.begin(); it != mTitleList.end(); ++it)
                    chat::addChat(player, it.key(), it.value().get<int>());
            }
            nlohmann::ordered_json mItemList = cdkJson.at("item");
            nlohmann::ordered_json mScoreboardList = cdkJson.at("scores");
            for (nlohmann::ordered_json::iterator it = mScoreboardList.begin(); it != mScoreboardList.end(); ++it)
                McUtils::scoreboard::addScore(player, it.key(), it.value().get<int>());
            for (nlohmann::ordered_json::iterator it = mItemList.begin(); it != mItemList.end(); ++it) {
                ItemStack itemStack(it.key(), it.value()["quantity"].get<int>(), it.value()["specialvalue"].get<short>(), nullptr);
                itemStack.setCustomName(Bedrock::Safety::RedactableString(it.value()["name"].get<std::string>()));
                McUtils::giveItem(player, itemStack, (int)itemStack.mCount);
                player.refreshInventory();
            }
            if (cdkJson["personal"].get<bool>()) db->remove(convertString);
            else {
                mPlayerList.push_back(player.getUuid().asString());
                cdkJson["player"] = mPlayerList;
                db->set(convertString, cdkJson);
            }
            db->save();
            
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips3"));

            logger->info(LOICollection::LOICollectionAPI::translateString(
                ll::string_utils::replaceAll(tr({}, "cdk.log3"), "${cdk}", convertString), player
            ));
            return;
        }
        player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<JsonStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        registerCommand();
    }
}