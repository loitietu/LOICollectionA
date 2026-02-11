#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/coro/InterruptableSleep.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/base/Containers.h>

#include <ll/api/form/ModalForm.h>
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

#include <mc/network/packet/TextPacket.h>

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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/InventoryUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/MarketPlugin.h"

template <typename T>
struct Allocator : public std::allocator<T> {
    using std::allocator<T>::allocator;

    using is_always_equal = typename std::allocator_traits<std::allocator<T>>::is_always_equal;
};

template <
    class Key,
    class Value,
    class Hash  = phmap::priv::hash_default_hash<Key>,
    class Eq    = phmap::priv::hash_default_eq<Key>,
    class Alloc = Allocator<::std::pair<Key const, Value>>,
    size_t N    = 4,
    class Mutex = std::shared_mutex>
using ConcurrentDenseMap = phmap::parallel_flat_hash_map<Key, Value, Hash, Eq, Alloc, N, Mutex>;

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct MarketPlugin::TradeEntry {
        std::string source;
        std::string target;

        TradeType type = TradeType::sell;
    };

    struct MarketPlugin::Impl {
        std::unordered_map<std::string, std::shared_ptr<ll::coro::InterruptableSleep>> mTimers;

        ConcurrentDenseMap<std::string, TradeEntry> mTradeMap;
        ConcurrentDenseMap<std::string, TradeEntry> mTradeRequestMap;

        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Market options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : BlacklistCache(100, 100) {}
    };

    MarketPlugin::MarketPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    MarketPlugin::~MarketPlugin() = default;

    MarketPlugin& MarketPlugin::getInstance() {
        static MarketPlugin instance;
        return instance;
    }

    std::shared_ptr<SQLiteStorage> MarketPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> MarketPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void MarketPlugin::gui::buyItem(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasItem(id)) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.error"));

            this->buy(player);
            return;
        }
        
        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Item", id);

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.item.introduce");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce), 
                mData.at("introduce"),
                mData.at("score"),
                mData.at("data"),
                mData.at("player_name")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.item.button1"), [this, id, mObjectLanguage, mData](Player& pl) mutable -> void {
            int mScore = SystemUtils::toInt(mData.at("score"), 0);

            std::string mScoreboard = this->mParent.mImpl->options.TargetScoreboard;
            if (ScoreboardUtils::getScore(pl, mScoreboard) < mScore) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.sell.sellItem.tips3"));

                this->buy(pl);
                return;
            }

            ScoreboardUtils::reduceScore(pl, mScoreboard, mScore);

            ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at("data"))->mTags);
            InventoryUtils::giveItem(pl, mItemStack, static_cast<int>(mItemStack.mCount));

            pl.refreshInventory();

            std::string mObject = mData.at("player_uuid");
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
                mPlayer->sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips1")), mData.at("name")));

                ScoreboardUtils::addScore(*mPlayer, mScoreboard, mScore);
            } else {
                int mMarketScore = SystemUtils::toInt(this->mParent.mImpl->db2->get("Market", mObject, "Score", "0"), 0);

                this->mParent.mImpl->db2->set("Market", mObject, "Score", std::to_string(mMarketScore + mScore));
            }

            this->mParent.delItem(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log2"), pl)), mData.at("name"));
        });
        if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) {
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.item.button2"), [this, id, mObjectLanguage, mData](Player& pl) -> void {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips2")), mData.at("name")));
                
                this->mParent.delItem(id);

                this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log3"), pl)), mData.at("name"));
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->buy(pl);
        });
    }

    void MarketPlugin::gui::itemContent(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasItem(id)) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.error"));

            this->sellItemContent(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Item", id);

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.item.introduce");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce),
                mData.at("introduce"),
                mData.at("score"),
                mData.at("data"),
                mData.at("player_name")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.item.button2"), [this, id, mObjectLanguage, mData](Player& pl) -> void {
            pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips2")), mData.at("name")));
            
            ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at("data"))->mTags);
            InventoryUtils::giveItem(pl, mItemStack, static_cast<int>(mItemStack.mCount));

            pl.refreshInventory();

            this->mParent.delItem(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log3"), pl)), mData.at("name"));
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->sellItemContent(pl);
        });
    }

    void MarketPlugin::gui::sellItem(Player& player, int mSlot) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "market.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "market.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "market.gui.sell.sellItem.input1"), tr(mObjectLanguage, "market.gui.sell.sellItem.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "market.gui.sell.sellItem.input2"), tr(mObjectLanguage, "market.gui.sell.sellItem.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "market.gui.sell.sellItem.input3"), tr(mObjectLanguage, "market.gui.sell.sellItem.input3.placeholder"));
        form.appendInput("Input4", tr(mObjectLanguage, "market.gui.sell.sellItem.input4"), tr(mObjectLanguage, "market.gui.sell.sellItem.input4.placeholder"));
        form.sendTo(player, [this, mSlot, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->sellItemInventory(pl);

            std::string mItemName = std::get<std::string>(dt->at("Input1"));
            std::string mItemIcon = std::get<std::string>(dt->at("Input2"));
            std::string mItemIntroduce = std::get<std::string>(dt->at("Input3"));

            if (mItemName.empty() || mItemIcon.empty() || mItemIntroduce.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                this->sellItemInventory(pl);
                return;
            }

            int mItemScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);

            ItemStack mItemStack = pl.mInventory->mInventory->getItem(mSlot);
            if (!mItemStack || mItemStack.isNull()) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));
                
                this->sellItemInventory(pl);
                return;
            }

            this->mParent.addItem(pl, mItemStack, mItemName, mItemIcon, mItemIntroduce, mItemScore);

            pl.mInventory->mInventory->removeItem(mSlot, 64);
            pl.refreshInventory();
        });
    }

    void MarketPlugin::gui::sellItemInventory(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mItems;
        std::vector<int> mItemSlots;

        std::vector<std::string> ProhibitedItems = this->mParent.mImpl->options.ProhibitedItems;
        for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
            ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
            
            if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                continue;

            std::string mItemName = fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.item.text")), 
                mItemStack.getName(), std::to_string(mItemStack.mCount)
            );

            mItems.push_back(mItemName);
            mItemSlots.push_back(i);
        }

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.sell.item.dropdown"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mItemSlots = std::move(mItemSlots)](Player& pl, int index) -> void {
            this->sellItem(pl, mItemSlots.at(index));
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->personal(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::sellItemContent(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::pair<std::string, std::string>> mItems;
        for (auto& item : this->mParent.getDatabase()->get("Item", this->mParent.getItems(player)))
            mItems.emplace_back(item.second.at("name"), item.second.at("icon"));

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.label"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->itemContent(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->personal(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(player, target)) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.error"));

            this->blacklist(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Blacklist", target);

        std::string mObjectLabel = tr(mObjectLanguage, "market.gui.sell.blacklist.set.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mObjectLabel),
                mData.at("target"),
                mData.at("name"),
                SystemUtils::toFormatTime(mData.at("time"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.set.remove"), [this, target](Player& pl) -> void {
            this->mParent.delBlacklist(pl, target);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->blacklist(pl);
        });
    }

    void MarketPlugin::gui::blacklistAdd(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::vector<std::string> mPlayers;
        std::vector<mce::UUID> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&player, &mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid());
            return true;
        });

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.sell.blacklist.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));

                this->blacklist(pl);
                return;
            }

            this->mParent.addBlacklist(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->blacklist(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.label"),
            this->mParent.getBlacklist(player)
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->blacklistSet(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->personal(pl);
        });

        form->appendDivider();
        form->appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.add"), "textures/ui/editIcon", [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = this->mParent.mImpl->options.BlacklistUpload;
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips5")), mBlacklistCount));
                
                this->personal(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::tradeConfirm(Player& player, Player& target, int mSlot, int score) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ItemStack mItemStack = player.mInventory->mInventory->getItem(mSlot);
        if (!mItemStack || mItemStack.isNull()) {
            this->mParent.cancelTrade(target);

            return;
        }

        ll::form::ModalForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.trade.confirm.introduce")),
                target.getRealName(),
                mItemStack.getName(),
                mItemStack.mCount,
                mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0),
                score
            ),
            tr(mObjectLanguage, "market.yes"),
            tr(mObjectLanguage, "market.no")
        );
        form.sendTo(player, [this, mObjectLanguage, mSlot, score](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                if (!this->mParent.acceptTrade(pl, mSlot, score))
                    pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));

                return;
            }
            
            this->mParent.cancelTrade(pl);
        });
    }

    void MarketPlugin::gui::tradeItem(Player& player, Player& target, int mSlot) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(target);

        ItemStack mItemStack = player.mInventory->mInventory->getItem(mSlot);
        if (!mItemStack || mItemStack.isNull()) {
            this->mParent.cancelTrade(player);

            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "market.gui.title"));
        form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.trade.introduce")), 
            player.getRealName(),
            mItemStack.getName(),
            mItemStack.mCount,
            mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0)
        ));
        form.appendInput("Input", tr(mObjectLanguage, "market.gui.trade.input"), tr(mObjectLanguage, "market.gui.trade.input.placeholder"));
        form.sendTo(target, [this, mSlot, player = player.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) {
                this->mParent.cancelTrade(pl);

                return;
            }

            Player* mPlayer = ll::service::getLevel()->getPlayer(player);
            if (!mPlayer) {
                this->mParent.cancelTrade(pl);

                return;
            }

            int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);

            this->tradeConfirm(*mPlayer, pl, mSlot, mScore);
        });
    }

    void MarketPlugin::gui::tradeContent(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mItems;
        std::vector<int> mItemSlots;

        std::vector<std::string> ProhibitedItems = this->mParent.mImpl->options.ProhibitedItems;
        for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
            ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
            
            if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                continue;

            std::string mItemName = fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.item.text")), 
                mItemStack.getName(), std::to_string(mItemStack.mCount)
            );

            mItems.push_back(mItemName);
            mItemSlots.push_back(i);
        }

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.sell.item.dropdown"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, target = target.getUuid(),  mItemSlots = std::move(mItemSlots)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(target);
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));

                this->mParent.cancelTrade(pl);
                return;
            }

            this->tradeItem(pl, *mPlayer, mItemSlots.at(index));
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->mParent.cancelTrade(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::tradeRequest(Player& player, Player& target, TradeType type) {
        if (this->mParent.mImpl->mTradeRequestMap.contains(player.getUuid().asString()) || this->mParent.mImpl->mTradeMap.contains(player.getUuid().asString())) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.tips3"));

            return;
        }

        this->mParent.sendRequest(player, target, type);

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(target);

        ll::form::ModalForm form(tr(mObjectLanguage, "market.gui.title"),
            LOICollectionAPI::APIUtils::getInstance().translate(tr(mObjectLanguage, (type == TradeType::buy) ? "market.buy" : "market.sell"), player),
            tr(mObjectLanguage, "market.yes"),
            tr(mObjectLanguage, "market.no")
        );
        form.sendTo(target, [this, mObjectLanguage](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                if (!this->mParent.acceptRequest(pl))
                    pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));

                return;
            }
            
            this->mParent.rejectRequest(pl);
        });
    }

    void MarketPlugin::gui::tradeType(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "market.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "market.gui.label"));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "market.gui.trade.dropdown"), { "sell", "buy" });
        form.sendTo(player, [this, mObjectLanguage, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);

            Player* mTarget = ll::service::getLevel()->getPlayer(target);
            if (!mTarget) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));

                this->open(pl);
                return;
            }

            this->tradeRequest(pl, *mTarget,
                std::get<std::string>(dt->at("dropdown")) == "sell" ? TradeType::sell : TradeType::buy
            );
        });
    }

    void MarketPlugin::gui::trade(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayers;
        std::vector<mce::UUID> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&player, &mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid());
            return true;
        });

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.trade.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));

                this->open(pl);
                return;
            }

            this->tradeType(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::personal(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        form.appendButton(tr(mObjectLanguage, "market.gui.personal.sellItem"), "textures/ui/icon_blackfriday", "path", [this](Player& pl) -> void {
            this->sellItemInventory(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.personal.sellItemContent"), "textures/ui/creative_icon", "path", [this, mObjectLanguage](Player& pl) -> void {
            int mItemCount = this->mParent.mImpl->options.MaximumUpload;
            if (static_cast<int>(this->mParent.getItems(pl).size()) >= mItemCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips4")), mItemCount));

                return;
            }
            
            this->sellItemContent(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.personal.blacklist"), "textures/ui/icon_deals", "path", [this](Player& pl) -> void {
            this->blacklist(pl);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void MarketPlugin::gui::buy(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mUuid = player.getUuid().asString();
        
        std::vector<std::pair<std::string, std::string>> mItems;
        for (auto& item : this->mParent.getDatabase()->get("Item", this->mParent.getItems())) {
            std::vector<std::string> mList = this->mParent.getBlacklist(item.second.at("player_uuid"));
            if (std::find(mList.begin(), mList.end(), mUuid) != mList.end())
                continue;

            mItems.emplace_back(item.second.at("name"), item.second.at("icon"));
        }

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.label"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->buyItem(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        form.appendButton(tr(mObjectLanguage, "market.gui.worldbuy"), "textures/ui/world_glyph_color", "path", [this](Player& pl) -> void {
            this->buy(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.trade"), "textures/ui/trade_icon", "path", [this](Player& pl) -> void {
            this->trade(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.personal"), "textures/ui/icon_best3", "path", [this](Player& pl) -> void {
            this->personal(pl);
        });
        form.sendTo(player);
    }

    void MarketPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("market", tr({}, "commands.market.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void MarketPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            
            if (!this->mImpl->db2->has("Market", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "score", "0" }
                };

                this->mImpl->db2->set("Market", mObject, mData);
            }

            if (int mScore = SystemUtils::toInt(this->mImpl->db2->get("Market", mObject, "score"), 0); mScore > 0) {
                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mScore);

                this->mImpl->db2->set("Market", mObject, "score", "0");
            }
        });
    }

    void MarketPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);

        for (auto& it : this->mImpl->mTimers)
            it.second->interrupt();

        this->mImpl->mTimers.clear();
    }

    void MarketPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();
        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        std::unordered_map<std::string, std::string> mData = {
            { "name", target.getRealName() },
            { "target", mTargetObject },
            { "author", mObject },
            { "time", mTimestamp }
        };

        this->getDatabase()->set("Blacklist", mTimestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log4"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTargetObject);
            });
    }

    void MarketPlugin::addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!this->isValid())
            return;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", name },
            { "icon", icon },
            { "introduce", intr },
            { "score", std::to_string(score) },
            { "data", item.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0) },
            { "player_name", player.getRealName() },
            { "player_uuid", player.getUuid().asString() }
        };

        this->getDatabase()->set("Item", mTimestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log2"), player)), name);
    }

    void MarketPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid())
            return;

        if (!this->hasBlacklist(player, target)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "MarketPlugin");

            return;
        }

        std::string mId = this->getDatabase()->find("Blacklist", {
            { "target", target },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Blacklist", mId);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log5"), player)), target);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [target](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), target), mList->end());
        });
    }

    void MarketPlugin::delItem(const std::string& id) {
        if (!this->isValid())
            return;

        if (!this->hasItem(id)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "MarketPlugin");

            return;
        }

        this->getDatabase()->del("Item", id);
    }

    bool MarketPlugin::acceptRequest(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequestMap.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeRequestMap.at(mObject);

        Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));
        if (!sourcePlayer) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.gui.error"));

            return false;
        }

        sourcePlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "market.yes.tips"));

        if (mEntry.type == TradeType::sell) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.tips4"));

            this->mGui->tradeContent(*sourcePlayer, player);
        } else {
            sourcePlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "market.tips4"));

            this->mGui->tradeContent(player, *sourcePlayer);
        }

        this->sendTrade(*sourcePlayer, player, mEntry.type);

        this->mImpl->mTradeRequestMap.erase(mObject);
        this->mImpl->mTradeRequestMap.erase(mEntry.source);

        this->mImpl->mTimers[mEntry.source]->interrupt();
        this->mImpl->mTimers.erase(mEntry.source);

        return true;
    }

    bool MarketPlugin::rejectRequest(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequestMap.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeRequestMap.at(mObject);

        if (Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source)); sourcePlayer)
            sourcePlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "market.no.tips"));
        
        this->mImpl->mTradeRequestMap.erase(mObject);
        this->mImpl->mTradeRequestMap.erase(mEntry.source);

        this->mImpl->mTimers[mEntry.source]->interrupt();
        this->mImpl->mTimers.erase(mEntry.source);

        return true;
    }

    bool MarketPlugin::cancelRequest(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequestMap.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeRequestMap.at(mObject);
        
        this->mImpl->mTradeRequestMap.erase(mObject);
        this->mImpl->mTradeRequestMap.erase(mEntry.target);

        this->mImpl->mTimers[mEntry.source]->interrupt();
        this->mImpl->mTimers.erase(mEntry.source);

        return true;
    }

    bool MarketPlugin::acceptTrade(Player& player, int slot, int score) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeMap.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeMap.at(mObject);

        ItemStack mItemStack = player.getInventory().getItem(slot);
        if (!mItemStack || mItemStack.isNull())
            return false;

        Player* mPlayer = (mEntry.source == mObject) ?
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.target)) :
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));

        if (!mPlayer)
            return false;

        if (ScoreboardUtils::getScore(*mPlayer, this->mImpl->options.TargetScoreboard) < score || score <= 0)
            return false;

        player.mInventory->mInventory->removeItem(slot, 64);
        mPlayer->getInventory().addItem(mItemStack);

        player.refreshInventory();
        mPlayer->refreshInventory();

        ScoreboardUtils::addScore(player, this->mImpl->options.TargetScoreboard, score);
        ScoreboardUtils::reduceScore(*mPlayer, this->mImpl->options.TargetScoreboard, score);

        this->mImpl->mTradeMap.erase(mObject);
        this->mImpl->mTradeMap.erase(mEntry.target);

        this->mImpl->mTimers[mEntry.source + "_trade"]->interrupt();
        this->mImpl->mTimers.erase(mEntry.source + "_trade");

        return true;
    }

    bool MarketPlugin::cancelTrade(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeMap.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeMap.at(mObject);

        Player* mPlayer = (mEntry.source == mObject) ?
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.target)) :
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));

        if (mPlayer)
            mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips6"));

        this->mImpl->mTradeMap.erase(mEntry.source);
        this->mImpl->mTradeMap.erase(mEntry.target);

        this->mImpl->mTimers[mEntry.source + "_trade"]->interrupt();
        this->mImpl->mTimers.erase(mEntry.source + "_trade");

        this->getLogger()->info(fmt::runtime(tr({}, "market.log8")), player.getRealName());

        return true;
    }

    void MarketPlugin::sendRequest(Player& player, Player& target, TradeType type) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        TradeEntry mEntry{ mObject, mTargetObject, type };
        this->mImpl->mTradeRequestMap[mObject] = mEntry;
        this->mImpl->mTradeRequestMap[mTargetObject] = mEntry;

        ll::coro::keepThis([this, mObject, mTargetObject]() -> ll::coro::CoroTask<> {
            this->mImpl->mTimers[mObject] = std::make_shared<ll::coro::InterruptableSleep>();

            co_await this->mImpl->mTimers[mObject]->sleepFor(std::chrono::seconds(this->mImpl->options.TradeRequestTimeout));

            if (!this->mImpl->mTradeRequestMap.contains(mObject) || !this->mImpl->mTradeRequestMap.contains(mTargetObject))
                co_return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips2"));
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips2"));

            this->mImpl->mTradeRequestMap.erase(mObject);
            this->mImpl->mTradeRequestMap.erase(mTargetObject);

            this->mImpl->mTimers.erase(mObject);
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.tips1"));

        this->getLogger()->info(fmt::runtime(tr({}, "market.log6")), player.getRealName(), target.getRealName());
    }

    void MarketPlugin::sendTrade(Player& player, Player& target, TradeType type) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        TradeEntry mEntry{ mObject, mTargetObject, type };
        this->mImpl->mTradeMap[mObject] = mEntry;
        this->mImpl->mTradeMap[mTargetObject] = mEntry;

        ll::coro::keepThis([this, mObject, mTargetObject]() -> ll::coro::CoroTask<> {
            this->mImpl->mTimers[mObject + "_trade"] = std::make_shared<ll::coro::InterruptableSleep>();

            co_await this->mImpl->mTimers[mObject + "_trade"]->sleepFor(std::chrono::seconds(this->mImpl->options.TradeTimeout));

            if (!this->mImpl->mTradeMap.contains(mObject) || !this->mImpl->mTradeMap.contains(mTargetObject))
                co_return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips5"));
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips5"));

            this->mImpl->mTradeMap.erase(mObject);
            this->mImpl->mTradeMap.erase(mTargetObject);

            this->mImpl->mTimers.erase(mObject + "_trade");
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        this->getLogger()->info(fmt::runtime(tr({}, "market.log7")), player.getRealName(), target.getRealName());
    }

    std::vector<std::string> MarketPlugin::getBlacklist(Player& player) {
        if (!this->isValid())
            return {};

        return this->getBlacklist(player.getUuid().asString());
    }

    std::vector<std::string> MarketPlugin::getBlacklist(const std::string& target) {
        if (!this->isValid())
            return {};

        if (this->mImpl->BlacklistCache.contains(target))
            return *this->mImpl->BlacklistCache.get(target).value();

        std::vector<std::string> mResult = this->getDatabase()->find("Blacklist", {
            { "target", target }
        }, SQLiteStorage::FindCondition::AND);

        this->mImpl->BlacklistCache.put(target, mResult);
        return mResult;
    }

    std::vector<std::string> MarketPlugin::getItems() {
        if (!this->isValid())
            return {};

        return this->getDatabase()->list("Item");
    }

    std::vector<std::string> MarketPlugin::getItems(Player& player) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Item", {
            { "player_uuid", player.getUuid().asString() }
        }, SQLiteStorage::FindCondition::AND);
    }

    bool MarketPlugin::hasItem(const std::string& id) {
        if (!this->isValid())
            return false;

        return !this->getDatabase()->find("Item", {
            { "name", id }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool MarketPlugin::hasBlacklist(Player& player, const std::string& uuid) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mList = this->mImpl->BlacklistCache.get(mObject).value();

            return std::find(mList->begin(), mList->end(), uuid) != mList->end();
        }

        return !this->getDatabase()->find("Blacklist", {
            { "target", uuid },
            { "author", mObject }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool MarketPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool MarketPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Market.ModuleEnabled)
            return false;
        
        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "market.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Market;

        return true;
    }

    bool MarketPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->db2.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool MarketPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db2->create("Market", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("score");
        });

        this->getDatabase()->create("Item", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("icon");
            ctor("introduce");
            ctor("score");
            ctor("data");
            ctor("player_name");
            ctor("player_uuid");
        });
        this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("target");
            ctor("author");
            ctor("time");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool MarketPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("MarketPlugin", LOICollection::Plugins::MarketPlugin, LOICollection::Plugins::MarketPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
