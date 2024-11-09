#include <map>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/item/registry/ItemStack.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/marketPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::market {
    std::map<std::string, std::string> mObjectOptions;
    std::unique_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - Market");

    namespace MainGui {
        void buyItem(void* player_ptr, std::string mItemId) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
            ll::string_utils::replaceAll(mIntroduce, "${introduce}", db->get(mItemId, "introduce"));
            ll::string_utils::replaceAll(mIntroduce, "${score}", db->get(mItemId, "score"));
            ll::string_utils::replaceAll(mIntroduce, "${nbt}", db->get(mItemId, "nbt"));
            ll::string_utils::replaceAll(mIntroduce, "${player}", db->get(mItemId, "player"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), mIntroduce);
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.buy.button1"), [mItemId](Player& pl) {
                std::string mObjectScore = mObjectOptions.at("score");
                int mScore = SystemUtils::toInt(db->get(mItemId, "score"), 0);
                if (McUtils::scoreboard::getScore(&pl, mObjectScore) >= mScore) {
                    McUtils::scoreboard::reduceScore(&pl, mObjectScore, mScore);
                    std::string mName = db->get(mItemId, "name");
                    std::string mNbt = db->get(mItemId, "nbt");
                    ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mNbt)->mTags);
                    pl.add(mItemStack);
                    pl.refreshInventory();

                    Player* mPlayer = McUtils::getPlayerFromName(db->get(mItemId, "player"));
                    if (mPlayer == nullptr) {
                        std::string mObject = pl.getUuid().asString();
                        std::replace(mObject.begin(), mObject.end(), '-', '_');
                        int mPlayerScore = SystemUtils::toInt(db->get("OBJECT$" + mObject, "score"), 0) + mScore;
                        db->set("OBJECT$" + mObject, "score", std::to_string(mPlayerScore));
                    } else {
                        std::string mTips = tr(getLanguage(&pl), "market.gui.sell.sellItem.tips2");
                        mPlayer->sendMessage(ll::string_utils::replaceAll(mTips, "${item}", mName));
                        McUtils::scoreboard::addScore(mPlayer, mObjectScore, mScore);
                    }
                    delItem(mItemId);
                    
                    McUtils::Gui::submission(&pl, [](Player* player) {
                        return MainGui::buy(player);
                    });

                    std::string logString = tr(getLanguage(&pl), "market.log1");
                    ll::string_utils::replaceAll(logString, "${item}", mName);
                    logger.info(LOICollection::LOICollectionAPI::translateString(logString, &pl));
                    return;
                }
                pl.sendMessage(tr(getLanguage(&pl), "market.gui.sell.sellItem.tips4"));
                MainGui::buy(&pl);
            });
            if ((int) player->getPlayerPermissionLevel() >= 2) {
                form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [mItemId](Player& pl) {
                    std::string mName = db->get(mItemId, "name");
                    std::string mTips = tr(getLanguage(&pl), "market.gui.sell.sellItem.tips3");
                    pl.sendMessage(ll::string_utils::replaceAll(mTips, "${item}", mName));
                    delItem(mItemId);

                    McUtils::Gui::submission(&pl, [](Player* player) {
                        return MainGui::buy(player);
                    });

                    std::string logString = tr(getLanguage(&pl), "market.log3");
                    ll::string_utils::replaceAll(logString, "${item}", mName);
                    logger.info(LOICollection::LOICollectionAPI::translateString(logString, &pl));
                });
            }
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::buy(&pl);
            });
        }

        void itemContent(void* player_ptr, std::string mItemId) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
            ll::string_utils::replaceAll(mIntroduce, "${introduce}", db->get(mItemId, "introduce"));
            ll::string_utils::replaceAll(mIntroduce, "${score}", db->get(mItemId, "score"));
            ll::string_utils::replaceAll(mIntroduce, "${nbt}", db->get(mItemId, "nbt"));
            ll::string_utils::replaceAll(mIntroduce, "${player}", db->get(mItemId, "player"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), mIntroduce);
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [mItemId](Player& pl) {
                std::string mName = db->get(mItemId, "name");
                std::string mNbt = db->get(mItemId, "nbt");
                std::string mTips = tr(getLanguage(&pl), "market.gui.sell.sellItem.tips3");
                
                ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mNbt)->mTags);
                pl.sendMessage(ll::string_utils::replaceAll(mTips, "${item}", mName));
                pl.add(mItemStack);
                pl.refreshInventory();
                delItem(mItemId);

                McUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::sellItemContent(player);
                });

                std::string logString = tr(getLanguage(&pl), "market.log3");
                ll::string_utils::replaceAll(logString, "${item}", mName);
                logger.info(LOICollection::LOICollectionAPI::translateString(logString, &pl));
            });
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::sellItemContent(&pl);
            });
        }

        void sellItem(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "market.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "market.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "market.gui.sell.sellItem.input1"), "", "Item");
            form.appendInput("Input2", tr(mObjectLanguage, "market.gui.sell.sellItem.input2"), "", "textures/items/diamond");
            form.appendInput("Input3", tr(mObjectLanguage, "market.gui.sell.sellItem.input3"), "", "Introduce");
            form.appendInput("Input4", tr(mObjectLanguage, "market.gui.sell.sellItem.input4"), "", "100");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::sell(&pl);

                if (pl.getCarriedItem().isValid()) {
                    std::string mObject = pl.getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');

                    std::string mItemName = std::get<std::string>(dt->at("Input1"));
                    std::string mItemIcon = std::get<std::string>(dt->at("Input2"));
                    std::string mItemIntroduce = std::get<std::string>(dt->at("Input3"));
                    int mItemScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);

                    std::string mItemId = db->find("OBJECT$" + mObject + "$ITEMS", "Item", 1);
                    std::string mItemListId = "OBJECT$" + mObject + "$ITEMS_$LIST_" + mItemId;

                    ItemStack mItemStack = pl.getCarriedItem();
                    std::unique_ptr<CompoundTag> mNbt = mItemStack.save();
                    db->set("OBJECT$" + mObject + "$ITEMS", mItemId, "LIST");
                    db->create(mItemListId);
                    db->set(mItemListId, "name", mItemName);
                    db->set(mItemListId, "icon", mItemIcon);
                    db->set(mItemListId, "introduce", mItemIntroduce);
                    db->set(mItemListId, "score", std::to_string(mItemScore));
                    db->set(mItemListId, "nbt", mNbt->toSnbt(SnbtFormat::Minimize, 0));
                    db->set(mItemListId, "player", pl.getRealName());
                    db->set("Item", mItemListId, mObject);
                    mItemStack.remove(mItemStack.mCount);
                    pl.setCarriedItem(mItemStack);
                    pl.refreshInventory();

                    McUtils::Gui::submission(&pl, [](Player* player) {
                        return MainGui::sellItem(player);
                    });

                    std::string logString = tr(getLanguage(&pl), "market.log2");
                    ll::string_utils::replaceAll(logString, "${item}", mItemName);
                    logger.info(LOICollection::LOICollectionAPI::translateString(logString, &pl));
                    return;
                }
                pl.sendMessage(tr(getLanguage(&pl), "market.gui.sell.sellItem.tips1"));
                MainGui::sellItem(&pl);
            });
        }

        void sellItemContent(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            std::vector<std::string> mItems;
            std::string mObject = player->getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"));
            form.setContent(tr(mObjectLanguage, "market.gui.label"));
            for (auto& item : db->list("OBJECT$" + mObject + "$ITEMS")) {
                std::string mName = db->get("OBJECT$" + mObject + "$ITEMS_$LIST_" + item, "name");
                std::string mIcon = db->get("OBJECT$" + mObject + "$ITEMS_$LIST_" + item, "icon");
                form.appendButton(mName, mIcon, "path");
                mItems.push_back(item);
            }
            form.sendTo(*player, [mItems, mObject](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) return MainGui::sell(&pl);
                
                MainGui::itemContent(&pl, ("OBJECT$" + mObject + "$ITEMS_$LIST_" + mItems.at(id)));
            });
        }

        void sell(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItem"), "textures/items/diamond", "path", [](Player& pl) {
                MainGui::sellItem(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent"), "textures/items/diamond_axe", "path", [](Player& pl) {
                MainGui::sellItemContent(&pl);
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }

        void buy(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            std::vector<std::string> mItems;
            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"));
            form.setContent(tr(mObjectLanguage, "market.gui.label"));
            for (auto& item : db->list("Item")) {
                std::string mName = db->get(item, "name");
                std::string mIcon = db->get(item, "icon");
                form.appendButton(mName, mIcon, "path");
                mItems.push_back(item);
            }
            form.sendTo(*player, [mItems](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) return pl.sendMessage(tr(getLanguage(&pl), "exit"));

                MainGui::buyItem(&pl, mItems.at(id));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("market", "§e§lLOICollection -> §b玩家市场", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::buy(player);
            });
            command.overload().text("sell").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::sell(player);
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (!db->has("Item")) 
                        db->create("Item");
                    if (!db->has("OBJECT$" + mObject)) {
                        db->create("OBJECT$" + mObject);
                        db->create("OBJECT$" + mObject + "$ITEMS");
                        db->set("OBJECT$" + mObject, "score", "0");
                        return;
                    }
                    int mScore = SystemUtils::toInt(db->get("OBJECT$" + mObject, "score"), 0);
                    if (mScore > 0) {
                        std::string mObjectScore = mObjectOptions.at("score");
                        McUtils::scoreboard::addScore(&event.self(), mObjectScore, mScore);
                        db->set("OBJECT$" + mObject, "score", "0");
                    }
                }
            );
        }
    }

    void delItem(std::string mItemId) {
        auto mIdList = ll::string_utils::splitByPattern(mItemId, "_$LIST_");
        db->del(mIdList[0], mIdList[1]);
        db->del("Item", mItemId);
        db->remove(mItemId);
    }

    void registery(void* database, std::map<std::string, std::string>& options) {
        mObjectOptions = options;

        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}