#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
    void CdkGui::convert(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input", tr(mObjectLanguage, "cdk.gui.convert.input"), tr(mObjectLanguage, "cdk.gui.convert.input.placeholder"));
        form.sendTo(player, [this, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return;

            std::string mCdk = std::get<std::string>(dt->at("Input"));

            if (mCdk.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                return;
            }

            this->mParent.convert(pl, mCdk);
        });
    }

    void CdkGui::cdkNew(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.new.input1"), tr(mObjectLanguage, "cdk.gui.new.input1.placeholder"));
        form.appendToggle("Toggle", tr(mObjectLanguage, "cdk.gui.new.switch"));
        form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.new.input2"), tr(mObjectLanguage, "cdk.gui.new.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);

            std::string mObjectCdk = std::get<std::string>(dt->at("Input1"));

            if (mObjectCdk.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                this->open(pl);
                return;
            }

            this->mParent.create(
                mObjectCdk,
                SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0),
                static_cast<bool>(std::get<uint64>(dt->at("Toggle")))
            );
        
            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "cdk.log1"), pl)), mObjectCdk);
        });
    }

    void CdkGui::cdkRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        ll::form::ModalForm form(tr(mObjectLanguage, "cdk.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "cdk.gui.remove.content")), id),
            tr(mObjectLanguage, "cdk.gui.remove.yes"),
            tr(mObjectLanguage, "cdk.gui.remove.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper) 
                return this->open(pl);

            this->mParent.remove(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "cdk.log2"), pl)), id);
        });
    }

    void CdkGui::cdkRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "cdk.gui.title"),
            tr(mObjectLanguage, "cdk.gui.remove.label"),
            this->mParent.getCdks()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->cdkRemoveInfo(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void CdkGui::cdkAwardScore(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.score.input1"), tr(mObjectLanguage, "cdk.gui.award.score.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.score.input2"), tr(mObjectLanguage, "cdk.gui.award.score.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            std::string mObjective = std::get<std::string>(dt->at("Input1"));

            if (mObjective.empty() || !ScoreboardUtils::hasScoreboard(mObjective)) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                this->cdkAwardInfo(pl, id);
                return;
            }

            int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            this->mParent.getDatabase()->set_ptr("/" + id + "/scores/" + mObjective, mScore);
            this->mParent.getDatabase()->save(); 
        });
    }

    void CdkGui::cdkAwardItemCommon(Player& player, const std::string& id, const std::string& type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }
        
        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.item.custom.input1"), tr(mObjectLanguage, "cdk.gui.award.item.input1.custom.placeholder"));
        
        if (type == "universal") { 
            form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.item.custom.input2"), tr(mObjectLanguage, "cdk.gui.award.item.input2.custom.placeholder"));
            form.appendInput("Input3", tr(mObjectLanguage, "cdk.gui.award.item.custom.input3"), tr(mObjectLanguage, "cdk.gui.award.item.input3.custom.placeholder"));
            form.appendInput("Input4", tr(mObjectLanguage, "cdk.gui.award.item.custom.input4"), tr(mObjectLanguage, "cdk.gui.award.item.input4.custom.placeholder"));
        }

        form.sendTo(player, [this, mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
            if (mObjectId.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                this->cdkAwardInfo(pl, id);
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

            int mIndex = static_cast<int>(this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/item").size());

            this->mParent.getDatabase()->set_ptr("/" + id + "/item/" + std::to_string(mIndex), mItemData);
            this->mParent.getDatabase()->save();
        });
    }

    void CdkGui::cdkAwardItemType(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "cdk.gui.award.item.dropdown"), { "universal", "nbt" });
        form.sendTo(player, [this, mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            this->cdkAwardItemCommon(pl, id, std::get<std::string>(dt->at("dropdown")));
        });
    }

    void CdkGui::cdkAwardInventoryConfirm(Player& player, const std::string& id, int slot) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        ItemStack mItemStack = player.mInventory->mInventory->getItem(slot);
        if (!mItemStack || mItemStack.isNull()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->cdkAwardInfo(player, id);
            return;
        }

        ll::form::ModalForm form(tr(mObjectLanguage, "cdk.gui.title"),
            fmt::format(fmt::runtime(tr(mObjectLanguage, "cdk.gui.award.item.introduce")),
                mItemStack.getName(),
                mItemStack.mCount,
                mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0)
            ),
            tr(mObjectLanguage, "cdk.gui.award.item.inventory.yes"),
            tr(mObjectLanguage, "cdk.gui.award.item.inventory.no")
        );
        form.sendTo(player, [this, mObjectLanguage, id, slot](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                ItemStack mItemStack = pl.mInventory->mInventory->getItem(slot);
                if (!mItemStack || mItemStack.isNull()) {
                    pl.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

                    this->cdkAwardInfo(pl, id);
                    return;
                }

                nlohmann::ordered_json mItemData = {
                    { "id", mItemStack.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0) },
                    { "type", "nbt" }
                };

                int mIndex = static_cast<int>(this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/item").size());

                this->mParent.getDatabase()->set_ptr("/" + id + "/item/" + std::to_string(mIndex), mItemData);
                this->mParent.getDatabase()->save();

                return;
            }

            this->cdkAwardInfo(pl, id);
        });
    }

    void CdkGui::cdkAwardInventory(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        std::vector<std::string> mItems;
        std::vector<int> mItemSlots;

        for (int i = 0; i < player.mInventory->mInventory->getContainerSize(); i++) {
            ItemStack mItemStack = player.mInventory->mInventory->getItem(i);
            
            if (!mItemStack || mItemStack.isNull())
                continue;

            std::string mItemName = fmt::format(fmt::runtime(tr(mObjectLanguage, "cdk.gui.award.item.inventory.text")), 
                mItemStack.getName(), std::to_string(mItemStack.mCount)
            );

            mItems.push_back(mItemName);
            mItemSlots.push_back(i);
        }

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "cdk.gui.title"),
            tr(mObjectLanguage, "cdk.gui.award.item.inventory.label"),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, id, mItemSlots = std::move(mItemSlots)](Player& pl, int index) -> void {
            this->cdkAwardInventoryConfirm(pl, id, mItemSlots.at(index));
        });
        form->setCloseCallback([this, id](Player& pl) -> void {
            this->cdkAwardInfo(pl, id);
        });

        form->sendPage(player, 1);
    }

    void CdkGui::cdkAwardItem(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
        form.appendButton(tr(mObjectLanguage, "cdk.gui.award.item.inventory"), [this, id](Player& pl) -> void {
            this->cdkAwardInventory(pl, id);
        });
        form.appendButton(tr(mObjectLanguage, "cdk.gui.award.item.custom"), [this, id](Player& pl) -> void {
            this->cdkAwardItemType(pl, id);
        });
        form.sendTo(player, [this, ids = id](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->cdkAwardInfo(pl, ids);
        });
    }

    void CdkGui::cdkAwardTitle(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.title.input1"), tr(mObjectLanguage, "cdk.gui.award.title.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.title.input2"), tr(mObjectLanguage, "cdk.gui.award.title.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));

            if (mObjectTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                this->cdkAwardInfo(pl, id);
                return;
            }

            int mObjectData = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
            
            this->mParent.getDatabase()->set_ptr("/" + id + "/title/" + mObjectTitle, mObjectData);
            this->mParent.getDatabase()->save();
        });
    }

    void CdkGui::cdkAwardInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "cdk.gui.error"));

            this->open(player);
            return;
        }

        std::string mObjectLabel = tr(mObjectLanguage, "cdk.gui.award.info.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"),
            fmt::format(fmt::runtime(mObjectLabel), id, 
                this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/personal", false) ? "true" : "false",
                SystemUtils::toFormatTime(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/time", "None"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "cdk.gui.award.score"), "textures/items/diamond_sword", "path", [this, id](Player& pl) -> void {
            this->cdkAwardScore(pl, id);
        });
        form.appendButton(tr(mObjectLanguage, "cdk.gui.award.item"), "textures/items/diamond", "path", [this, id](Player& pl) -> void {
            this->cdkAwardItem(pl, id);
        });
        form.appendButton(tr(mObjectLanguage, "cdk.gui.award.title"), "textures/ui/backup_replace", "path", [this, id](Player& pl) -> void {
            this->cdkAwardTitle(pl, id);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void CdkGui::cdkAward(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "cdk.gui.title"),
            tr(mObjectLanguage, "cdk.gui.award.label"),
            this->mParent.getCdks()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->cdkAwardInfo(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void CdkGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.label"));
        form.appendButton(tr(mObjectLanguage, "cdk.gui.addCdk"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
            this->cdkNew(pl);
        });
        form.appendButton(tr(mObjectLanguage, "cdk.gui.removeCdk"), "textures/ui/cancel", "path", [this](Player& pl) -> void {
            this->cdkRemove(pl);
        });
        form.appendButton(tr(mObjectLanguage, "cdk.gui.addAward"), "textures/ui/color_picker", "path", [this](Player& pl) -> void {
            this->cdkAward(pl);
        });
        form.sendTo(player);
    }
}
