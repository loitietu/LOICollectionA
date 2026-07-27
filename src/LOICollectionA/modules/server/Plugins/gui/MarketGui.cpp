#include <string>
#include <unordered_map>

#include <ll/api/Expected.h>
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
    ll::Expected<void> MarketGui::buyItem(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.hasItem(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "market.gui.error"));

                            return this->sellItemContent(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .and_then([this, id](bool exists) -> ll::Expected<std::unordered_map<std::string, std::string>> {
                        if (!exists)
                            return {};

                        return this->mParent.getItemData(id);
                    })
                    .transform([this, language, id, &player](std::unordered_map<std::string, std::string> data) -> void {
                        if (data.empty())
                            return;

                        ll::form::SimpleForm form(tr(language, "market.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "market.gui.item.introduce")), 
                                data.at("introduce"),
                                data.at("score"),
                                data.at("data"),
                                data.at("player_name")
                            )
                        );
                        form.appendButton(tr(language, "market.gui.sell.item.button1"), [this, id, language](Player& pl) mutable -> void {
                            this->mParent.buyItem(pl, id)
                                .or_else([](ll::Error e) -> ll::Expected<bool> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::ItemNotFound))
                                        return false;

                                    return ll::Unexpected(e);
                                })
                                .and_then([this, &pl](bool exists) -> ll::Expected<void> {
                                    if (!exists)
                                        return this->buy(pl);

                                    return {};
                                })
                                .or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                        if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors) {
                            form.appendButton(tr(language, "market.gui.sell.item.button2"), [this, id, language](Player& pl) -> void {
                                this->mParent.offshelfItem(pl, id)
                                    .or_else([](ll::Error e) -> ll::Expected<bool> {
                                        if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::ItemNotFound))
                                            return false;

                                        return ll::Unexpected(e);
                                    })
                                    .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);
                            });
                        }
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->buy(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                    });
            });
    }

    ll::Expected<void> MarketGui::itemContent(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.hasItem(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "market.gui.error"));

                            return this->sellItemContent(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .and_then([this, id](bool exists) -> ll::Expected<std::unordered_map<std::string, std::string>> {
                        if (!exists)
                            return {};

                        return this->mParent.getItemData(id);
                    })
                    .transform([this, language, id, &player](std::unordered_map<std::string, std::string> data) -> void {
                        if (data.empty())
                            return;

                        ll::form::SimpleForm form(tr(language, "market.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "market.gui.item.introduce")),
                                data.at("introduce"),
                                data.at("score"),
                                data.at("data"),
                                data.at("player_name")
                            )
                        );
                        form.appendButton(tr(language, "market.gui.sell.item.button2"), [this, id, language](Player& pl) -> void {
                            this->mParent.offshelfItem(pl, id, true)
                                .or_else([](ll::Error e) -> ll::Expected<bool> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::ItemNotFound))
                                        return false;

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->sellItemContent(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                    });
            });
    }

    ll::Expected<void> MarketGui::sellItem(Player& player, int mSlot) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, mSlot, &player](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "market.gui.title"));
                form.appendLabel(tr(language, "market.gui.label"));
                form.appendInput("Input1", tr(language, "market.gui.sell.sellItem.input1"), tr(language, "market.gui.sell.sellItem.input1.placeholder"));
                form.appendInput("Input2", tr(language, "market.gui.sell.sellItem.input2"), tr(language, "market.gui.sell.sellItem.input2.placeholder"));
                form.appendInput("Input3", tr(language, "market.gui.sell.sellItem.input3"), tr(language, "market.gui.sell.sellItem.input3.placeholder"));
                form.appendInput("Input4", tr(language, "market.gui.sell.sellItem.input4"), tr(language, "market.gui.sell.sellItem.input4.placeholder"));
                form.sendTo(player, [this, mSlot, language](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->sellItemInventory(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);

                        return;
                    }

                    std::string mItemName = std::get<std::string>(dt->at("Input1"));
                    std::string mItemIcon = std::get<std::string>(dt->at("Input2"));
                    std::string mItemIntroduce = std::get<std::string>(dt->at("Input3"));

                    if (mItemName.empty() || mItemIcon.empty() || mItemIntroduce.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));

                        this->sellItemInventory(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);

                        return;
                    }

                    int mItemScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);
                    if (!this->mParent.sellItem(pl, mSlot, mItemName, mItemIcon, mItemIntroduce, mItemScore)) {
                        pl.sendMessage(tr(language, "market.gui.error"));
                        
                        this->sellItemInventory(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                    }
                });
            });
    }

    ll::Expected<void> MarketGui::sellItemInventory(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                std::vector<std::string> mItems;
                std::vector<int> mItemSlots;

                std::vector<std::string> ProhibitedItems = this->mParent.getProhibitedItems();
                for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
                    ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
                    
                    if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                        continue;

                    std::string mItemName = fmt::format(fmt::runtime(tr(language, "market.gui.sell.item.text")), 
                        mItemStack.getName(), std::to_string(mItemStack.mCount)
                    );

                    mItems.push_back(mItemName);
                    mItemSlots.push_back(i);
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "market.gui.title"),
                    tr(language, "market.gui.sell.item.dropdown"),
                    mItems
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mItemSlots = std::move(mItemSlots)](Player& pl, int index) -> void {
                    this->sellItem(pl, mItemSlots.at(index)).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->personal(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> MarketGui::sellItemContent(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getItems()
                    .and_then([this](const std::vector<std::string>& ids) -> ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> {
                        return this->mParent.getItemsData(ids);
                    })
                    .and_then([this, language, &player](std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data) -> ll::Expected<void> {
                        std::vector<std::pair<std::string, std::string>> mItems;
                        std::vector<std::string> mItemIds;

                        for (auto& item : data) {
                            mItems.emplace_back(item.second.at("name"), item.second.at("icon"));
                            mItemIds.push_back(item.first);
                        }

                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "market.gui.title"),
                            tr(language, "market.gui.label"),
                            mItems
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
                            this->itemContent(pl, mItemIds.at(index)).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->personal(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });

                        form->sendPage(player, 1);

                        return {};
                    });
            });
    }

    ll::Expected<void> MarketGui::blacklistSet(Player& player, const std::string& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, target, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.hasBlacklist(player, target)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "market.gui.error"));

                            return this->blacklist(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .and_then([this, target](bool exists) -> ll::Expected<std::unordered_map<std::string, std::string>> {
                        if (!exists)
                            return {};

                        return this->mParent.getBlacklistData(target);
                    })
                    .transform([this, language, target, &player](std::unordered_map<std::string, std::string> data) -> void {
                        if (data.empty())
                            return;

                        ll::form::SimpleForm form(tr(language, "market.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "market.gui.sell.blacklist.set.label")),
                                data.at("target"),
                                data.at("name"),
                                SystemUtils::toFormatTime(data.at("time"), "None")
                            )
                        );
                        form.appendButton(tr(language, "market.gui.sell.blacklist.set.remove"), [this, target](Player& pl) -> void {
                            this->mParent.delBlacklist(pl, target)
                                .or_else([](ll::Error e) -> ll::Expected<void> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::BlacklistNotFound))
                                        return {};

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<MarketPlugin, void>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->blacklist(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                    });
            });
    }

    ll::Expected<void> MarketGui::blacklistAdd(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
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
                    tr(language, "market.gui.title"),
                    tr(language, "market.gui.sell.blacklist.add.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "market.gui.error"));

                        this->blacklist(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);

                        return;
                    }

                    this->mParent.addBlacklist(pl, *mPlayer).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->blacklist(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> MarketGui::blacklist(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getBlacklist(player)
                    .transform([this, language, &player](const std::vector<std::string>& blacklists) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "market.gui.title"),
                            tr(language, "market.gui.label"),
                            blacklists
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this](Player& pl, const std::string& response) -> void {
                            this->blacklistSet(pl, response).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->personal(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });

                        form->appendDivider();
                        form->appendButton(tr(language, "market.gui.sell.blacklist.add"), "textures/ui/editIcon", [this, language](Player& pl) -> void {
                            int mBlacklistCount = this->mParent.getBlacklistUpload();

                            this->mParent.getBlacklist(pl)
                                .and_then([this, language, mBlacklistCount, &pl](const std::vector<std::string>& ids) -> ll::Expected<void> {
                                    if (static_cast<int>(ids.size()) >= mBlacklistCount) {
                                        pl.sendMessage(fmt::format(fmt::runtime(tr(language, "market.gui.sell.sellItem.tips5")), mBlacklistCount));
                                        
                                        return this->personal(pl);
                                    }

                                    return this->blacklistAdd(pl);
                                })
                                .or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> MarketGui::tradeConfirm(Player& player, Player& target, int mSlot, int score) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, mSlot, score, &player, &target](const std::string& language) -> ll::Expected<void> {
                ItemStack mItemStack = player.mInventory->mInventory->getItem(mSlot);
                if (!mItemStack || mItemStack.isNull()) {
                    return this->mParent.cancelTrade(target)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([](bool) -> void {});
                }

                ll::form::ModalForm form(tr(language, "market.gui.title"), 
                    fmt::format(fmt::runtime(tr(language, "market.gui.trade.confirm.introduce")),
                        target.getRealName(),
                        mItemStack.getName(),
                        mItemStack.mCount,
                        mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0),
                        score
                    ),
                    tr(language, "market.yes"),
                    tr(language, "market.no")
                );
                form.sendTo(player, [this, language, mSlot, score](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                    if (result == ll::form::ModalFormSelectedButton::Upper) {
                        this->mParent.acceptTrade(pl, mSlot, score)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .transform([language, &pl](bool exists) -> void {
                                if (!exists)
                                    pl.sendMessage(tr(language, "market.gui.error"));
                            })
                            .or_else(modules::defaultErrorHandler<MarketPlugin>);

                        return;
                    }
                    
                    this->mParent.cancelTrade(pl)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);
                });

                return {};
            });
    }

    ll::Expected<void> MarketGui::tradeItem(Player& player, Player& target, int mSlot) {
        return LanguagePlugin::getShared()->getLanguage(target)
            .and_then([this, mSlot, &player, &target](const std::string& language) -> ll::Expected<void> {
                ItemStack mItemStack = player.mInventory->mInventory->getItem(mSlot);
                if (!mItemStack || mItemStack.isNull()) {
                    return this->mParent.cancelTrade(player)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .transform([](bool) -> void {});
                }

                ll::form::CustomForm form(tr(language, "market.gui.title"));
                form.appendLabel(fmt::format(fmt::runtime(tr(language, "market.gui.trade.introduce")), 
                    player.getRealName(),
                    mItemStack.getName(),
                    mItemStack.mCount,
                    mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0)
                ));
                form.appendInput("Input", tr(language, "market.gui.trade.input"), tr(language, "market.gui.trade.input.placeholder"));
                form.sendTo(target, [this, mSlot, player = player.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->mParent.cancelTrade(pl)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);

                        return;
                    }

                    Player* mPlayer = ll::service::getLevel()->getPlayer(player);
                    if (!mPlayer) {
                        this->mParent.cancelTrade(pl)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);

                        return;
                    }

                    int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);

                    this->tradeConfirm(*mPlayer, pl, mSlot, mScore).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });

                return {};
            });
    }

    ll::Expected<void> MarketGui::tradeContent(Player& player, Player& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, &target](const std::string& language) -> void {
                std::vector<std::string> mItems;
                std::vector<int> mItemSlots;

                std::vector<std::string> ProhibitedItems = this->mParent.getProhibitedItems();
                for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
                    ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
                    
                    if (!mItemStack || mItemStack.isNull() || std::find(ProhibitedItems.begin(), ProhibitedItems.end(), mItemStack.getTypeName()) != ProhibitedItems.end())
                        continue;

                    std::string mItemName = fmt::format(fmt::runtime(tr(language, "market.gui.sell.item.text")), 
                        mItemStack.getName(), std::to_string(mItemStack.mCount)
                    );

                    mItems.push_back(mItemName);
                    mItemSlots.push_back(i);
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "market.gui.title"),
                    tr(language, "market.gui.sell.item.dropdown"),
                    mItems
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, target = target.getUuid(),  mItemSlots = std::move(mItemSlots)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(target);
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "market.gui.error"));

                        this->mParent.cancelTrade(pl)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);

                        return;
                    }

                    this->tradeItem(pl, *mPlayer, mItemSlots.at(index)).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->mParent.cancelTrade(pl)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::TradeNotFound))
                                return false;

                            return ll::Unexpected(e);
                        })
                        .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> MarketGui::tradeRequest(Player& player, Player& target, MarketTradeType type) {
        return this->mParent.hasTrade(player)
            .and_then([this, type, &player, &target](bool exists) -> ll::Expected<void> {
                if (exists) {
                    return LanguagePlugin::getShared()->getLanguage(player)
                        .transform([&player](const std::string& language) -> void {
                            player.sendMessage(tr(language, "market.tips3"));
                        });
                }

                return this->mParent.sendRequest(player, target, type)
                    .and_then([&target]() -> ll::Expected<std::string> {
                        return LanguagePlugin::getShared()->getLanguage(target);
                    })
                    .transform([this, type, &player, &target](const std::string& language) -> void {
                        ll::form::ModalForm form(tr(language, "market.gui.title"),
                            LOICollectionAPI::APIUtils::getInstance().translate(tr(language, (type == MarketTradeType::buy) ? "market.buy" : "market.sell"), player),
                            tr(language, "market.yes"),
                            tr(language, "market.no")
                        );
                        form.sendTo(target, [this, language](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                            if (result == ll::form::ModalFormSelectedButton::Upper) {
                                this->mParent.acceptRequest(pl)
                                    .or_else([](ll::Error e) -> ll::Expected<bool> {
                                        if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::RequestNotFound))
                                            return false;

                                        return ll::Unexpected(e);
                                    })
                                    .transform([language, &pl](bool exists) -> void {
                                        if (!exists)
                                            pl.sendMessage(tr(language, "market.gui.error"));
                                    })
                                    .or_else(modules::defaultErrorHandler<MarketPlugin>);

                                return;
                            }
                            
                            this->mParent.rejectRequest(pl)
                                .or_else([](ll::Error e) -> ll::Expected<bool> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == MarketPlugin::makeErrorCode(MarketPluginErrorCode::RequestNotFound))
                                        return false;

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<MarketPlugin, bool>);
                        });
                    });
            });
    }

    ll::Expected<void> MarketGui::tradeType(Player& player, Player& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, &target](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "market.gui.title"));
                form.appendLabel(tr(language, "market.gui.label"));
                form.appendDropdown("dropdown", tr(language, "market.gui.trade.dropdown"), { "sell", "buy" });
                form.sendTo(player, [this, language, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->open(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);

                        return;
                    }

                    Player* mTarget = ll::service::getLevel()->getPlayer(target);
                    if (!mTarget) {
                        pl.sendMessage(tr(language, "market.gui.error"));

                        this->open(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);

                        return;
                    }

                    this->tradeRequest(pl, *mTarget,
                        std::get<std::string>(dt->at("dropdown")) == "sell" ? MarketTradeType::sell : MarketTradeType::buy
                    ).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
            });
    }

    ll::Expected<void> MarketGui::trade(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
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
                    tr(language, "market.gui.title"),
                    tr(language, "market.gui.trade.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "market.gui.error"));

                        this->open(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        return;
                    }

                    this->tradeType(pl, *mPlayer).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->open(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> MarketGui::personal(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "market.gui.title"), tr(language, "market.gui.label"));
                form.appendButton(tr(language, "market.gui.personal.sellItem"), "textures/ui/icon_blackfriday", "path", [this](Player& pl) -> void {
                    this->sellItemInventory(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form.appendButton(tr(language, "market.gui.personal.sellItemContent"), "textures/ui/creative_icon", "path", [this, language](Player& pl) -> void {
                    int mItemCount = this->mParent.getMaximumUpload();

                    this->mParent.getItems(pl)
                        .and_then([this, mItemCount, language, &pl](const std::vector<std::string>& items) -> ll::Expected<void> {
                            if (static_cast<int>(items.size()) >= mItemCount) {
                                pl.sendMessage(fmt::format(fmt::runtime(tr(language, "market.gui.sell.sellItem.tips4")), mItemCount));

                                return {};
                            }
                            
                            return this->sellItemContent(pl);
                        })
                        .or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form.appendButton(tr(language, "market.gui.personal.blacklist"), "textures/ui/icon_deals", "path", [this](Player& pl) -> void {
                    this->blacklist(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                    if (id == -1)
                        this->open(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
            });
    }

    ll::Expected<void> MarketGui::buy(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getItems()
                    .and_then([this](const std::vector<std::string>& ids) -> ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> {
                        return this->mParent.getItemsData(ids);
                    })
                    .and_then([this, language, &player](std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data) -> ll::Expected<void> {
                        std::string mUuid = player.getUuid().asString();
        
                        std::vector<std::pair<std::string, std::string>> mItems;
                        std::vector<std::string> mItemIds;

                        for (auto& item : data) {
                            auto blacklists = this->mParent.getBlacklist(item.second.at("player_uuid"));
                            if (!blacklists.has_value())
                                return ll::Unexpected(blacklists.error());

                            if (std::find(blacklists.value().begin(), blacklists.value().end(), mUuid) != blacklists.value().end())
                                continue;

                            mItems.emplace_back(item.second.at("name"), item.second.at("icon"));
                            mItemIds.push_back(item.first);
                        }

                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "market.gui.title"),
                            tr(language, "market.gui.label"),
                            mItems
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
                            this->buyItem(pl, mItemIds.at(index)).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->open(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                        });

                        form->sendPage(player, 1);

                        return {};
                    });
            });
    }

    ll::Expected<void> MarketGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "market.gui.title"), tr(language, "market.gui.label"));
                form.appendButton(tr(language, "market.gui.worldbuy"), "textures/ui/world_glyph_color", "path", [this](Player& pl) -> void {
                    this->buy(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form.appendButton(tr(language, "market.gui.trade"), "textures/ui/trade_icon", "path", [this](Player& pl) -> void {
                    this->trade(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form.appendButton(tr(language, "market.gui.personal"), "textures/ui/icon_best3", "path", [this](Player& pl) -> void {
                    this->personal(pl).or_else(modules::defaultErrorHandler<MarketPlugin>);
                });
                form.sendTo(player);
            });
    }
}
