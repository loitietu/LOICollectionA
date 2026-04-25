#include <string>
#include <unordered_map>

#include <ll/api/io/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/MarketPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/MarketGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void MarketGui::buyItem(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasItem(id)) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.error"));

            this->buy(player);
            return;
        }
        
        std::unordered_map<std::string, std::string> mData = this->mParent.getItemData(id);

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.item.introduce");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce), 
                mData.at("introduce"),
                mData.at("score"),
                mData.at("data"),
                mData.at("player_name")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.item.button1"), [this, id, mObjectLanguage](Player& pl) mutable -> void {
            if (!this->mParent.buyItem(pl, id))
                this->buy(pl);
        });
        if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) {
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.item.button2"), [this, id, mObjectLanguage](Player& pl) -> void {
                this->mParent.offshelfItem(pl, id);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->buy(pl);
        });
    }

    void MarketGui::itemContent(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasItem(id)) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.error"));

            this->sellItemContent(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getItemData(id);

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.item.introduce");
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce),
                mData.at("introduce"),
                mData.at("score"),
                mData.at("data"),
                mData.at("player_name")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.item.button2"), [this, id, mObjectLanguage](Player& pl) -> void {
            this->mParent.offshelfItem(pl, id, true);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->sellItemContent(pl);
        });
    }

    void MarketGui::sellItem(Player& player, int mSlot) {
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
            if (!this->mParent.sellItem(pl, mSlot, mItemName, mItemIcon, mItemIntroduce, mItemScore)) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.error"));
                
                this->sellItemInventory(pl);
            }
        });
    }

    void MarketGui::sellItemInventory(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mItems;
        std::vector<int> mItemSlots;

        std::vector<std::string> ProhibitedItems = this->mParent.getProhibitedItems();
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

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void MarketGui::sellItemContent(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::pair<std::string, std::string>> mItems;
        std::vector<std::string> mItemIds;

        for (auto& item : this->mParent.getItemsData(this->mParent.getItems(player))) {
            mItems.emplace_back(item.second.at("name"), item.second.at("icon"));
            mItemIds.push_back(item.first);
        }

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.label"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
            this->itemContent(pl, mItemIds.at(index));
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->personal(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketGui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(player, target)) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.error"));

            this->blacklist(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getBlacklistData(target);

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

    void MarketGui::blacklistAdd(Player& player) {
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

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void MarketGui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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
            int mBlacklistCount = this->mParent.getBlacklistUpload();
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips5")), mBlacklistCount));
                
                this->personal(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketGui::tradeConfirm(Player& player, Player& target, int mSlot, int score) {
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

    void MarketGui::tradeItem(Player& player, Player& target, int mSlot) {
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

    void MarketGui::tradeContent(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mItems;
        std::vector<int> mItemSlots;

        std::vector<std::string> ProhibitedItems = this->mParent.getProhibitedItems();
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

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void MarketGui::tradeRequest(Player& player, Player& target, MarketTradeType type) {
        if (this->mParent.hasTrade(player)) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.tips3"));

            return;
        }

        this->mParent.sendRequest(player, target, type);

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(target);

        ll::form::ModalForm form(tr(mObjectLanguage, "market.gui.title"),
            LOICollectionAPI::APIUtils::getInstance().translate(tr(mObjectLanguage, (type == MarketTradeType::buy) ? "market.buy" : "market.sell"), player),
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

    void MarketGui::tradeType(Player& player, Player& target) {
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
                std::get<std::string>(dt->at("dropdown")) == "sell" ? MarketTradeType::sell : MarketTradeType::buy
            );
        });
    }

    void MarketGui::trade(Player& player) {
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

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void MarketGui::personal(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        form.appendButton(tr(mObjectLanguage, "market.gui.personal.sellItem"), "textures/ui/icon_blackfriday", "path", [this](Player& pl) -> void {
            this->sellItemInventory(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.personal.sellItemContent"), "textures/ui/creative_icon", "path", [this, mObjectLanguage](Player& pl) -> void {
            int mItemCount = this->mParent.getMaximumUpload();
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

    void MarketGui::buy(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mUuid = player.getUuid().asString();
        
        std::vector<std::pair<std::string, std::string>> mItems;
        std::vector<std::string> mItemIds;

        for (auto& item : this->mParent.getItemsData(this->mParent.getItems())) {
            std::vector<std::string> mList = this->mParent.getBlacklist(item.second.at("player_uuid"));
            if (std::find(mList.begin(), mList.end(), mUuid) != mList.end())
                continue;

            mItems.emplace_back(item.second.at("name"), item.second.at("icon"));
            mItemIds.push_back(item.first);
        }

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "market.gui.title"),
            tr(mObjectLanguage, "market.gui.label"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
            this->buyItem(pl, mItemIds.at(index));
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void MarketGui::open(Player& player) {
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
}
