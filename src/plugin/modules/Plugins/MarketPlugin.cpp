#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

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
#include "include/Plugins/LanguagePlugin.h"

#include "utils/InventoryUtils.h"
#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "ConfigPlugin.h"

#include "include/Plugins/MarketPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::market {
    C_Config::C_Plugins::C_Market options;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<SQLiteStorage> db2;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void buyItem(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);
            
            std::unordered_map<std::string, std::string> mData = db->getByPrefix("Item", id + ".");

            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
                fmt::format(fmt::runtime(mIntroduce), 
                    mData.at(id + ".INTRODUCE"),
                    mData.at(id + ".SCORE"),
                    mData.at(id + ".DATA"),
                    mData.at(id + ".PLAYER_NAME")
                )
            );
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.buy.button1"), [id, mObjectLanguage, mData](Player& pl) -> void {
                int mScore = SystemUtils::toInt(mData.at(id + ".SCORE"), 0);

                if (ScoreboardUtils::getScore(pl, options.TargetScoreboard) < mScore) {
                    pl.sendMessage(tr(mObjectLanguage, "market.gui.sell.sellItem.tips3"));
                    return MainGui::buy(pl);
                }

                ScoreboardUtils::reduceScore(pl, options.TargetScoreboard, mScore);

                ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at(id + ".DATA"))->mTags);
                InventoryUtils::giveItem(pl, mItemStack, (int)mItemStack.mCount);

                pl.refreshInventory();

                std::string mObject = mData.at(id + ".PLAYER_UUID");
                if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
                    mPlayer->sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips1")), mData.at(id + ".NAME")));

                    ScoreboardUtils::addScore(*mPlayer, options.TargetScoreboard, mScore);
                } else {
                    int mMarketScore = SystemUtils::toInt(db2->get(mObject, "Market_Score"), 0);

                    db2->set(mObject, "Market_Score", std::to_string(mMarketScore + mScore));
                }

                delItem(id);

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log2"), pl)), mData.at(id + ".NAME"));
            });
            if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) {
                form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [id, mObjectLanguage, mData](Player& pl) -> void {
                    pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips2")), mData.at(id + ".NAME")));
                    
                    delItem(id);

                    logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log3"), pl)), mData.at(id + ".NAME"));
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::buy(pl);
            });
        }

        void itemContent(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            std::unordered_map<std::string, std::string> mData = db->getByPrefix("Item", id + ".");

            std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
                fmt::format(fmt::runtime(mIntroduce),
                    mData.at(id + ".INTRODUCE"),
                    mData.at(id + ".SCORE"),
                    mData.at(id + ".DATA"),
                    mData.at(id + ".PLAYER_NAME")
                )
            );
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [id, mObjectLanguage, mData](Player& pl) -> void {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips2")), mData.at(id + ".NAME")));
                
                ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at(id + ".DATA"))->mTags);
                InventoryUtils::giveItem(pl, mItemStack, (int)mItemStack.mCount);

                pl.refreshInventory();

                delItem(id);

                logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log3"), pl)), mData.at(id + ".NAME"));
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::sellItemContent(pl);
            });
        }

        void sellItem(Player& player, int mSlot) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "market.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "market.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "market.gui.sell.sellItem.input1"), tr(mObjectLanguage, "market.gui.sell.sellItem.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "market.gui.sell.sellItem.input2"), tr(mObjectLanguage, "market.gui.sell.sellItem.input2.placeholder"));
            form.appendInput("Input3", tr(mObjectLanguage, "market.gui.sell.sellItem.input3"), tr(mObjectLanguage, "market.gui.sell.sellItem.input3.placeholder"));
            form.appendInput("Input4", tr(mObjectLanguage, "market.gui.sell.sellItem.input4"), tr(mObjectLanguage, "market.gui.sell.sellItem.input4.placeholder"));
            form.sendTo(player, [mSlot, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::sellItemInventory(pl);

                std::string mItemName = std::get<std::string>(dt->at("Input1"));
                std::string mItemIcon = std::get<std::string>(dt->at("Input2"));
                std::string mItemIntroduce = std::get<std::string>(dt->at("Input3"));

                if (mItemName.empty() || mItemIcon.empty() || mItemIntroduce.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::sellItemInventory(pl);
                }

                int mItemScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);

                ItemStack mItemStack = pl.mInventory->mInventory->getItem(mSlot);

                addItem(pl, mItemStack, mItemName, mItemIcon, mItemIntroduce, mItemScore);

                pl.mInventory->mInventory->removeItem(mSlot, 64);
                pl.refreshInventory();
            });
        }

        void sellItemInventory(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::vector<std::string> ProhibitedItems = options.ProhibitedItems;

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown"));
            for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
                ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
                
                if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                    continue;

                std::string mItemName = fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown.text")), 
                    mItemStack.getName(), std::to_string(mItemStack.mCount)
                );

                form.appendButton(mItemName, [mItemStack, i](Player& pl) -> void {
                    MainGui::sellItem(pl, i);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) return MainGui::sell(pl);
            });
        }

        void sellItemContent(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
            for (std::string& item : getItems(player)) {
                std::unordered_map<std::string, std::string> mData = db->getByPrefix("Item", item + ".");

                form.appendButton(mData.at(item + ".NAME"), mData.at(item + ".ICON"), "path", [item](Player& pl) -> void {
                    MainGui::itemContent(pl, item);
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

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
                fmt::format(fmt::runtime(mObjectLabel), target,
                    db->get("Blacklist", mObject + "." + target + "_NAME", "None"),
                    SystemUtils::formatDataTime(db->get("Blacklist", mObject + "." + target + "_TIME", "None"), "None")
                )
            );
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.set.remove"), [target](Player& pl) -> void {
                delBlacklist(pl, target);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::blacklist(pl);
            });
        }

        void blacklistAdd(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
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
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.add"), "textures/ui/editIcon", "path", [mObjectLanguage](Player& pl) -> void {
                if (((int) getBlacklist(pl).size()) >= options.BlacklistUpload) {
                    pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips5")), options.BlacklistUpload));
                    return MainGui::sell(pl);
                }
                
                MainGui::blacklistAdd(pl);
            });
            for (std::string& mTarget : getBlacklist(player)) {
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
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent"), "textures/ui/creative_icon", "path", [mObjectLanguage](Player& pl) -> void {
                if (((int) getItems(pl).size()) >= options.MaximumUpload) {
                    pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips4")), options.MaximumUpload));
                    return;
                }
                
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

            ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
            for (std::string& item : getItems()) {
                std::unordered_map<std::string, std::string> mData = db->getByPrefix("Item", item + ".");

                std::vector<std::string> mList = getBlacklist(mData.at(item + ".PLAYER_UUID"));
                if (std::find(mList.begin(), mList.end(), mUuid) != mList.end())
                    continue;

                form.appendButton(mData.at(item + ".NAME"), mData.at(item + ".ICON"), "path", [item](Player& pl) -> void {
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
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                std::string mObject = event.self().getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                if (!db2->has("OBJECT$" + mObject, "Market_Score"))
                    db2->set("OBJECT$" + mObject, "Market_Score", "0");

                if (int mScore = SystemUtils::toInt(db2->get("OBJECT$" + mObject, "Market_Score"), 0); mScore > 0) {
                    ScoreboardUtils::addScore(event.self(), options.TargetScoreboard, mScore);

                    db2->set("OBJECT$" + mObject, "Market_Score", "0");
                }
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
        }
    }

    void addBlacklist(Player& player, Player& target) {
        if (!isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        db->set("Blacklist", mObject + "." + mTargetObject + "_NAME", target.getRealName());
        db->set("Blacklist", mObject + "." + mTargetObject + "_TIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log4"), player)), mTargetObject);
    }

    void delBlacklist(Player& player, const std::string& target) {
        if (!isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        db->delByPrefix("Blacklist", target + ".");

        logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log5"), player)), target);
    }

    void addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!isValid())
            return;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        db->set("Item", mTimestamp + ".NAME", name);
        db->set("Item", mTimestamp + ".ICON", icon);
        db->set("Item", mTimestamp + ".INTRODUCE", intr);
        db->set("Item", mTimestamp + ".SCORE", std::to_string(score));
        db->set("Item", mTimestamp + ".DATA", item.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0));
        db->set("Item", mTimestamp + ".PLAYER_NAME", player.getRealName());
        db->set("Item", mTimestamp + ".PLAYER_UUID", player.getUuid().asString());

        logger->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log2"), player)), name);
    }

    void delItem(const std::string& id) {
        if (!isValid())
            return;

        db->delByPrefix("Item", id + ".");
    }

    std::vector<std::string> getBlacklist(Player& player) {
        if (!isValid())
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return getBlacklist(mObject);
    }

    std::vector<std::string> getBlacklist(std::string target) {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;
        for (auto& mTarget : db->listByPrefix("Blacklist", target + "."))  {
            std::string mKey = mTarget.substr(mTarget.find_first_of('.') + 1);

            mResult.push_back(mKey.substr(0, mKey.find_last_of('_')));
        }

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    std::vector<std::string> getItems() {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = db->listByPrefix("Item", "%.");
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult](const std::string& mId) -> void {
            std::string mData = mId.substr(0, mId.find_first_of('.'));

            mResult.push_back(mData);
        });

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    std::vector<std::string> getItems(Player& player) {
        if (!isValid())
            return {};

        std::string mObject = player.getUuid().asString();

        std::vector<std::string> mResult;
        for (auto& mItem : getItems()) {
            if (db->get("Item", mItem + ".PLAYER_UUID") == mObject)
                mResult.push_back(mItem);
        }

        return mResult;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database, void* setting) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        db2 = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        options = Config::GetBaseConfigContext().Plugins.Market;

        db->create("Item");
        db->create("Blacklist");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}
