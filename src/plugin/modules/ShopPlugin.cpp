#include <memory>
#include <vector>
#include <string>
#include <utility>

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
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/ChatPlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "include/ShopPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;
using LOICollection::LOICollectionAPI::translateString;

namespace LOICollection::Plugins::shop {
    struct ShopOP {
        std::string uiName;
    };

    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    namespace MainGui {
        void editNew(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button1.input1"), "", "None");
            form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button1.input7"), "", "Shop Example");
            form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button1.input6"), "", "This is a shop example");
            form.appendDropdown("dropdown", tr(mObjectLanguage, "shop.gui.button1.dropdown"), { "buy", "sell" });
            form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button1.input2"), "", "execute as ${player} run say Exit Shop");
            form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button1.input3"), "", "execute as ${player} run say You do not have enough title to sell");
            form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button1.input4"), "", "execute as ${player} run say You do not have enough item to sell");
            form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button1.input5"), "", "execute as ${player} run say You do not have enough score to buy this item");
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::edit(pl);

                std::string mObjectInput1 = std::get<std::string>(dt->at("Input1"));
                std::string mObjectInput2 = std::get<std::string>(dt->at("Input2"));
                std::string mObjectInput3 = std::get<std::string>(dt->at("Input3"));
                std::string mObjectInput4 = std::get<std::string>(dt->at("Input4"));
                std::string mObjectInput5 = std::get<std::string>(dt->at("Input5"));
                std::string mObjectInput6 = std::get<std::string>(dt->at("Input6"));
                std::string mObjectInput7 = std::get<std::string>(dt->at("Input7"));
                std::string mObjectDropdown = std::get<std::string>(dt->at("dropdown"));

                nlohmann::ordered_json mData;
                switch (ll::hash_utils::doHash(mObjectDropdown)) {
                    case ll::hash_utils::doHash("buy"):
                        mData["title"] = mObjectInput7;
                        mData["content"] = mObjectInput6;
                        mData["exit"] = mObjectInput2;
                        mData["NoScore"] = mObjectInput5;
                        mData["classiflcation"] = nlohmann::ordered_json::array();
                        mData["type"] = "buy";
                        break;
                    case ll::hash_utils::doHash("sell"):
                        mData["title"] = mObjectInput7;
                        mData["content"] = mObjectInput6;
                        mData["exit"] = mObjectInput2;
                        mData["NoTitle"] = mObjectInput3;
                        mData["NoItem"] = mObjectInput4;
                        mData["classiflcation"] = nlohmann::ordered_json::array();
                        mData["type"] = "sell";
                        break;
                };
                if (!db->has(mObjectInput1))
                    db->set(mObjectInput1, mData);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "shop.log1"), "${menu}", mObjectInput1), pl));
            });
        }

        void editRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            for (std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) -> void {
                    std::string mObjectLanguage = getLanguage(pl);
                    std::string mObjectContent = tr(mObjectLanguage, "shop.gui.button2.content");

                    ll::string_utils::replaceAll(mObjectContent, "${menu}", key);
                    ll::form::ModalForm form(tr(mObjectLanguage, "shop.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "shop.gui.button2.yes"), tr(mObjectLanguage, "shop.gui.button2.no"));
                    form.sendTo(pl, [key](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                        if (result == ll::form::ModalFormSelectedButton::Upper) {
                            db->remove(key);
                            db->save();

                            logger->info(translateString(ll::string_utils::replaceAll(tr({}, "shop.log2"), "${menu}", key), pl));
                        }
                    });
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void editAwardSetting(Player& player, const std::string& uiName, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            nlohmann::ordered_json data = db->toJson(uiName);
            ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
            form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button1.input7"), "", data.at("title"));
            form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button1.input6"), "", data.at("content"));
            form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button1.input2"), "", data.at("exit"));
            if (type == ShopType::buy)
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button1.input5"), "", data.at("NoScore"));
            if (type == ShopType::sell) {
                form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button1.input3"), "", data.at("NoTitle"));
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button1.input4"), "", data.at("NoItem"));
            }
            form.sendTo(player, [uiName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, uiName);

                nlohmann::ordered_json data = db->toJson(uiName);
                data["title"] = std::get<std::string>(dt->at("Input6"));
                data["content"] = std::get<std::string>(dt->at("Input7"));
                data["exit"] = std::get<std::string>(dt->at("Input2"));
                switch (type) {
                    case ShopType::buy:
                        data["NoScore"] = std::get<std::string>(dt->at("Input5"));
                        break;
                    case ShopType::sell:
                        data["NoTitle"] = std::get<std::string>(dt->at("Input3"));
                        data["NoItem"] = std::get<std::string>(dt->at("Input4"));
                        break;
                };
                db->set(uiName, data);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "shop.log4"), "${menu}", uiName), pl));
            });
        }

        void editAwardNew(Player& player, const std::string& uiName, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button3.new.input1"), "", "Title");
            form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button3.new.input2"), "", "textures/items/diamond");
            form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), "", "This is a item");
            form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), "", "money");
            form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), "", "100");
            form.appendDropdown("dropdown", tr(mObjectLanguage, "shop.gui.button3.new.dropdown"), { "commodity", "title", "from" });
            form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), "", "");
            form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), "", "Confirm");
            form.appendInput("Input8", tr(mObjectLanguage, "shop.gui.button3.new.input8"), "", "Cancel");
            if (type == ShopType::buy)
                form.appendInput("Input9", tr(mObjectLanguage, "shop.gui.button3.new.input9"), "", "24");
            form.sendTo(player, [uiName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, uiName);

                std::string mObjectInput4 = std::get<std::string>(dt->at("Input4"));
                std::string mObjectInput5 = std::get<std::string>(dt->at("Input5"));
                std::string mObjectInput9 = std::get<std::string>(dt->at("Input9"));
                std::string mObjectType = std::get<std::string>(dt->at("dropdown"));

                nlohmann::ordered_json data;
                data["title"] = std::get<std::string>(dt->at("Input1"));
                data["image"] = std::get<std::string>(dt->at("Input2"));
                if (mObjectType != "from") {
                    data["introduce"] = std::get<std::string>(dt->at("Input3"));
                    if (mObjectType == "title") {
                        data["confirmButton"] = std::get<std::string>(dt->at("Input7"));
                        data["cancelButton"] = std::get<std::string>(dt->at("Input8"));
                    } else data["number"] = std::get<std::string>(dt->at("Input7"));
                }
                (mObjectType == "from" ? data["menu"] : data["id"]) = std::get<std::string>(dt->at("Input6"));
                if (type == ShopType::buy && mObjectType == "title")
                    data["time"] = SystemUtils::toInt((mObjectInput9.empty() ? "24" : mObjectInput9), 0);
                data["scores"] = nlohmann::ordered_json::object();
                if (!mObjectInput4.empty())
                    data["scores"][mObjectInput4] = SystemUtils::toInt((mObjectInput5.empty() ? "100" : mObjectInput5), 0);
                data["type"] = mObjectType;

                nlohmann::ordered_json mContent = db->toJson(uiName);
                mContent["classiflcation"].push_back(data);
                db->set(uiName, mContent);
                db->save();

                logger->info(translateString(ll::string_utils::replaceAll(tr({}, "menu.log5"), "${menu}", uiName), pl));
            });
        }

        void editAwardRemove(Player& player, const std::string& uiName) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            for (nlohmann::ordered_json& item : db->toJson(uiName).at("classiflcation")) {
                std::string mName = item.at("title").get<std::string>();
                form.appendButton(mName, [mName, uiName](Player& pl) -> void {
                    std::string mObjectLanguage = getLanguage(pl);
                    std::string mObjectContent = tr(mObjectLanguage, "shop.gui.button3.remove.content");
                    ll::string_utils::replaceAll(mObjectContent, "${customize}", mName);
                    ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), mObjectContent,
                        tr(mObjectLanguage, "shop.gui.button3.remove.yes"), tr(mObjectLanguage, "shop.gui.button3.remove.no"));
                    form.sendTo(pl, [uiName, mName](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                        if (result == ll::form::ModalFormSelectedButton::Upper) {
                            nlohmann::ordered_json data = db->toJson(uiName);
                            nlohmann::ordered_json mContent = data.at("classiflcation");
                            for (int i = 0; i < (int) mContent.size(); i++) {
                                if (mContent.at(i).at("title").get<std::string>() == mName)
                                    mContent.erase(i);
                            }
                            data["classiflcation"] = mContent;
                            db->set(uiName, data);
                            db->save();

                            std::string logString = tr({}, "shop.log3");
                            ll::string_utils::replaceAll(logString, "${menu}", uiName);
                            ll::string_utils::replaceAll(logString, "${customize}", mName);
                            logger->info(translateString(logString, pl));
                        }
                    });
                });
            }
            form.sendTo(player, [uiName](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::editAwardContent(pl, uiName);
            });
        }

        void editAwardContent(Player& player, const std::string& uiName) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObjectLabel = tr(mObjectLanguage, "shop.gui.button3.label");
            std::string mObjectType = db->toJson(uiName).at("type").get<std::string>();

            ll::string_utils::replaceAll(mObjectLabel, "${menu}", uiName);
            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "shop.gui.button3.setting"), "textures/ui/icon_setting", "path", [uiName, mObjectType](Player& pl) -> void {
                switch (ll::hash_utils::doHash(mObjectType)) {
                    case ll::hash_utils::doHash("buy"):
                        MainGui::editAwardSetting(pl, uiName, ShopType::buy);
                        break;
                    case ll::hash_utils::doHash("sell"):
                        MainGui::editAwardSetting(pl, uiName, ShopType::sell);
                        break;
                };
            });
            form.appendButton(tr(mObjectLanguage, "shop.gui.button3.new"), "textures/ui/icon_sign", "path", [uiName, mObjectType](Player& pl) -> void {
                switch (ll::hash_utils::doHash(mObjectType)) {
                    case ll::hash_utils::doHash("buy"):
                        MainGui::editAwardNew(pl, uiName, ShopType::buy);
                        break;
                    case ll::hash_utils::doHash("sell"):
                        MainGui::editAwardNew(pl, uiName, ShopType::sell);
                        break;
                };
            });
            form.appendButton(tr(mObjectLanguage, "shop.gui.button3.remove"), "textures/ui/icon_trash", "path", [uiName](Player& pl) -> void {
                MainGui::editAwardRemove(pl, uiName);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::editAward(pl);
            });
        }

        void editAward(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
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

            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            form.appendButton(tr(mObjectLanguage, "shop.gui.button1"), "textures/ui/achievements", "path", [](Player& pl) -> void {
                MainGui::editNew(pl);
            });
            form.appendButton(tr(mObjectLanguage, "shop.gui.button2"), "textures/ui/world_glyph_color", "path", [](Player& pl) -> void {
                MainGui::editRemove(pl);
            });
            form.appendButton(tr(mObjectLanguage, "shop.gui.button3"), "textures/ui/editIcon", "path", [](Player& pl) -> void {
                MainGui::editAward(pl);
            });
            form.sendTo(player);
        }

        void menu(Player& player, nlohmann::ordered_json& data, ShopType type) {
            ll::form::SimpleForm form(translateString(data.at("title").get<std::string>(), player),
                translateString(data.at("content").get<std::string>(), player));
            for (nlohmann::ordered_json& item : data.at("classiflcation")) {
                form.appendButton(translateString(item.at("title").get<std::string>(), player), 
                    item.at("image").get<std::string>(), "path", [item, data, type](Player& pl) -> void {
                    nlohmann::ordered_json mItem = item;
                    switch (ll::hash_utils::doHash(mItem["type"].get<std::string>())) {
                        case ll::hash_utils::doHash("commodity"):
                            MainGui::commodity(pl, mItem, data, type);
                            break;
                        case ll::hash_utils::doHash("title"):
                            MainGui::title(pl, mItem, data, type);
                            break;
                        case ll::hash_utils::doHash("from"):
                            MainGui::open(pl, mItem.at("menu").get<std::string>());
                            break;
                        default:
                            logger->error("Unknown UI type {}.", mItem["type"].get<std::string>());
                            break;
                    };
                });
            }
            form.sendTo(player, [data](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return McUtils::executeCommand(pl, data.at("exit").get<std::string>());
            });
        }

        void commodity(Player& player, nlohmann::ordered_json& data, const nlohmann::ordered_json& original, ShopType type) {
            ll::form::CustomForm form(translateString(data.at("title").get<std::string>(), player));
            form.appendLabel(translateString(data.at("introduce").get<std::string>(), player));
            form.appendInput("Input", translateString(data.at("number").get<std::string>(), player), "", "1");
            form.sendTo(player, [data, original, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return McUtils::executeCommand(pl, original.at("exit").get<std::string>());

                int mNumber = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                if (mNumber > 2304) return;
                if (type == ShopType::buy) {
                    if (checkModifiedData(pl, data, mNumber)) {
                        ItemStack itemStack = data.contains("nbt") ? ItemStack::fromTag(CompoundTag::fromSnbt(data.at("nbt").get<std::string>())->mTags)
                            : ItemStack(data.at("id").get<std::string>(), 1, 0, nullptr);
                        McUtils::giveItem(pl, itemStack, mNumber);
                        pl.refreshInventory();
                        return;
                    }
                    return McUtils::executeCommand(pl, original.at("NoScore").get<std::string>());
                }
                if (McUtils::isItemPlayerInventory(pl, data.at("id").get<std::string>(), mNumber)) {
                    nlohmann::ordered_json mScoreboardBase = data.at("scores");
                    for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                        McUtils::scoreboard::addScore(pl, it.key(), (it.value().get<int>() * mNumber));
                    McUtils::clearItem(pl, data.at("id").get<std::string>(), mNumber);
                    pl.refreshInventory();
                    return;
                }
                McUtils::executeCommand(pl, original.at("NoItem").get<std::string>());
            });
        }

        void title(Player& player, nlohmann::ordered_json& data, const nlohmann::ordered_json& original, ShopType type) {
            ll::form::ModalForm form(translateString(data.at("title").get<std::string>(), player),
                translateString(data.at("introduce").get<std::string>(), player),
                translateString(data.at("confirmButton").get<std::string>(), player),
                translateString(data.at("cancelButton").get<std::string>(), player));
            form.sendTo(player, [data, original, type](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    std::string id = data.at("id").get<std::string>();
                    if (type == ShopType::buy) {
                        if (checkModifiedData(pl, data, 1)) {
                            if (data.contains("time"))
                                return chat::addChat(pl, id, data.at("time").get<int>());
                            return chat::addChat(pl, id, 0);
                        }
                        return McUtils::executeCommand(pl, original.at("NoScore").get<std::string>());
                    }
                    if (chat::isChat(pl, id)) {
                        nlohmann::ordered_json mScoreboardBase = data.at("scores");
                        for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                            McUtils::scoreboard::addScore(pl, it.key(), it.value().get<int>());
                        return chat::delChat(pl, id);
                    }
                    McUtils::executeCommand(pl, original.at("NoTitle").get<std::string>());
                }
            });
        }

        void open(Player& player, std::string uiName) {
            if (db->has(uiName)) {
                nlohmann::ordered_json data = db->toJson(uiName);

                if (data.empty())
                    return;
                
                switch (ll::hash_utils::doHash(data.at("type").get<std::string>())) {
                    case ll::hash_utils::doHash("buy"):
                        MainGui::menu(player, data, ShopType::buy);
                        break;
                    case ll::hash_utils::doHash("sell"):
                        MainGui::menu(player, data, ShopType::sell);
                        break;
                    default:
                        logger->error("Unknown UI type {}.", data.at("type").get<std::string>());
                        break;
                };
                return;
            }
            logger->error("ShopUI {} reading failed.", uiName);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("shop", tr({}, "commands.shop.description"), CommandPermissionLevel::Any);
            command.overload<ShopOP>().text("gui").required("uiName").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ShopOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player, param.uiName);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    output.error(tr({}, "commands.generic.permission"));
                
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::edit(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }
    }

    bool checkModifiedData(Player& player, nlohmann::ordered_json data, int number) {
        if (!isValid()) return true;

        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if ((it.value().get<int>() * number) > McUtils::scoreboard::getScore(player, it.key()))
                return false;
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            McUtils::scoreboard::reduceScore(player, it.key(), (it.value().get<int>() * number));
        return true;
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