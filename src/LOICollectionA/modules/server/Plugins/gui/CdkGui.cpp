#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/CdkPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/include/server/Plugins/gui/CdkGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    ll::Expected<void> CdkGui::convert(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "cdk.gui.title"));
                form.appendLabel(tr(language, "cdk.gui.label"));
                form.appendInput("Input", tr(language, "cdk.gui.convert.input"), tr(language, "cdk.gui.convert.input.placeholder"));
                form.sendTo(player, [this, language](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) return;

                    std::string mCdk = std::get<std::string>(dt->at("Input"));

                    if (mCdk.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));

                        return;
                    }

                    this->mParent.convert(pl, mCdk)
                        .or_else([](ll::Error e) -> ll::Expected<void> {
                            if (e.isA<ll::ErrorCodeError>() 
                                && (e.as<ll::ErrorCodeError>().ec == CdkPlugin::makeErrorCode(CdkPluginErrorCode::NotFound)
                                    || e.as<ll::ErrorCodeError>().ec == CdkPlugin::makeErrorCode(CdkPluginErrorCode::Received)))
                                return {};

                            return ll::Unexpected(e);
                        })
                        .or_else(modules::defaultErrorHandler<CdkPlugin>);
                });
            });
    }

    ll::Expected<void> CdkGui::cdkNew(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "cdk.gui.title"));
                form.appendLabel(tr(language, "cdk.gui.label"));
                form.appendInput("Input1", tr(language, "cdk.gui.new.input1"), tr(language, "cdk.gui.new.input1.placeholder"));
                form.appendToggle("Toggle", tr(language, "cdk.gui.new.switch"));
                form.appendInput("Input2", tr(language, "cdk.gui.new.input2"), tr(language, "cdk.gui.new.input2.placeholder"));
                form.sendTo(player, [this, language](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->open(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);

                        return;
                    }

                    std::string mObjectCdk = std::get<std::string>(dt->at("Input1"));

                    if (mObjectCdk.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));

                        this->open(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);

                        return;
                    }

                    this->mParent.create(
                        mObjectCdk,
                        SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0),
                        static_cast<bool>(std::get<uint64>(dt->at("Toggle")))
                    ).or_else(modules::defaultErrorHandler<CdkPlugin>);
                
                    this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "cdk.log1"), pl)), mObjectCdk);
                });
            });
    }

    ll::Expected<void> CdkGui::cdkRemoveInfo(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::ModalForm form(tr(language, "cdk.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "cdk.gui.remove.content")), id),
                            tr(language, "cdk.gui.remove.yes"),
                            tr(language, "cdk.gui.remove.no")
                        );
                        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
                            if (result != ll::form::ModalFormSelectedButton::Upper) {
                                this->open(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            this->mParent.remove(id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "cdk.log2"), pl)), id);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkRemove(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getCdks()
                    .transform([this, language, &player](const std::vector<std::string>& cdks) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "cdk.gui.title"),
                            tr(language, "cdk.gui.remove.label"),
                            cdks
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this](Player& pl, const std::string& response) -> void {
                            this->cdkRemoveInfo(pl, response).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->open(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardScore(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::CustomForm form(tr(language, "cdk.gui.title"));
                        form.appendLabel(tr(language, "cdk.gui.label"));
                        form.appendInput("Input1", tr(language, "cdk.gui.award.score.input1"), tr(language, "cdk.gui.award.score.input1.placeholder"));
                        form.appendInput("Input2", tr(language, "cdk.gui.award.score.input2"), tr(language, "cdk.gui.award.score.input2.placeholder"));
                        form.sendTo(player, [this, language, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            std::string mObjective = std::get<std::string>(dt->at("Input1"));

                            if (mObjective.empty() || !ScoreboardUtils::hasScoreboard(mObjective)) {
                                pl.sendMessage(tr(language, "generic.tips.noinput"));

                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

                            this->mParent.getDatabase()->set_ptr("/" + id + "/scores/" + mObjective, mScore);
                            this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardItemCommon(Player& player, const std::string& id, const std::string& type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, type, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, type, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::CustomForm form(tr(language, "cdk.gui.title"));
                        form.appendLabel(tr(language, "cdk.gui.label"));
                        form.appendInput("Input1", tr(language, "cdk.gui.award.item.custom.input1"), tr(language, "cdk.gui.award.item.input1.custom.placeholder"));
                        
                        if (type == "universal") { 
                            form.appendInput("Input2", tr(language, "cdk.gui.award.item.custom.input2"), tr(language, "cdk.gui.award.item.input2.custom.placeholder"));
                            form.appendInput("Input3", tr(language, "cdk.gui.award.item.custom.input3"), tr(language, "cdk.gui.award.item.input3.custom.placeholder"));
                            form.appendInput("Input4", tr(language, "cdk.gui.award.item.custom.input4"), tr(language, "cdk.gui.award.item.input4.custom.placeholder"));
                        }

                        form.sendTo(player, [this, language, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                            if (mObjectId.empty()) {
                                pl.sendMessage(tr(language, "generic.tips.noinput"));

                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            nlohmann::ordered_json mItemData = {
                                { "id", mObjectId },
                                { "type", type }
                            };
                            
                            if (type == "universal") {
                                mItemData["name"] = std::get<std::string>(dt->at("Input2"));
                                mItemData["quantity"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 1);
                                mItemData["specialvalue"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);
                            }

                            int mIndex = static_cast<int>(this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/item").value_or(nlohmann::ordered_json::array()).size());

                            this->mParent.getDatabase()->set_ptr("/" + id + "/item/" + std::to_string(mIndex), mItemData);
                            this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardItemType(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::CustomForm form(tr(language, "cdk.gui.title"));
                        form.appendLabel(tr(language, "cdk.gui.label"));
                        form.appendDropdown("dropdown", tr(language, "cdk.gui.award.item.dropdown"), { "universal", "nbt" });
                        form.sendTo(player, [this, language, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            this->cdkAwardItemCommon(pl, id, std::get<std::string>(dt->at("dropdown"))).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardInventoryConfirm(Player& player, const std::string& id, int slot) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, slot, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, slot, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ItemStack mItemStack = player.mInventory->mInventory->getItem(slot);
                        if (!mItemStack || mItemStack.isNull()) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->cdkAwardInfo(player, id);
                        }

                        ll::form::ModalForm form(tr(language, "cdk.gui.title"),
                            fmt::format(fmt::runtime(tr(language, "cdk.gui.award.item.introduce")),
                                mItemStack.getName(),
                                mItemStack.mCount,
                                mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0)
                            ),
                            tr(language, "cdk.gui.award.item.inventory.yes"),
                            tr(language, "cdk.gui.award.item.inventory.no")
                        );
                        form.sendTo(player, [this, language, id, slot](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                            if (result == ll::form::ModalFormSelectedButton::Upper) {
                                ItemStack mItemStack = pl.mInventory->mInventory->getItem(slot);
                                if (!mItemStack || mItemStack.isNull()) {
                                    pl.sendMessage(tr(language, "cdk.gui.error"));

                                    this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                    return;
                                }

                                nlohmann::ordered_json mItemData = {
                                    { "id", mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0) },
                                    { "type", "nbt" }
                                };

                                int mIndex = static_cast<int>(this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/item").value_or(nlohmann::ordered_json::array()).size());

                                this->mParent.getDatabase()->set_ptr("/" + id + "/item/" + std::to_string(mIndex), mItemData);
                                this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardInventory(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        std::vector<std::string> mItems;
                        std::vector<int> mItemSlots;

                        for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
                            ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
                            
                            if (!mItemStack || mItemStack.isNull())
                                continue;

                            std::string mItemName = fmt::format(fmt::runtime(tr(language, "cdk.gui.award.item.inventory.text")), 
                                mItemStack.getName(), std::to_string(mItemStack.mCount)
                            );

                            mItems.push_back(mItemName);
                            mItemSlots.push_back(i);
                        }

                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "cdk.gui.title"),
                            tr(language, "cdk.gui.award.item.inventory.label"),
                            mItems
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this, language, id, mItemSlots = std::move(mItemSlots)](Player& pl, int index) -> void {
                            this->cdkAwardInventoryConfirm(pl, id, mItemSlots.at(index)).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form->setCloseCallback([this, id](Player& pl) -> void {
                            this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        form->sendPage(player, 1);

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardItem(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::SimpleForm form(tr(language, "cdk.gui.title"), tr(language, "cdk.gui.label"));
                        form.appendButton(tr(language, "cdk.gui.award.item.inventory"), [this, id](Player& pl) -> void {
                            this->cdkAwardInventory(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form.appendButton(tr(language, "cdk.gui.award.item.custom"), [this, id](Player& pl) -> void {
                            this->cdkAwardItemType(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form.sendTo(player, [this, ids = id](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->cdkAwardInfo(pl, ids).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardTitle(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::CustomForm form(tr(language, "cdk.gui.title"));
                        form.appendLabel(tr(language, "cdk.gui.label"));
                        form.appendInput("Input1", tr(language, "cdk.gui.award.title.input1"), tr(language, "cdk.gui.award.title.input1.placeholder"));
                        form.appendInput("Input2", tr(language, "cdk.gui.award.title.input2"), tr(language, "cdk.gui.award.title.input2.placeholder"));
                        form.sendTo(player, [this, language, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));

                            if (mObjectTitle.empty()) {
                                pl.sendMessage(tr(language, "generic.tips.noinput"));

                                this->cdkAwardInfo(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);

                                return;
                            }

                            int mObjectData = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                            
                            this->mParent.getDatabase()->set_ptr("/" + id + "/title/" + mObjectTitle, mObjectData);
                            this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAwardInfo(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "cdk.gui.error"));

                            return this->open(player);
                        }

                        ll::form::SimpleForm form(tr(language, "cdk.gui.title"),
                            fmt::format(fmt::runtime(tr(language, "cdk.gui.award.info.label")), id, 
                                this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/personal").value_or(false) ? "true" : "false",
                                SystemUtils::toFormatTime(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/time").value_or("None"), "None")
                            )
                        );
                        form.appendButton(tr(language, "cdk.gui.award.score"), "textures/items/diamond_sword", "path", [this, id](Player& pl) -> void {
                            this->cdkAwardScore(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form.appendButton(tr(language, "cdk.gui.award.item"), "textures/items/diamond", "path", [this, id](Player& pl) -> void {
                            this->cdkAwardItem(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form.appendButton(tr(language, "cdk.gui.award.title"), "textures/ui/backup_replace", "path", [this, id](Player& pl) -> void {
                            this->cdkAwardTitle(pl, id).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->open(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> CdkGui::cdkAward(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getCdks()
                    .transform([this, language, &player](const std::vector<std::string>& cdks) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "cdk.gui.title"),
                            tr(language, "cdk.gui.award.label"),
                            cdks
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this](Player& pl, const std::string& response) -> void {
                            this->cdkAwardInfo(pl, response).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->open(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> CdkGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "cdk.gui.title"), tr(language, "cdk.gui.label"));
                form.appendButton(tr(language, "cdk.gui.addCdk"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
                    this->cdkNew(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);
                });
                form.appendButton(tr(language, "cdk.gui.removeCdk"), "textures/ui/cancel", "path", [this](Player& pl) -> void {
                    this->cdkRemove(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);
                });
                form.appendButton(tr(language, "cdk.gui.addAward"), "textures/ui/color_picker", "path", [this](Player& pl) -> void {
                    this->cdkAward(pl).or_else(modules::defaultErrorHandler<CdkPlugin>);
                });
                form.sendTo(player);
            });
    }
}
