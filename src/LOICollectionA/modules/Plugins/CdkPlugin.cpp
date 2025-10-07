#include <memory>
#include <string>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <mc/safety/RedactableString.h>

#include <nlohmann/json.hpp>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/Plugins/ChatPlugin.h"

#include "LOICollectionA/utils/InventoryUtils.h"
#include "LOICollectionA/utils/ScoreboardUtils.h"
#include "LOICollectionA/utils/SystemUtils.h"
#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/CdkPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct CdkPlugin::operation {
        std::string Id;
    };

    struct CdkPlugin::Impl {
        bool ModuleEnabled = false;
        
        std::unique_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
    };

    CdkPlugin::CdkPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    CdkPlugin::~CdkPlugin() = default;

    JsonStorage* CdkPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* CdkPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void CdkPlugin::gui::convert(Player& player) {
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

    void CdkPlugin::gui::cdkNew(Player& player) {
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
                return this->open(pl);
            }

            if (!this->mParent.getDatabase()->has(mObjectCdk)) {
                int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                std::string mObjectTime = time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time, "0") : "0";

                nlohmann::ordered_json dataList = {
                    { "personal", (bool)std::get<uint64>(dt->at("Toggle")) },
                    { "player", nlohmann::ordered_json::array() },
                    { "scores", nlohmann::ordered_json::object() },
                    { "item", nlohmann::ordered_json::array() },
                    { "title", nlohmann::ordered_json::object() },
                    { "time", mObjectTime }
                };

                this->mParent.getDatabase()->set(mObjectCdk, dataList);
                this->mParent.getDatabase()->save();
            }

            this->mParent.getLogger()->info(fmt::runtime(tr({}, "cdk.log1")), mObjectCdk);
        });
    }

    void CdkPlugin::gui::cdkRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::ModalForm form(tr(mObjectLanguage, "cdk.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "cdk.gui.remove.content")), id),
            tr(mObjectLanguage, "cdk.gui.remove.yes"), tr(mObjectLanguage, "cdk.gui.remove.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper) 
                return this->open(pl);

            this->mParent.getDatabase()->remove(id);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(tr({}, "cdk.log2")), id);
        });
    }

    void CdkPlugin::gui::cdkRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.remove.label"));
        for (std::string& mItem : this->mParent.getDatabase()->keys()) {
            form.appendButton(mItem, [this, mItem](Player& pl) {
                this->cdkRemoveInfo(pl, mItem);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) return this->open(pl);
        });
    }

    void CdkPlugin::gui::cdkAwardScore(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.score.input1"), tr(mObjectLanguage, "cdk.gui.award.score.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.score.input2"), tr(mObjectLanguage, "cdk.gui.award.score.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            std::string mObjective = std::get<std::string>(dt->at("Input1"));

            if (mObjective.empty() || !ScoreboardUtils::hasScoreboard(mObjective)) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->cdkAwardInfo(pl, id);
            }

            int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            this->mParent.getDatabase()->set_ptr("/" + id + "/scores/" + mObjective, mScore);
            this->mParent.getDatabase()->save(); 
        });
    }

    void CdkPlugin::gui::cdkAwardItem(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.item.input1"), tr(mObjectLanguage, "cdk.gui.award.item.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.item.input2"), tr(mObjectLanguage, "cdk.gui.award.item.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "cdk.gui.award.item.input3"), tr(mObjectLanguage, "cdk.gui.award.item.input3.placeholder"));
        form.appendInput("Input4", tr(mObjectLanguage, "cdk.gui.award.item.input4"), tr(mObjectLanguage, "cdk.gui.award.item.input4.placeholder"));
        form.appendDropdown("dropdown2", tr(mObjectLanguage, "cdk.gui.award.item.dropdown"), { "universal", "nbt" });
        form.sendTo(player, [this, mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
            std::string mObjectName = std::get<std::string>(dt->at("Input2"));

            if (mObjectId.empty() || mObjectName.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->cdkAwardInfo(pl, id);
            }

            std::string mObjectType = std::get<std::string>(dt->at("dropdown2"));

            nlohmann::ordered_json mItemData = {
                { "id", mObjectId },
                { "type", mObjectType }
            };
            if (mObjectType == "universal") {
                mItemData["name"] = mObjectName;
                mItemData["quantity"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 1);
                mItemData["specialvalue"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input4")), 0);
            }

            int mIndex = (int) this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/item").size();

            this->mParent.getDatabase()->set_ptr("/" + id + "/item/" + std::to_string(mIndex), mItemData);
            this->mParent.getDatabase()->save();
        });
    }

    void CdkPlugin::gui::cdkAwardTitle(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "cdk.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "cdk.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "cdk.gui.award.title.input1"), tr(mObjectLanguage, "cdk.gui.award.title.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "cdk.gui.award.title.input2"), tr(mObjectLanguage, "cdk.gui.award.title.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->cdkAwardInfo(pl, id);

            std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));

            if (mObjectTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->cdkAwardInfo(pl, id);
            }

            int mObjectData = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
            
            this->mParent.getDatabase()->set_ptr("/" + id + "/title/" + mObjectTitle, mObjectData);
            this->mParent.getDatabase()->save();
        });
    }

    void CdkPlugin::gui::cdkAwardInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mObjectLabel = tr(mObjectLanguage, "cdk.gui.award.info.label");

        ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"),
            fmt::format(fmt::runtime(mObjectLabel), id, 
                this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/personal", false) ? "true" : "false",
                SystemUtils::formatDataTime(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/time", "None"), "None")
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

    void CdkPlugin::gui::cdkAward(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "cdk.gui.title"), tr(mObjectLanguage, "cdk.gui.award.label"));
        for (std::string& mItem : this->mParent.getDatabase()->keys()) {
            form.appendButton(mItem, [this, mItem](Player& pl) {
                this->cdkAwardInfo(pl, mItem);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void CdkPlugin::gui::open(Player& player) {
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

    void CdkPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("cdk", tr({}, "commands.cdk.description"), CommandPermissionLevel::Any);
        command.overload<operation>().text("convert").required("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->convert(player, param.Id);
            
            output.success(fmt::runtime(tr({}, "commands.cdk.success.convert")), player.getRealName(), param.Id);
        });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->convert(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void CdkPlugin::convert(Player& player, const std::string& id) {
        if (!this->isValid()) 
            return;

        auto data = this->getDatabase()->get<nlohmann::ordered_json>(id);

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (data.is_null()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
            return;
        }

        if (SystemUtils::isReach(data.value("time", ""))) {
            this->getDatabase()->remove(id);
            this->getDatabase()->save();

            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
            return;
        }

        std::string mUuid = player.getUuid().asString();
        if (auto it = data.value<nlohmann::ordered_json>("player", {}); std::find(it.begin(), it.end(), mUuid) != it.end()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips2"));
            return;
        }

        for (auto elements = data.value<nlohmann::ordered_json>("title", {}); auto& element : elements.items())
            ChatPlugin::getInstance().addTitle(player, element.key(), element.value());

        for (auto elements = data.value<nlohmann::ordered_json>("scores", {}); auto& element : elements.items())
            ScoreboardUtils::addScore(player, element.key(), element.value());

        for (auto& value : data.value<nlohmann::ordered_json>("item", {})) {
            if (value.value("type", "") == "nbt") {
                ItemStack itemStack = ItemStack::fromTag(CompoundTag::fromSnbt(value.value("id", ""))->mTags);
                InventoryUtils::giveItem(player, itemStack, (int)itemStack.mCount);
            } else {
                Bedrock::Safety::RedactableString mRedactableString;
                mRedactableString.mUnredactedString = value.value("name", "");
                
                ItemStack itemStack(value.value("id", ""), 1, value.value("specialvalue", 0), nullptr);
                itemStack.setCustomName(mRedactableString);
                
                InventoryUtils::giveItem(player, itemStack, value.value("quantity", 1));
            }
        }

        player.refreshInventory();
        player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips3"));

        data.value("personal", false) ? (void)this->getDatabase()->remove(id) : data.at("player").push_back(mUuid);

        this->getDatabase()->set(id, data);
        this->getDatabase()->save();

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "cdk.log3"), player)), id);
    }

    bool CdkPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool CdkPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Cdk)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_unique<JsonStorage>(mDataPath / "cdk.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool CdkPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->registeryCommand();

        return true;
    }

    bool CdkPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->save();

        return true;
    }
}

REGISTRY_HELPER("CdkPlugin", LOICollection::Plugins::CdkPlugin, LOICollection::Plugins::CdkPlugin::getInstance())
