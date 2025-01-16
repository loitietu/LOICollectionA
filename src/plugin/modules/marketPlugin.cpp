#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
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

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

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

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::market {
    std::map<std::string, std::variant<std::string, int>> mObjectOptions;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void buyItem(Player& player, std::string mItemId) {
            std::string mObjectLanguage = getLanguage(player);
            
            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
            ll::string_utils::replaceAll(mIntroduce, "${introduce}", db->get(mItemId, "introduce"));
            ll::string_utils::replaceAll(mIntroduce, "${score}", db->get(mItemId, "score"));
            ll::string_utils::replaceAll(mIntroduce, "${nbt}", db->get(mItemId, "nbt"));
            ll::string_utils::replaceAll(mIntroduce, "${player}", db->get(mItemId, "player"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), mIntroduce);
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.buy.button1"), [mItemId](Player& pl) {
                std::string mObjectScore = std::get<std::string>(mObjectOptions.at("score"));
                int mScore = SystemUtils::toInt(db->get(mItemId, "score"), 0);
                if (McUtils::scoreboard::getScore(pl, mObjectScore) >= mScore) {
                    McUtils::scoreboard::reduceScore(pl, mObjectScore, mScore);
                    std::string mName = db->get(mItemId, "name");
                    std::string mNbt = db->get(mItemId, "nbt");

                    ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mNbt)->mTags);
                    pl.add(mItemStack);
                    pl.refreshInventory();

                    Player* mPlayer = ll::service::getLevel()->getPlayer(db->get(mItemId, "player"));
                    if (!mPlayer) {
                        std::string mObject = mItemId.substr(0, mItemId.find("$ITEMS"));
                        db->set(mObject, "score", std::to_string(
                            SystemUtils::toInt(db->get(mObject, "score"), 0) + mScore
                        ));
                    } else {
                        mPlayer->sendMessage(ll::string_utils::replaceAll(tr(getLanguage(pl),
                            "market.gui.sell.sellItem.tips1"), "${item}", mName));
                        McUtils::scoreboard::addScore(*mPlayer, mObjectScore, mScore);
                    }
                    delItem(mItemId);

                    logger->info(LOICollection::LOICollectionAPI::translateString(
                        ll::string_utils::replaceAll(tr({}, "market.log1"), "${item}", mName), pl
                    ));
                } else {
                    pl.sendMessage(tr(getLanguage(pl), "market.gui.sell.sellItem.tips3"));
                }
            });
            if ((int) player.getPlayerPermissionLevel() >= 2) {
                form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [mItemId](Player& pl) {
                    std::string mName = db->get(mItemId, "name");
                    pl.sendMessage(ll::string_utils::replaceAll(tr(getLanguage(pl), 
                        "market.gui.sell.sellItem.tips2"), "${item}", mName));
                    delItem(mItemId);

                    logger->info(LOICollection::LOICollectionAPI::translateString(
                        ll::string_utils::replaceAll(tr({}, "market.log3"), "${item}", mName), pl
                    ));
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::buy(pl);
            });
        }

        void itemContent(Player& player, std::string mItemId) {
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

                pl.sendMessage(ll::string_utils::replaceAll(tr(getLanguage(pl),
                    "market.gui.sell.sellItem.tips2"), "${item}", mName));
                
                ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mNbt)->mTags);
                pl.add(mItemStack);
                pl.refreshInventory();
                delItem(mItemId);

                logger->info(LOICollection::LOICollectionAPI::translateString(
                    ll::string_utils::replaceAll(tr({}, "market.log3"), "${item}", mName), pl
                ));
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::sellItemContent(pl);
            });
        }

        void sellItem(Player& player, std::string mNbt, int mSlot) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            int mSize = std::get<int>(mObjectOptions.at("upload"));
            if (((int) db->list("OBJECT$" + mObject + "$ITEMS").size()) >= mSize) {
                return player.sendMessage(ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "market.gui.sell.sellItem.tips4"), "${size}", std::to_string(mSize)
                ));
            }

            ll::form::CustomForm form(tr(mObjectLanguage, "market.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "market.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "market.gui.sell.sellItem.input1"), "", "Item");
            form.appendInput("Input2", tr(mObjectLanguage, "market.gui.sell.sellItem.input2"), "", "textures/items/diamond");
            form.appendInput("Input3", tr(mObjectLanguage, "market.gui.sell.sellItem.input3"), "", "Introduce");
            form.appendInput("Input4", tr(mObjectLanguage, "market.gui.sell.sellItem.input4"), "", "100");
            form.sendTo(player, [mNbt, mSlot](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::sellItemInventory(pl);

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                std::string mItemName = std::get<std::string>(dt->at("Input1"));
                std::string mItemIcon = std::get<std::string>(dt->at("Input2"));
                std::string mItemIntroduce = std::get<std::string>(dt->at("Input3"));

                std::string mItemId = SystemUtils::nest<std::string>([mObject](std::string& val, int loop) {
                    val = val + std::to_string(loop);
                    return db->has("OBJECT$" + mObject + "$ITEMS", val);
                }, "Item", 1);
                std::string mItemListId = "OBJECT$" + mObject + "$ITEMS_$LIST_" + mItemId;

                db->set("OBJECT$" + mObject + "$ITEMS", mItemId, "LIST");
                db->create(mItemListId);
                db->set(mItemListId, "name", mItemName);
                db->set(mItemListId, "icon", mItemIcon);
                db->set(mItemListId, "introduce", mItemIntroduce);
                db->set(mItemListId, "score", std::to_string(
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0)
                ));
                db->set(mItemListId, "nbt", mNbt);
                db->set(mItemListId, "player", pl.getRealName());
                db->set("Item", mItemListId, mObject);
                pl.getInventory().removeItem(mSlot, 64);
                pl.refreshInventory();

                logger->info(LOICollection::LOICollectionAPI::translateString(
                    ll::string_utils::replaceAll(tr({}, "market.log2"), "${item}", mItemName), pl
                ));
            });
        }

        void sellItemInventory(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            Container& mItemInventory = player.getInventory();

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"),
                tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown"));
            for (int i = 0; i < mItemInventory.getContainerSize(); i++) {
                ItemStack mItemStack = mItemInventory.getItem(i);
                
                if (!mItemStack || mItemStack.isNull())
                    continue;

                std::string mItemName = tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown.text");
                ll::string_utils::replaceAll(mItemName, "${item}", mItemStack.getName());
                ll::string_utils::replaceAll(mItemName, "${count}", std::to_string(mItemStack.mCount));
                form.appendButton(mItemName, [mItemStack, i](Player& pl) {
                    MainGui::sellItem(pl, mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(
                        SnbtFormat::Minimize, 0), i);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) return MainGui::sell(pl);
            });
        }

        void sellItemContent(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"),
                tr(mObjectLanguage, "market.gui.label"));
            for (auto& item : db->list("OBJECT$" + mObject + "$ITEMS")) {
                std::string mName = db->get("OBJECT$" + mObject + "$ITEMS_$LIST_" + item, "name");
                std::string mIcon = db->get("OBJECT$" + mObject + "$ITEMS_$LIST_" + item, "icon");
                form.appendButton(mName, mIcon, "path", [mObject, item](Player& pl) {
                    MainGui::itemContent(pl, ("OBJECT$" + mObject + "$ITEMS_$LIST_" + item));
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) return MainGui::sell(pl);
            });
        }

        void sell(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItem"), "textures/items/diamond", "path", [](Player& pl) {
                MainGui::sellItemInventory(pl);
            });
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent"), "textures/items/diamond_axe", "path", [](Player& pl) {
                MainGui::sellItemContent(pl);
            });
            form.sendTo(player);
        }

        void buy(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"),
                tr(mObjectLanguage, "market.gui.label"));
            for (auto& item : db->list("Item")) {
                std::string mName = db->get(item, "name");
                std::string mIcon = db->get(item, "icon");
                form.appendButton(mName, mIcon, "path", [item](Player& pl) {
                    MainGui::buyItem(pl, item);
                });
            }
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("market", "§e§lLOICollection -> §b玩家市场", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::buy(player);

                output.success("The UI has been opened to player {}", player.getRealName());
            });
            command.overload().text("sell").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::sell(player);

                output.success("The UI has been opened to player {}", player.getRealName());
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
                    if (!db->has("Item")) db->create("Item");
                    if (!db->has("OBJECT$" + mObject)) {
                        db->create("OBJECT$" + mObject);
                        db->create("OBJECT$" + mObject + "$ITEMS");
                        db->set("OBJECT$" + mObject, "score", "0");
                        return;
                    }
                    int mScore = SystemUtils::toInt(db->get("OBJECT$" + mObject, "score"), 0);
                    if (mScore > 0) {
                        McUtils::scoreboard::addScore(event.self(), std::get<std::string>(mObjectOptions.at("score")), mScore);
                        db->set("OBJECT$" + mObject, "score", "0");
                    }
                }
            );
        }
    }

    void delItem(std::string mItemId) {
        if (!isValid()) return;

        auto mIdList = ll::string_utils::splitByPattern(mItemId, "_$LIST_");
        db->del(mIdList[0], mIdList[1]);
        db->del("Item", mItemId);
        db->remove(mItemId);
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database, std::map<std::string, std::variant<std::string, int>>& options) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        mObjectOptions = options;
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}