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

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/InventoryUtils.h"
#include "LOICollectionA/utils/ScoreboardUtils.h"
#include "LOICollectionA/utils/SystemUtils.h"
#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/MarketPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct MarketPlugin::Impl {
        C_Config::C_Plugins::C_Market options;

        std::unique_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
    };

    MarketPlugin::MarketPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    MarketPlugin::~MarketPlugin() = default;

    SQLiteStorage* MarketPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* MarketPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void MarketPlugin::gui::buyItem(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->getByPrefix("Item", id + ".");

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce), 
                mData.at(id + ".INTRODUCE"),
                mData.at(id + ".SCORE"),
                mData.at(id + ".DATA"),
                mData.at(id + ".PLAYER_NAME")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.buy.button1"), [this, id, mObjectLanguage, mData](Player& pl) mutable -> void {
            int mScore = SystemUtils::toInt(mData.at(id + ".SCORE"), 0);

            std::string mScoreboard = this->mParent.mImpl->options.TargetScoreboard;
            if (ScoreboardUtils::getScore(pl, mScoreboard) < mScore) {
                pl.sendMessage(tr(mObjectLanguage, "market.gui.sell.sellItem.tips3"));
                return this->buy(pl);
            }

            ScoreboardUtils::reduceScore(pl, mScoreboard, mScore);

            ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at(id + ".DATA"))->mTags);
            InventoryUtils::giveItem(pl, mItemStack, (int)mItemStack.mCount);

            pl.refreshInventory();

            std::string mObject = mData.at(id + ".PLAYER_UUID");
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
                mPlayer->sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips1")), mData.at(id + ".NAME")));

                ScoreboardUtils::addScore(*mPlayer, mScoreboard, mScore);
            } else {
                int mMarketScore = SystemUtils::toInt(this->mParent.mImpl->db2->get(mObject, "Market_Score"), 0);

                this->mParent.mImpl->db2->set(mObject, "Market_Score", std::to_string(mMarketScore + mScore));
            }

            this->mParent.delItem(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log2"), pl)), mData.at(id + ".NAME"));
        });
        if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) {
            form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [this, id, mObjectLanguage, mData](Player& pl) -> void {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips2")), mData.at(id + ".NAME")));
                
                this->mParent.delItem(id);

                this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log3"), pl)), mData.at(id + ".NAME"));
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->buy(pl);
        });
    }

    void MarketPlugin::gui::itemContent(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->getByPrefix("Item", id + ".");

        std::string mIntroduce = tr(mObjectLanguage, "market.gui.sell.introduce");

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mIntroduce),
                mData.at(id + ".INTRODUCE"),
                mData.at(id + ".SCORE"),
                mData.at(id + ".DATA"),
                mData.at(id + ".PLAYER_NAME")
            )
        );
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent.button1"), [this, id, mObjectLanguage, mData](Player& pl) -> void {
            pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips2")), mData.at(id + ".NAME")));
            
            ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at(id + ".DATA"))->mTags);
            InventoryUtils::giveItem(pl, mItemStack, (int)mItemStack.mCount);

            pl.refreshInventory();

            this->mParent.delItem(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log3"), pl)), mData.at(id + ".NAME"));
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
                return this->sellItemInventory(pl);
            }

            int mItemScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);

            ItemStack mItemStack = pl.mInventory->mInventory->getItem(mSlot);

            this->mParent.addItem(pl, mItemStack, mItemName, mItemIcon, mItemIntroduce, mItemScore);

            pl.mInventory->mInventory->removeItem(mSlot, 64);
            pl.refreshInventory();
        });
    }

    void MarketPlugin::gui::sellItemInventory(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> ProhibitedItems = this->mParent.mImpl->options.ProhibitedItems;

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown"));
        for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
            ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
            
            if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                continue;

            std::string mItemName = fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.dropdown.text")), 
                mItemStack.getName(), std::to_string(mItemStack.mCount)
            );

            form.appendButton(mItemName, [this, mItemStack, i](Player& pl) -> void {
                this->sellItem(pl, i);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->sell(pl);
        });
    }

    void MarketPlugin::gui::sellItemContent(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        for (std::string& item : this->mParent.getItems(player)) {
            std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->getByPrefix("Item", item + ".");

            form.appendButton(mData.at(item + ".NAME"), mData.at(item + ".ICON"), "path", [this, item](Player& pl) -> void {
                this->itemContent(pl, item);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->sell(pl);
        });
    }

    void MarketPlugin::gui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::string mObjectLabel = tr(mObjectLanguage, "market.gui.sell.blacklist.set.label");

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), 
            fmt::format(fmt::runtime(mObjectLabel), target,
                this->mParent.getDatabase()->get("Blacklist", mObject + "." + target + "_NAME", "None"),
                SystemUtils::formatDataTime(this->mParent.getDatabase()->get("Blacklist", mObject + "." + target + "_TIME", "None"), "None")
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
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.sell.blacklist.add.label"));
        ll::service::getLevel()->forEachPlayer([this, &form, &player](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->mParent.addBlacklist(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->blacklist(pl);
        });
    }

    void MarketPlugin::gui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.blacklist.add"), "textures/ui/editIcon", "path", [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = this->mParent.mImpl->options.BlacklistUpload;
            if (((int) this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips5")), mBlacklistCount));
                return this->sell(pl);
            }
            
            this->blacklistAdd(pl);
        });
        for (std::string& mTarget : this->mParent.getBlacklist(player)) {
            form.appendButton(mTarget, [this, mTarget](Player& pl) -> void {
                this->blacklistSet(pl, mTarget);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->sell(pl);
        });
    }

    void MarketPlugin::gui::sell(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItem"), "textures/ui/icon_blackfriday", "path", [this](Player& pl) -> void {
            this->sellItemInventory(pl);
        });
        form.appendButton(tr(mObjectLanguage, "market.gui.sell.sellItemContent"), "textures/ui/creative_icon", "path", [this, mObjectLanguage](Player& pl) -> void {
            int mItemCount = this->mParent.mImpl->options.MaximumUpload;
            if (((int) this->mParent.getItems(pl).size()) >= mItemCount) {
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
        std::replace(mUuid.begin(), mUuid.end(), '-', '_');

        ll::form::SimpleForm form(tr(mObjectLanguage, "market.gui.title"), tr(mObjectLanguage, "market.gui.label"));
        for (std::string& item : this->mParent.getItems()) {
            std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->getByPrefix("Item", item + ".");

            std::vector<std::string> mList = this->mParent.getBlacklist(mData.at(item + ".PLAYER_UUID"));
            if (std::find(mList.begin(), mList.end(), mUuid) != mList.end())
                continue;

            form.appendButton(mData.at(item + ".NAME"), mData.at(item + ".ICON"), "path", [this, item](Player& pl) -> void {
                this->buyItem(pl, item);
            });
        }
        form.sendTo(player);
    }

    void MarketPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("market", tr({}, "commands.market.description"), CommandPermissionLevel::Any);
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
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            if (!this->mImpl->db2->has("OBJECT$" + mObject, "Market_Score"))
                this->mImpl->db2->set("OBJECT$" + mObject, "Market_Score", "0");

            if (int mScore = SystemUtils::toInt(this->mImpl->db2->get("OBJECT$" + mObject, "Market_Score"), 0); mScore > 0) {
                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mScore);

                this->mImpl->db2->set("OBJECT$" + mObject, "Market_Score", "0");
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

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        this->getDatabase()->set("Blacklist", mObject + "." + mTargetObject + "_NAME", target.getRealName());
        this->getDatabase()->set("Blacklist", mObject + "." + mTargetObject + "_TIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log4"), player)), mTargetObject);
    }

    void MarketPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        this->getDatabase()->delByPrefix("Blacklist", target + ".");

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log5"), player)), target);
    }

    void MarketPlugin::addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!this->isValid())
            return;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        this->getDatabase()->set("Item", mTimestamp + ".NAME", name);
        this->getDatabase()->set("Item", mTimestamp + ".ICON", icon);
        this->getDatabase()->set("Item", mTimestamp + ".INTRODUCE", intr);
        this->getDatabase()->set("Item", mTimestamp + ".SCORE", std::to_string(score));
        this->getDatabase()->set("Item", mTimestamp + ".DATA", item.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0));
        this->getDatabase()->set("Item", mTimestamp + ".PLAYER_NAME", player.getRealName());
        this->getDatabase()->set("Item", mTimestamp + ".PLAYER_UUID", player.getUuid().asString());

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "market.log2"), player)), name);
    }

    void MarketPlugin::delItem(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->delByPrefix("Item", id + ".");
    }

    std::vector<std::string> MarketPlugin::getBlacklist(Player& player) {
        if (!this->isValid())
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return this->getBlacklist(mObject);
    }

    std::vector<std::string> MarketPlugin::getBlacklist(std::string target) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mResult;
        for (auto& mTarget : this->getDatabase()->listByPrefix("Blacklist", target + "."))  {
            std::string mKey = mTarget.substr(mTarget.find_first_of('.') + 1);

            mResult.push_back(mKey.substr(0, mKey.find_last_of('_')));
        }

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    std::vector<std::string> MarketPlugin::getItems() {
        if (!this->isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Item", "%.");
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult](const std::string& mId) -> void {
            std::string mData = mId.substr(0, mId.find_first_of('.'));

            mResult.push_back(mData);
        });

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    std::vector<std::string> MarketPlugin::getItems(Player& player) {
        if (!this->isValid())
            return {};

        std::string mObject = player.getUuid().asString();

        std::vector<std::string> mResult;
        for (auto& mItem : this->getItems()) {
            if (this->getDatabase()->get("Item", mItem + ".PLAYER_UUID") == mObject)
                mResult.push_back(mItem);
        }

        return mResult;
    }

    bool MarketPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool MarketPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Market.ModuleEnabled)
            return false;
        
        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "market.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Market;

        return true;
    }

    bool MarketPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->getDatabase()->create("Item");
        this->getDatabase()->create("Blacklist");
        
        this->registeryCommand();
        this->listenEvent();

        return true;
    }

    bool MarketPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        return true;
    }
}

REGISTRY_HELPER("MarketPlugin", LOICollection::Plugins::MarketPlugin, LOICollection::Plugins::MarketPlugin::getInstance())
