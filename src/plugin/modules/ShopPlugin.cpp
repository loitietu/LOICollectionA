#include <memory>
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
#include <ll/api/utils/HashUtils.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

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

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/ChatPlugin.h"

#include "utils/InventoryUtils.h"
#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"

#include "include/ShopPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::shop {
    struct ShopOP {
        std::string Id;
    };

    std::unique_ptr<JsonStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    namespace MainGui {
        void editNewInfo(Player& player, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button1.input1"), tr(mObjectLanguage, "shop.gui.button1.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button1.input2"), tr(mObjectLanguage, "shop.gui.button1.input2.placeholder"));
            form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button1.input3"), tr(mObjectLanguage, "shop.gui.button1.input3.placeholder"));

            switch (type) {
                case ShopType::buy:
                    form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button1.input4"), tr(mObjectLanguage, "shop.gui.button1.input4.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button1.input5"), tr(mObjectLanguage, "shop.gui.button1.input5.placeholder"));
                    break;
                case ShopType::sell:
                    form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button1.input4"), tr(mObjectLanguage, "shop.gui.button1.input4.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button1.input6"), tr(mObjectLanguage, "shop.gui.button1.input6.placeholder"));
                    form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button1.input7"), tr(mObjectLanguage, "shop.gui.button1.input7.placeholder"));
                    break;
            };

            form.sendTo(player, [mObjectLanguage, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editNew(pl);
                
                std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
                std::string mObjectContent = std::get<std::string>(dt->at("Input3"));

                if (mObjectId.empty() || mObjectTitle.empty() || mObjectContent.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::editNew(pl);
                }

                nlohmann::ordered_json data = {
                    { "title", mObjectTitle },
                    { "content", mObjectContent },
                    { "info", nlohmann::ordered_json::object() },
                    { "classiflcation", nlohmann::ordered_json::array() }
                };

                switch (type) {
                    case ShopType::buy: {
                        data["type"] = "buy";
                        data["info"].update({
                            { "exit", std::get<std::string>(dt->at("Input4")) },
                            { "score", std::get<std::string>(dt->at("Input5")) }
                        });
                        break;
                    }
                    case ShopType::sell: {
                        data["type"] = "sell";
                        data["info"].update({
                            { "exit", std::get<std::string>(dt->at("Input4")) },
                            { "title", std::get<std::string>(dt->at("Input6")) },
                            { "item", std::get<std::string>(dt->at("Input7")) }
                        });
                        break;
                    }
                }

                if (!db->has(mObjectId))
                    db->set(mObjectId, data);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "shop.log1"), pl)), mObjectId);
            });
        }

        void editNew(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            form.appendButton("Buy", [](Player& pl) {
                MainGui::editNewInfo(pl, ShopType::buy);
            });
            form.appendButton("Sell", [](Player& pl) {
                MainGui::editNewInfo(pl, ShopType::sell);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void editRemoveInfo(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::ModalForm form(tr(mObjectLanguage, "shop.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "shop.gui.button2.content")), id),
                tr(mObjectLanguage, "shop.gui.button2.yes"), tr(mObjectLanguage, "shop.gui.button2.no")
            );
            form.sendTo(player, [id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result != ll::form::ModalFormSelectedButton::Upper)
                    return;

                db->remove(id);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "shop.log2"), pl)), id);
            });
        }

        void editRemove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            for (std::string& key : db->keys()) {
                form.appendButton(key, [key](Player& pl) -> void {
                    MainGui::editRemoveInfo(pl, key);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::edit(pl);
            });
        }

        void editAwardSetting(Player& player, const std::string& id, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
            form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button3.input2"), tr(mObjectLanguage, "shop.gui.button3.input2.placeholder"), db->get_ptr<std::string>("/" + id + "/title", ""));
            form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.input3"), tr(mObjectLanguage, "shop.gui.button3.input3.placeholder"), db->get_ptr<std::string>("/" + id + "/content", ""));
            form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.input4"), tr(mObjectLanguage, "shop.gui.button3.input4.placeholder"), db->get_ptr<std::string>("/" + id + "/info/exit", ""));

            switch (type) {
                case ShopType::buy:
                    form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.input5"), tr(mObjectLanguage, "shop.gui.button3.input5.placeholder"), db->get_ptr<std::string>("/" + id + "/info/score", ""));
                    break;
                case ShopType::sell:
                    form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.input6"), tr(mObjectLanguage, "shop.gui.button3.input6.placeholder"), db->get_ptr<std::string>("/" + id + "/info/title", ""));
                    form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.input7"), tr(mObjectLanguage, "shop.gui.button3.input7.placeholder"), db->get_ptr<std::string>("/" + id + "/info/item", ""));
                    break;
            };

            form.sendTo(player, [mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, id, type);

                std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
                std::string mObjectContent = std::get<std::string>(dt->at("Input3"));

                if (mObjectTitle.empty() || mObjectContent.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::editAwardSetting(pl, id, type);
                }

                db->set_ptr("/" + id + "/title", mObjectTitle);
                db->set_ptr("/" + id + "/content", mObjectContent);
                db->set_ptr("/" + id + "/info/exit", std::get<std::string>(dt->at("Input4")));

                switch (type) {
                    case ShopType::buy:
                        db->set_ptr("/" + id + "/info/score", std::get<std::string>(dt->at("Input5")));
                        break;
                    case ShopType::sell:
                        db->set_ptr("/" + id + "/info/title", std::get<std::string>(dt->at("Input6")));
                        db->set_ptr("/" + id + "/info/item", std::get<std::string>(dt->at("Input7")));
                        break;
                };

                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "shop.log4"), pl)), id);
            });
        }

        void editAwardNewInfo(Player& player, const std::string& id, ShopType type, AwardType awardType) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button3.new.input1"), tr(mObjectLanguage, "shop.gui.button3.new.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button3.new.input2"), tr(mObjectLanguage, "shop.gui.button3.new.input2.placeholder"));

            switch (awardType) {
                case AwardType::commodity:
                    form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), tr(mObjectLanguage, "shop.gui.button3.new.input3.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                    form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), tr(mObjectLanguage, "shop.gui.button3.new.input4.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), tr(mObjectLanguage, "shop.gui.button3.new.input5.placeholder"));
                    form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), tr(mObjectLanguage, "shop.gui.button3.new.input7.placeholder"));
                    break;
                case AwardType::title:
                    form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), tr(mObjectLanguage, "shop.gui.button3.new.input3.placeholder"));
                    form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                    
                    if (type == ShopType::buy)
                        form.appendInput("Input9", tr(mObjectLanguage, "shop.gui.button3.new.input9"), tr(mObjectLanguage, "shop.gui.button3.new.input9.placeholder"));
                    
                    form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), tr(mObjectLanguage, "shop.gui.button3.new.input4.placeholder"));
                    form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), tr(mObjectLanguage, "shop.gui.button3.new.input5.placeholder"));
                    form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), tr(mObjectLanguage, "shop.gui.button3.new.input7.placeholder"));
                    form.appendInput("Input8", tr(mObjectLanguage, "shop.gui.button3.new.input8"), tr(mObjectLanguage, "shop.gui.button3.new.input8.placeholder"));
                    break;
                case AwardType::from:
                    form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                    break;
            };

            form.sendTo(player, [mObjectLanguage, id, type, awardType](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::editAwardContent(pl, id, type);

                std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));
                std::string mObjectImage = std::get<std::string>(dt->at("Input2"));

                if (mObjectTitle.empty() || mObjectImage.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::editAwardContent(pl, id, type);
                }

                nlohmann::ordered_json data;
                switch (awardType) {
                    case AwardType::commodity: {
                        std::string mObjectObjective = std::get<std::string>(dt->at("Input4"));
                        std::string mObjectScore = std::get<std::string>(dt->at("Input5"));

                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "introduce", std::get<std::string>(dt->at("Input3")) },
                            { "number", std::get<std::string>(dt->at("Input7")) },
                            { "id", std::get<std::string>(dt->at("Input6")) },
                            { "scores", nlohmann::ordered_json::object() },
                            { "type", "commodity" }
                        });

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        break;
                    }
                    case AwardType::title: {
                        std::string mObjectObjective = std::get<std::string>(dt->at("Input4"));
                        std::string mObjectScore = std::get<std::string>(dt->at("Input5"));

                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "introduce", std::get<std::string>(dt->at("Input3")) },
                            { "confirmButton", std::get<std::string>(dt->at("Input8")) },
                            { "cancelButton", std::get<std::string>(dt->at("Input7")) },
                            { "id", std::get<std::string>(dt->at("Input6")) },
                            { "scores", nlohmann::ordered_json::object() },
                            { "type", "title" }
                        });

                        if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                            data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                        if (type == ShopType::buy)
                            data["time"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input9")), 0);

                        break;
                    }
                    case AwardType::from: {
                        data.update({
                            { "title", mObjectTitle },
                            { "image", mObjectImage },
                            { "id", std::get<std::string>(dt->at("Input6")) },
                            { "type", "from" }
                        });

                        break;
                    }
                };

                int mIndex = (int) db->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").size();

                db->set_ptr("/" + id + "/classiflcation/" + std::to_string(mIndex), data);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "shop.log5"), pl)), id);
            });
        }

        void editAwardNew(Player& player, const std::string& id, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            form.appendButton("Commodity", [id, type](Player& pl) -> void {
                MainGui::editAwardNewInfo(pl, id, type, AwardType::commodity);
            });
            form.appendButton("Title", [id, type](Player& pl) -> void {
                MainGui::editAwardNewInfo(pl, id, type, AwardType::title);
            });
            form.appendButton("From", [id, type](Player& pl) -> void {
                MainGui::editAwardNewInfo(pl, id, type, AwardType::from);
            });
            form.sendTo(player, [ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::editAwardContent(pl, ids, type);
            });
        }

        void editAwardSettingInfo(Player& player, const std::string& id, const std::string& packageid) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "shop.gui.button3.remove.content")), packageid),
                tr(mObjectLanguage, "shop.gui.button3.remove.yes"), tr(mObjectLanguage, "shop.gui.button3.remove.no")
            );
            form.sendTo(player, [id, packageid](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result != ll::form::ModalFormSelectedButton::Upper)
                    return;

                auto mContent = db->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation");
                for (int i = ((int) mContent.size() - 1); i >= 0; i--) {
                    if (mContent.at(i).value("title", "") == packageid)
                        mContent.erase(i);
                }

                db->set_ptr("/" + id + "/classiflcation", mContent);
                db->save();

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "shop.log3"), pl)), id, packageid);
            });
        }

        void editAwardRemove(Player& player, const std::string& id, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
            for (nlohmann::ordered_json& item : db->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation")) {
                std::string mName = item.value("title", "");
                form.appendButton(mName, [mName, id](Player& pl) -> void {
                    MainGui::editAwardSettingInfo(pl, id, mName);
                });
            }
            form.sendTo(player, [ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::editAwardContent(pl, ids, type);
            });
        }

        void editAwardContent(Player& player, const std::string& id, ShopType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), 
                fmt::format(fmt::runtime(tr(mObjectLanguage, "shop.gui.button3.label")), id)
            );
            form.appendButton(tr(mObjectLanguage, "shop.gui.button3.setting"), "textures/ui/icon_setting", "path", [id, type](Player& pl) -> void {
                MainGui::editAwardSetting(pl, id, type);
            });

            form.appendButton(tr(mObjectLanguage, "shop.gui.button3.new"), "textures/ui/icon_sign", "path", [id, type](Player& pl) -> void {
                MainGui::editAwardNew(pl, id, type);
            });
            form.appendButton(tr(mObjectLanguage, "shop.gui.button3.remove"), "textures/ui/icon_trash", "path", [id, type](Player& pl) -> void {
                MainGui::editAwardRemove(pl, id, type);
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
                    auto mObjectType = db->get_ptr<std::string>("/" + key + "/type", "buy");

                    switch (ll::hash_utils::doHash(mObjectType)) {
                        case ll::hash_utils::doHash("buy"):
                            MainGui::editAwardContent(pl, key, ShopType::buy);
                            break;
                        case ll::hash_utils::doHash("sell"):
                            MainGui::editAwardContent(pl, key, ShopType::sell);
                            break;
                    }
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
            ll::form::SimpleForm form(LOICollectionAPI::translateString(data.value("title", ""), player), LOICollectionAPI::translateString(data.value("content", ""), player));
            for (nlohmann::ordered_json& item : data.value("classiflcation", nlohmann::ordered_json())) {
                form.appendButton(LOICollectionAPI::translateString(item.value("title", ""), player), item.value("image", ""), "path", [item, data, type](Player& pl) -> void {
                    nlohmann::ordered_json mItem = item;
                    switch (ll::hash_utils::doHash(mItem.value("type", ""))) {
                        case ll::hash_utils::doHash("commodity"):
                            MainGui::commodity(pl, mItem, data, type);
                            break;
                        case ll::hash_utils::doHash("title"):
                            MainGui::title(pl, mItem, data, type);
                            break;
                        case ll::hash_utils::doHash("from"):
                            MainGui::open(pl, mItem.value("id", ""));
                            break;
                        default:
                            logger->error("Unknown UI type {}.", mItem.value("type", ""));
                            break;
                    };
                });
            }
            form.sendTo(player, [data](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));
            });
        }

        void commodity(Player& player, nlohmann::ordered_json& data, const nlohmann::ordered_json& original, ShopType type) {
            ll::form::CustomForm form(LOICollectionAPI::translateString(data.value("title", ""), player));
            form.appendLabel(LOICollectionAPI::translateString(data.value("introduce", ""), player));
            form.appendInput("Input", LOICollectionAPI::translateString(data.value("number", ""), player), "", "1");
            form.sendTo(player, [data, original, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("exit", ""));

                int mNumber = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                if (mNumber > 2304 || mNumber <= 0)
                    return;
                if (type == ShopType::buy) {
                    if (checkModifiedData(pl, data, mNumber)) {
                        ItemStack itemStack = data.contains("nbt") ? ItemStack::fromTag(CompoundTag::fromSnbt(data.value("nbt", ""))->mTags)
                            : ItemStack(data.value("id", ""), 1, 0, nullptr);
                        InventoryUtils::giveItem(pl, itemStack, mNumber);
                        pl.refreshInventory();
                        return;
                    }
                    return executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("score", ""));
                }
                if (InventoryUtils::isItemInInventory(pl, data.value("id", ""), mNumber)) {
                    nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json());
                    for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                        ScoreboardUtils::addScore(pl, it.key(), (it.value().get<int>() * mNumber));
                    InventoryUtils::clearItem(pl, data.value("id", ""), mNumber);
                    pl.refreshInventory();
                    return;
                }
                executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("item", ""));
            });
        }

        void title(Player& player, nlohmann::ordered_json& data, const nlohmann::ordered_json& original, ShopType type) {
            ll::form::ModalForm form(
                LOICollectionAPI::translateString(data.value("title", ""), player),
                LOICollectionAPI::translateString(data.value("introduce", ""), player),
                LOICollectionAPI::translateString(data.value("confirmButton", "confirm"), player),
                LOICollectionAPI::translateString(data.value("cancelButton", "cancel"), player)
            );
            form.sendTo(player, [data, original, type](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    std::string id = data.value("id", "None");
                    if (type == ShopType::buy) {
                        if (checkModifiedData(pl, data, 1)) {
                            if (data.contains("time"))
                                return chat::addTitle(pl, id, data.value("time", 0));
                            return chat::addTitle(pl, id, 0);
                        }
                        return executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("sScore", ""));
                    }
                    if (chat::isTitle(pl, id)) {
                        nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json());
                        for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                            ScoreboardUtils::addScore(pl, it.key(), it.value().get<int>());
                        return chat::delTitle(pl, id);
                    }
                    executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("title", ""));
                }
            });
        }

        void open(Player& player, std::string id) {
            if (db->has(id)) {
                auto data = db->get_ptr<nlohmann::ordered_json>("/" + id);

                if (data.empty())
                    return;
                
                switch (ll::hash_utils::doHash(data.value("type", ""))) {
                    case ll::hash_utils::doHash("buy"):
                        MainGui::menu(player, data, ShopType::buy);
                        break;
                    case ll::hash_utils::doHash("sell"):
                        MainGui::menu(player, data, ShopType::sell);
                        break;
                    default:
                        logger->error("Unknown UI type {}.", data.value("type", ""));
                        break;
                };
                return;
            }
            logger->error("ShopUI {} reading failed.", id);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("shop", tr({}, "commands.shop.description"), CommandPermissionLevel::Any);
            command.overload<ShopOP>().text("gui").required("Id").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ShopOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::open(player, param.Id);

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
        }
    }

    void executeCommand(Player& player, std::string cmd) {
        ll::string_utils::replaceAll(cmd, "${player}", std::string(player.mName));

        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        Command* command = ll::service::getMinecraft()->mCommands->compileCommand(
            HashedString(cmd), origin, (CurrentCmdVersion)CommandVersion::CurrentVersion(),
            [&](std::string const& message) -> void {
                logger->error(message + " >> Shop");
            }
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    bool checkModifiedData(Player& player, nlohmann::ordered_json data, int number) {
        if (!data.contains("scores") || !isValid())
            return true;

        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if ((it.value().get<int>() * number) > ScoreboardUtils::getScore(player, it.key()))
                return false;
        }
        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            ScoreboardUtils::reduceScore(player, it.key(), (it.value().get<int>() * number));
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

    void unregistery() {
        db->save();
    }
}