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

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct MarketPlugin::Impl {
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

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce), 
                mData.at("introduce"),
                mData.at("score"),
                mData.at("data"),
                mData.at("player_name")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.buy.button1"), [this, id, mObjectLanguage, mData](Player& pl) mutable -> void {
            int mScore = SystemUtils::toInt(mData.at("score"), 0);

            std::string mScoreboard = this->mParent.mImpl->options.TargetScoreboard;
            if (ScoreboardUtils::getScore(pl, mScoreboard) < mScore) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.sell.sellItem.tips3"));
                return this->buy(pl);
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
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [this, id, mObjectLanguage, mData](Player& pl) -> void {
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

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce),
                mData.at("introduce"),
                mData.at("score"),
                mData.at("data"),
                mData.at("player_name")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [this, id, mObjectLanguage, mData](Player& pl) -> void {
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

        std::vector<std::string> ProhibitedItems = this->mParent.mImpl->options.ProhibitedItems;

        std::vector<std::string> mItems;
        std::vector<int> mItemSlots;

        for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
            ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
            
            if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                continue;

            std::string mItemName = fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown.text")), 
                mItemStack.getName(), std::to_string(mItemStack.mCount)
            );

            mItems.push_back(mItemName);
            mItemSlots.push_back(i);
        }

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown"),
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
            this->sell(pl);
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
            this->sell(pl);
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
            this->sell(pl);
        });

        form->appendDivider();
        form->appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.add"), "textures/ui/editIcon", [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = this->mParent.mImpl->options.BlacklistUpload;
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips5")), mBlacklistCount));
                
                this->sell(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketPlugin::gui::sell(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItem"), "textures/ui/icon_blackfriday", "path", [this](Player& pl) -> void {
            this->sellItemInventory(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent"), "textures/ui/creative_icon", "path", [this, mObjectLanguage](Player& pl) -> void {
            int mItemCount = this->mParent.mImpl->options.MaximumUpload;
            if (static_cast<int>(this->mParent.getItems(pl).size()) >= mItemCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips4")), mItemCount));

                return;
            }
            
            this->sellItemContent(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist"), "textures/ui/icon_deals", "path", [this](Player& pl) -> void {
            this->blacklist(pl);
        });
        form.sendTo(player);
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

        form->sendPage(player, 1);
    }

    void MarketPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("market", tr({}, "commands.market.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->buy(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("sell").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->sell(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
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
