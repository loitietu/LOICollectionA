#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>

#include <fmt/core.h>

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
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/InventoryUtils.h"
#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/MarketPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::market {
    std::map<std::string, std::variant<std::string, int, std::vector<std::string>>> mObjectOptions;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<SQLiteStorage> db2;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void buyItem(Player& player, const std::string& mItemId) {
            std::string mObjectLanguage = getLanguage(player);
            
            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
            ll::string_utils::replaceAll(mIntroduce, "${introduce}", db->get(mItemId, "introduce"));
            ll::string_utils::replaceAll(mIntroduce, "${score}", db->get(mItemId, "score"));
            ll::string_utils::replaceAll(mIntroduce, "${nbt}", db->get(mItemId, "nbt"));
            ll::string_utils::replaceAll(mIntroduce, "${player}", db->get(mItemId, "player"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), mIntroduce);
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.buy.button1"), [mItemId](Player& pl) -> void {
                std::string mObjectScore = std::get<std::string>(mObjectOptions.at("score"));
                int mScore = SystemUtils::toInt(db->get(mItemId, "score"), 0);
                if (ScoreboardUtils::getScore(pl, mObjectScore) >= mScore) {
                    ScoreboardUtils::reduceScore(pl, mObjectScore, mScore);
                    std::string mName = db->get(mItemId, "name");
                    std::string mNbt = db->get(mItemId, "nbt");

                    ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mNbt)->mTags);
                    InventoryUtils::giveItem(pl, mItemStack, (int)mItemStack.mCount);
                    pl.refreshInventory();

                    Player* mPlayer = ll::service::getLevel()->getPlayer(db->get(mItemId, "player"));
                    if (!mPlayer) {
                        std::string mObject = mItemId.substr(0, mItemId.find("$ITEMS"));
                        db2->set(mObject, "Score", std::to_string(
                            SystemUtils::toInt(db2->get(mObject, "Score"), 0) + mScore
                        ));
                    } else {
                        mPlayer->sendMessage(ll::string_utils::replaceAll(tr(getLanguage(pl),
                            "market.gui.sell.sellItem.tips1"), "${item}", mName));
                        ScoreboardUtils::addScore(*mPlayer, mObjectScore, mScore);
                    }
                    delItem(mItemId);

                    logger->info(LOICollectionAPI::translateString(
                        ll::string_utils::replaceAll(tr({}, "market.log1"), "${item}", mName), pl
                    ));
                    return;
                }
                pl.sendMessage(tr(getLanguage(pl), "market.gui.sell.sellItem.tips3"));
            });
            if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) {
                form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [mItemId](Player& pl) -> void {
                    std::string mName = db->get(mItemId, "name");
                    pl.sendMessage(ll::string_utils::replaceAll(tr(getLanguage(pl), 
                        "market.gui.sell.sellItem.tips2"), "${item}", mName));
                    delItem(mItemId);

                    logger->info(LOICollectionAPI::translateString(
                        ll::string_utils::replaceAll(tr({}, "market.log3"), "${item}", mName), pl
                    ));
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::buy(pl);
            });
        }

        void itemContent(Player& player, const std::string& mItemId) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
            ll::string_utils::replaceAll(mIntroduce, "${introduce}", db->get(mItemId, "introduce"));
            ll::string_utils::replaceAll(mIntroduce, "${score}", db->get(mItemId, "score"));
            ll::string_utils::replaceAll(mIntroduce, "${nbt}", db->get(mItemId, "nbt"));
            ll::string_utils::replaceAll(mIntroduce, "${player}", db->get(mItemId, "player"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), mIntroduce);
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [mItemId](Player& pl) -> void {
                std::string mName = db->get(mItemId, "name");
                std::string mNbt = db->get(mItemId, "nbt");

                pl.sendMessage(ll::string_utils::replaceAll(tr(getLanguage(pl),
                    "market.gui.sell.sellItem.tips2"), "${item}", mName));
                
                ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mNbt)->mTags);
                InventoryUtils::giveItem(pl, mItemStack, (int)mItemStack.mCount);
                pl.refreshInventory();
                delItem(mItemId);

                logger->info(LOICollectionAPI::translateString(
                    ll::string_utils::replaceAll(tr({}, "market.log3"), "${item}", mName), pl
                ));
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::sellItemContent(pl);
            });
        }

        void sellItem(Player& player, const std::string& mNbt, int mSlot) {
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
            form.sendTo(player, [mNbt, mSlot](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::sellItemInventory(pl);

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                std::string mItemName = std::get<std::string>(dt->at("Input1"));
                std::string mItemIcon = std::get<std::string>(dt->at("Input2"));
                std::string mItemIntroduce = std::get<std::string>(dt->at("Input3"));

                std::string mItemId = "Item" + SystemUtils::getCurrentTimestamp();
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
                pl.mInventory->mInventory->removeItem(mSlot, 64);
                pl.refreshInventory();

                logger->info(LOICollectionAPI::translateString(
                    ll::string_utils::replaceAll(tr({}, "market.log2"), "${item}", mItemName), pl
                ));
            });
        }

        void sellItemInventory(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            auto& mItemInventory = player.mInventory->mInventory;

            std::vector<std::string> ProhibitedItems = std::get<std::vector<std::string>>(mObjectOptions.at("items"));
            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"),
                tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown"));
            for (int i = 0; i < mItemInventory->getContainerSize(); i++) {
                ItemStack mItemStack = mItemInventory->getItem(i);
                
                if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                    continue;

                std::string mItemName = tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown.text");
                ll::string_utils::replaceAll(mItemName, "${item}", mItemStack.getName());
                ll::string_utils::replaceAll(mItemName, "${count}", std::to_string(mItemStack.mCount));
                form.appendButton(mItemName, [mItemStack, i](Player& pl) -> void {
                    MainGui::sellItem(pl, mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0), i);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::sell(pl);
            });
        }

        void sellItemContent(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            std::string mTableId = "OBJECT$" + mObject + "$ITEMS_$LIST_";
            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"),
                tr(mObjectLanguage, "market.gui.label"));
            for (std::string& item : db->list("OBJECT$" + mObject + "$ITEMS")) {
                std::string mName = db->get(mTableId + item, "name");
                std::string mIcon = db->get(mTableId + item, "icon");
                form.appendButton(mName, mIcon, "path", [item, mTableId](Player& pl) -> void {
                    MainGui::itemContent(pl, (mTableId + item));
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::sell(pl);
            });
        }

        void blacklistSet(Player& player, const std::string& target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            std::string mObjectLabel = tr(mObjectLanguage, "market.gui.sell.blacklist.set.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("OBJECT$" + mObject + "$PLAYER" + target, "name", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(
                db->get("OBJECT$" + mObject + "$PLAYER" + target, "time", "None"), "None"
            ));

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.set.remove"), [target](Player& pl) -> void {
                delBlacklist(pl, target);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::blacklist(pl);
            });
        }

        void blacklistAdd(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            int mSize = std::get<int>(mObjectOptions.at("blacklist"));
            if (((int) getBlacklist(player.getUuid().asString()).size()) >= mSize) {
                return player.sendMessage(ll::string_utils::replaceAll(
                    tr(mObjectLanguage, "market.gui.sell.sellItem.tips5"), "${size}", std::to_string(mSize)
                ));
            }
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.sell.blacklist.add.label"));
            ll::service::getLevel()->forEachPlayer([&form, &player](Player& mTarget) -> bool {
                if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    addBlacklist(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::blacklist(pl);
            });
        }

        void blacklist(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.add"), "textures/ui/editIcon", "path", [](Player& pl) -> void {
                MainGui::blacklistAdd(pl);
            });
            for (std::string& mTarget : getBlacklist(player.getUuid().asString())) {
                form.appendButton(mTarget, [mTarget](Player& pl) -> void {
                    MainGui::blacklistSet(pl, mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::sell(pl);
            });
        }

        void sell(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItem"), "textures/ui/icon_blackfriday", "path", [](Player& pl) -> void {
                MainGui::sellItemInventory(pl);
            });
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent"), "textures/ui/creative_icon", "path", [](Player& pl) -> void {
                MainGui::sellItemContent(pl);
            });
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist"), "textures/ui/icon_deals", "path", [](Player& pl) -> void {
                MainGui::blacklist(pl);
            });
            form.sendTo(player);
        }

        void buy(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mUuid = player.getUuid().asString();
            std::replace(mUuid.begin(), mUuid.end(), '-', '_');

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"),
                tr(mObjectLanguage, "market.gui.label"));
            for (std::string& item : db->list("Item")) {
                std::vector<std::string> mList = getBlacklist(db->get("Item", item));
                if (std::find(mList.begin(), mList.end(), mUuid) != mList.end())
                    continue;

                std::string mName = db->get(item, "name");
                std::string mIcon = db->get(item, "icon");
                form.appendButton(mName, mIcon, "path", [item](Player& pl) -> void {
                    MainGui::buyItem(pl, item);
                });
            }
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("market", tr({}, "commands.market.description"), CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::buy(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("sell").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::sell(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (!db->has("Item")) db->create("Item");
                    if (!db->has("OBJECT$" + mObject + "$ITEMS"))
                        db->create("OBJECT$" + mObject + "$ITEMS");
                    if (!db->has("OBJECT$" + mObject + "$PLAYER"))
                        db->create("OBJECT$" + mObject + "$PLAYER");
                    
                    if (!db2->has("OBJECT$" + mObject)) db2->create("OBJECT$" + mObject);
                    if (!db2->has("OBJECT$" + mObject, "Score"))
                        db2->set("OBJECT$" + mObject, "Score", "0");

                    int mScore = SystemUtils::toInt(db2->get("OBJECT$" + mObject, "Score"), 0);
                    if (mScore > 0) {
                        ScoreboardUtils::addScore(event.self(), std::get<std::string>(mObjectOptions.at("score")), mScore);
                        db2->set("OBJECT$" + mObject, "Score", "0");
                    }
                }
            );
        }
    }

    void addBlacklist(Player& player, Player& target) {
        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        db->set("OBJECT$" + mObject + "$PLAYER", mTargetObject, "blacklist");
        
        db->create("OBJECT$" + mObject + "$PLAYER" + mTargetObject);
        db->set("OBJECT$" + mObject + "$PLAYER" + mTargetObject, "name", target.getRealName());
        db->set("OBJECT$" + mObject + "$PLAYER" + mTargetObject, "time", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "market.log4"), "${target}", mTargetObject), player
        ));
    }

    void delBlacklist(Player& player, const std::string& target) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        db->del("OBJECT$" + mObject + "$PLAYER", target);
        db->remove("OBJECT$" + mObject + "$PLAYER" + target);

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "market.log5"), "${target}", target), player
        ));
    }

    void delItem(const std::string& mItemId) {
        if (!isValid()) return;

        auto mIdList = ll::string_utils::splitByPattern(mItemId, "_$LIST_");
        db->del(mIdList[0], mIdList[1]);
        db->del("Item", mItemId);
        db->remove(mItemId);
    }

    std::vector<std::string> getBlacklist(std::string target) {
        if (!isValid()) return {};

        std::replace(target.begin(), target.end(), '-', '_');
        return db->list("OBJECT$" + target + "$PLAYER");
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database, void* setting, std::map<std::string, std::variant<std::string, int, std::vector<std::string>>>& options) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        mObjectOptions = std::move(options);
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}