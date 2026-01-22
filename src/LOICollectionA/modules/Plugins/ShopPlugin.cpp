#include <atomic>
#include <memory>
#include <string>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

#include <mc/world/Minecraft.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CurrentCmdVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/ServerCommandOrigin.h>
#include <mc/server/commands/MinecraftCommands.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/Plugins/ChatPlugin.h"

#include "LOICollectionA/include/ServerEvents/modules/ShopEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/InventoryUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/ShopPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    enum class ShopObject;

    constexpr inline auto ShopObjectName = ll::command::enum_name_v<ShopObject>;

    struct ShopPlugin::operation {
        ll::command::SoftEnum<ShopObject> Object;
    };

    struct ShopPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;

        std::unique_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr ShopCreateEventListener;
        ll::event::ListenerPtr ShopDeleteEventListener;
    };

    ShopPlugin::ShopPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    ShopPlugin::~ShopPlugin() = default;

    ShopPlugin& ShopPlugin::getInstance() {
        static ShopPlugin instance;
        return instance;
    }
    
    JsonStorage* ShopPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* ShopPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void ShopPlugin::gui::editNewInfo(Player& player, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button1.input1"), tr(mObjectLanguage, "shop.gui.button1.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button1.input2"), tr(mObjectLanguage, "shop.gui.button1.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button1.input3"), tr(mObjectLanguage, "shop.gui.button1.input3.placeholder"));

        switch (type) {
            case ShopType::buy:
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button1.input4"), tr(mObjectLanguage, "shop.gui.button1.input4.placeholder"));
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button1.input5"), tr(mObjectLanguage, "shop.gui.button1.input5.placeholder"));
                break;
            case ShopType::sell:
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button1.input4"), tr(mObjectLanguage, "shop.gui.button1.input4.placeholder"));
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button1.input6"), tr(mObjectLanguage, "shop.gui.button1.input6.placeholder"));
                form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button1.input7"), tr(mObjectLanguage, "shop.gui.button1.input7.placeholder"));
                break;
        };

        form.sendTo(player, [this, mObjectLanguage, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editNew(pl);
            
            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
            std::string mObjectContent = std::get<std::string>(dt->at("Input3"));

            if (mObjectId.empty() || mObjectTitle.empty() || mObjectContent.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->editNew(pl);
            }

            nlohmann::ordered_json data = {
                { "title", mObjectTitle },
                { "content", mObjectContent },
                { "info", nlohmann::ordered_json::object() },
                { "classiflcation", nlohmann::ordered_json::array() }
            };

            switch (type) {
                case ShopType::buy: {
                    data["type"] = "buy";
                    data["info"].update({
                        { "exit", std::get<std::string>(dt->at("Input4")) },
                        { "score", std::get<std::string>(dt->at("Input5")) }
                    });
                    break;
                }
                case ShopType::sell: {
                    data["type"] = "sell";
                    data["info"].update({
                        { "exit", std::get<std::string>(dt->at("Input4")) },
                        { "title", std::get<std::string>(dt->at("Input6")) },
                        { "item", std::get<std::string>(dt->at("Input7")) }
                    });
                    break;
                }
            }

            this->mParent.create(mObjectId, data);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log1"), pl)), mObjectId);
        });
    }

    void ShopPlugin::gui::editNew(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
        form.appendButton("Buy", [this](Player& pl) {
            this->editNewInfo(pl, ShopType::buy);
        });
        form.appendButton("Sell", [this](Player& pl) {
            this->editNewInfo(pl, ShopType::sell);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->edit(pl);
        });
    }

    void ShopPlugin::gui::editRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::ModalForm form(tr(mObjectLanguage, "shop.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "shop.gui.button2.content")), id),
            tr(mObjectLanguage, "shop.gui.button2.yes"), tr(mObjectLanguage, "shop.gui.button2.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper)
                return;

            this->mParent.remove(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log2"), pl)), id);
        });
    }

    void ShopPlugin::gui::editRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "shop.gui.title"),
            tr(mObjectLanguage, "shop.gui.label"),
            this->mParent.getDatabase()->keys()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->editRemoveInfo(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->edit(pl);
        });

        form->sendPage(player, 1);
    }

    void ShopPlugin::gui::editAwardSetting(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
        form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button3.input2"), tr(mObjectLanguage, "shop.gui.button3.input2.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""));
        form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.input3"), tr(mObjectLanguage, "shop.gui.button3.input3.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/content", ""));
        form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.input4"), tr(mObjectLanguage, "shop.gui.button3.input4.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/exit", ""));

        switch (type) {
            case ShopType::buy:
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.input5"), tr(mObjectLanguage, "shop.gui.button3.input5.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/score", ""));
                break;
            case ShopType::sell:
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.input6"), tr(mObjectLanguage, "shop.gui.button3.input6.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/title", ""));
                form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.input7"), tr(mObjectLanguage, "shop.gui.button3.input7.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/item", ""));
                break;
        };

        form.sendTo(player, [this, mObjectLanguage, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editAwardContent(pl, id, type);

            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
            std::string mObjectContent = std::get<std::string>(dt->at("Input3"));

            if (mObjectTitle.empty() || mObjectContent.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->editAwardSetting(pl, id, type);
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/title", mObjectTitle);
            this->mParent.getDatabase()->set_ptr("/" + id + "/content", mObjectContent);
            this->mParent.getDatabase()->set_ptr("/" + id + "/info/exit", std::get<std::string>(dt->at("Input4")));

            switch (type) {
                case ShopType::buy:
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/score", std::get<std::string>(dt->at("Input5")));
                    break;
                case ShopType::sell:
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/title", std::get<std::string>(dt->at("Input6")));
                    this->mParent.getDatabase()->set_ptr("/" + id + "/info/item", std::get<std::string>(dt->at("Input7")));
                    break;
            };

            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log4"), pl)), id);
        });
    }

    void ShopPlugin::gui::editAwardNewInfo(Player& player, const std::string& id, ShopType type, AwardType awardType) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button3.new.input1"), tr(mObjectLanguage, "shop.gui.button3.new.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button3.new.input2"), tr(mObjectLanguage, "shop.gui.button3.new.input2.placeholder"));

        switch (awardType) {
            case AwardType::commodity:
                form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), tr(mObjectLanguage, "shop.gui.button3.new.input3.placeholder"));
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), tr(mObjectLanguage, "shop.gui.button3.new.input4.placeholder"));
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), tr(mObjectLanguage, "shop.gui.button3.new.input5.placeholder"));
                form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), tr(mObjectLanguage, "shop.gui.button3.new.input7.placeholder"));
                break;
            case AwardType::title:
                form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), tr(mObjectLanguage, "shop.gui.button3.new.input3.placeholder"));
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                
                if (type == ShopType::buy)
                    form.appendInput("Input9", tr(mObjectLanguage, "shop.gui.button3.new.input9"), tr(mObjectLanguage, "shop.gui.button3.new.input9.placeholder"));
                
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), tr(mObjectLanguage, "shop.gui.button3.new.input4.placeholder"));
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), tr(mObjectLanguage, "shop.gui.button3.new.input5.placeholder"));
                form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), tr(mObjectLanguage, "shop.gui.button3.new.input7.placeholder"));
                form.appendInput("Input8", tr(mObjectLanguage, "shop.gui.button3.new.input8"), tr(mObjectLanguage, "shop.gui.button3.new.input8.placeholder"));
                break;
            case AwardType::from:
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                break;
        };

        form.sendTo(player, [this, mObjectLanguage, id, type, awardType](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->editAwardContent(pl, id, type);

            std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));
            std::string mObjectImage = std::get<std::string>(dt->at("Input2"));

            if (mObjectTitle.empty() || mObjectImage.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->editAwardContent(pl, id, type);
            }

            nlohmann::ordered_json data;
            switch (awardType) {
                case AwardType::commodity: {
                    std::string mObjectObjective = std::get<std::string>(dt->at("Input4"));
                    std::string mObjectScore = std::get<std::string>(dt->at("Input5"));

                    data.update({
                        { "title", mObjectTitle },
                        { "image", mObjectImage },
                        { "introduce", std::get<std::string>(dt->at("Input3")) },
                        { "number", std::get<std::string>(dt->at("Input7")) },
                        { "id", std::get<std::string>(dt->at("Input6")) },
                        { "scores", nlohmann::ordered_json::object() },
                        { "type", "commodity" }
                    });

                    if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                        data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                    break;
                }
                case AwardType::title: {
                    std::string mObjectObjective = std::get<std::string>(dt->at("Input4"));
                    std::string mObjectScore = std::get<std::string>(dt->at("Input5"));

                    data.update({
                        { "title", mObjectTitle },
                        { "image", mObjectImage },
                        { "introduce", std::get<std::string>(dt->at("Input3")) },
                        { "confirmButton", std::get<std::string>(dt->at("Input8")) },
                        { "cancelButton", std::get<std::string>(dt->at("Input7")) },
                        { "id", std::get<std::string>(dt->at("Input6")) },
                        { "scores", nlohmann::ordered_json::object() },
                        { "type", "title" }
                    });

                    if (!mObjectObjective.empty() && ScoreboardUtils::hasScoreboard(mObjectObjective))
                        data["scores"][mObjectObjective] = SystemUtils::toInt((mObjectScore.empty() ? "100" : mObjectScore), 0);

                    if (type == ShopType::buy)
                        data["time"] = SystemUtils::toInt(std::get<std::string>(dt->at("Input9")), 0);

                    break;
                }
                case AwardType::from: {
                    data.update({
                        { "title", mObjectTitle },
                        { "image", mObjectImage },
                        { "id", std::get<std::string>(dt->at("Input6")) },
                        { "type", "from" }
                    });

                    break;
                }
            };

            int mIndex = static_cast<int>(this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").size());

            this->mParent.getDatabase()->set_ptr("/" + id + "/classiflcation/" + std::to_string(mIndex), data);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log5"), pl)), id);
        });
    }

    void ShopPlugin::gui::editAwardNew(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
        form.appendButton("Commodity", [this, id, type](Player& pl) -> void {
            this->editAwardNewInfo(pl, id, type, AwardType::commodity);
        });
        form.appendButton("Title", [this, id, type](Player& pl) -> void {
            this->editAwardNewInfo(pl, id, type, AwardType::title);
        });
        form.appendButton("From", [this, id, type](Player& pl) -> void {
            this->editAwardNewInfo(pl, id, type, AwardType::from);
        });
        form.sendTo(player, [this, ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->editAwardContent(pl, ids, type);
        });
    }

    void ShopPlugin::gui::editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "shop.gui.error"));

            this->edit(player);
            return;
        }
        
        ll::form::ModalForm form(tr(mObjectLanguage, "menu.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "shop.gui.button3.remove.content")), packageid),
            tr(mObjectLanguage, "shop.gui.button3.remove.yes"), tr(mObjectLanguage, "shop.gui.button3.remove.no")
        );
        form.sendTo(player, [this, id, packageid](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper)
                return this->edit(pl);

            auto mContent = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation");
            for (int i = static_cast<int>(mContent.size() - 1); i >= 0; i--) {
                if (mContent.at(i).value("title", "") == packageid)
                    mContent.erase(i);
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/classiflcation", mContent);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log3"), pl)), id, packageid);
        });
    }

    void ShopPlugin::gui::editAwardRemove(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mNames;
        for (nlohmann::ordered_json& item : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation"))
            mNames.push_back(item.value("title", ""));

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "shop.gui.title"),
            tr(mObjectLanguage, "shop.gui.label"),
            mNames
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, id](Player& pl, const std::string& response) -> void {
            this->editAwardRemoveInfo(pl, id, response);
        });
        form->setCloseCallback([this, id, type](Player& pl) -> void {
            this->editAwardContent(pl, id, type);
        });

        form->sendPage(player, 1);
    }

    void ShopPlugin::gui::editAwardContent(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "shop.gui.error"));

            this->edit(player);
            return;
        }

        ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "shop.gui.button3.label")), id)
        );
        form.appendButton(tr(mObjectLanguage, "shop.gui.button3.setting"), "textures/ui/icon_setting", "path", [this, id, type](Player& pl) -> void {
            this->editAwardSetting(pl, id, type);
        });

        form.appendButton(tr(mObjectLanguage, "shop.gui.button3.new"), "textures/ui/icon_sign", "path", [this, id, type](Player& pl) -> void {
            this->editAwardNew(pl, id, type);
        });
        form.appendButton(tr(mObjectLanguage, "shop.gui.button3.remove"), "textures/ui/icon_trash", "path", [this, id, type](Player& pl) -> void {
            this->editAwardRemove(pl, id, type);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->editAward(pl);
        });
    }

    void ShopPlugin::gui::editAward(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "shop.gui.title"),
            tr(mObjectLanguage, "shop.gui.label"),
            this->mParent.getDatabase()->keys()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            auto mObjectType = this->mParent.getDatabase()->get_ptr<std::string>("/" + response + "/type", "buy");

            switch (ll::hash_utils::doHash(mObjectType)) {
                case ll::hash_utils::doHash("buy"):
                    this->editAwardContent(pl, response, ShopType::buy);
                    break;
                case ll::hash_utils::doHash("sell"):
                    this->editAwardContent(pl, response, ShopType::sell);
                    break;
            }
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->edit(pl);
        });

        form->sendPage(player, 1);
    }

    void ShopPlugin::gui::edit(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
        form.appendButton(tr(mObjectLanguage, "shop.gui.button1"), "textures/ui/achievements", "path", [this](Player& pl) -> void {
            this->editNew(pl);
        });
        form.appendButton(tr(mObjectLanguage, "shop.gui.button2"), "textures/ui/world_glyph_color", "path", [this](Player& pl) -> void {
            this->editRemove(pl);
        });
        form.appendButton(tr(mObjectLanguage, "shop.gui.button3"), "textures/ui/editIcon", "path", [this](Player& pl) -> void {
            this->editAward(pl);
        });
        form.sendTo(player);
    }

    void ShopPlugin::gui::menu(Player& player, const std::string& id, ShopType type) {
        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

        std::vector<std::pair<std::string, std::string>> mItems;
        std::vector<std::string> mItemIds;
        std::vector<std::string> mItemTypes;
        std::vector<int> mItemIndexs;

        for (size_t i = 0; i < data.value("classiflcation", nlohmann::ordered_json()).size(); ++i) {
            nlohmann::ordered_json& item = data.value("classiflcation", nlohmann::ordered_json())[i];

            mItems.emplace_back(std::make_pair(LOICollectionAPI::APIUtils::getInstance().translate(item.value("title", ""), player), item.value("image", "")));
            mItemIds.push_back(item.value("id", ""));
            mItemTypes.push_back(item.value("type", ""));
            mItemIndexs.push_back(static_cast<int>(i));
        }

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("content", ""), player),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, type, id, mItemIds = std::move(mItemIds), mItemTypes = std::move(mItemTypes), mItemIndexs = std::move(mItemIndexs)](Player& pl, int index) -> void {
            int mIndex = mItemIndexs.at(index);
            std::string mType = mItemTypes.at(index);

            switch (ll::hash_utils::doHash(mType)) {
                case ll::hash_utils::doHash("commodity"):
                    this->commodity(pl, mIndex, id, type);
                    break;
                case ll::hash_utils::doHash("title"):
                    this->title(pl, mIndex, id, type);
                    break;
                case ll::hash_utils::doHash("from"):
                    this->open(pl, mItemIds.at(index));
                    break;
                default:
                    this->mParent.getLogger()->error("Unknown UI type {}.", mType);
                    break;
            };
        });
        form->setCloseCallback([this, data](Player& pl) -> void {
            this->mParent.executeCommand(pl, data.value("info", nlohmann::ordered_json{}).value("exit", ""));
        });

        form->sendPage(player, 1);
    }

    void ShopPlugin::gui::commodity(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);
        auto data = original.at("classiflcation").at(index);

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player));
        form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player));
        form.appendInput("Input", LOICollectionAPI::APIUtils::getInstance().translate(data.value("number", ""), player), "", "1");
        form.sendTo(player, [this, original, data, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->mParent.executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("exit", ""));

            int mNumber = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
            if (mNumber > 2304 || mNumber <= 0)
                return;
            if (type == ShopType::buy) {
                if (this->mParent.checkModifiedData(pl, data, mNumber)) {
                    ItemStack itemStack = data.contains("nbt") ? ItemStack::fromTag(CompoundTag::fromSnbt(data.value("nbt", ""))->mTags)
                        : ItemStack(data.value("id", ""), 1, 0, nullptr);
                    InventoryUtils::giveItem(pl, itemStack, mNumber);
                    pl.refreshInventory();
                    return;
                }
                return this->mParent.executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("score", ""));
            }
            if (InventoryUtils::isItemInInventory(pl, data.value("id", ""), mNumber)) {
                nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json());
                for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                    ScoreboardUtils::addScore(pl, it.key(), (it.value().get<int>() * mNumber));

                InventoryUtils::clearItem(pl, data.value("id", ""), mNumber);
                pl.refreshInventory();
                return;
            }
            this->mParent.executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("item", ""));
        });
    }

    void ShopPlugin::gui::title(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);
        auto data = original.at("classiflcation").at(index);

        ll::form::ModalForm form(
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("confirmButton", "confirm"), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("cancelButton", "cancel"), player)
        );
        form.sendTo(player, [this, original, data, type](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                std::string id = data.value("id", "None");

                if (type == ShopType::buy) {
                    if (this->mParent.checkModifiedData(pl, data, 1)) {
                        if (data.contains("time"))
                            return ChatPlugin::getInstance().addTitle(pl, id, data.value("time", 0));
                        return ChatPlugin::getInstance().addTitle(pl, id, 0);
                    }
                    return this->mParent.executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("sScore", ""));
                }
                if (ChatPlugin::getInstance().isTitle(pl, id)) {
                    nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json());
                    for (nlohmann::ordered_json::iterator it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                        ScoreboardUtils::addScore(pl, it.key(), it.value().get<int>());

                    return ChatPlugin::getInstance().delTitle(pl, id);
                }
                this->mParent.executeCommand(pl, original.value("info", nlohmann::ordered_json{}).value("title", ""));
            }
        });
    }

    void ShopPlugin::gui::open(Player& player, const std::string& id) {
        if (this->mParent.getDatabase()->has(id)) {
            auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

            if (data.empty())
                return;
            
            switch (ll::hash_utils::doHash(data.value("type", ""))) {
                case ll::hash_utils::doHash("buy"):
                    this->menu(player, id, ShopType::buy);
                    break;
                case ll::hash_utils::doHash("sell"):
                    this->menu(player, id, ShopType::sell);
                    break;
                default:
                    this->mParent.getLogger()->error("Unknown UI type {}.", data.value("type", ""));
                    break;
            };
            return;
        }

        this->mParent.getLogger()->error("ShopUI {} reading failed.", id);
    }

    void ShopPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance().tryRegisterSoftEnum(ShopObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("shop", tr({}, "commands.shop.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").required("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player, param.Object);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->mGui->edit(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void ShopPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->ShopCreateEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::ShopCreateEvent>([](LOICollection::ServerEvents::ShopCreateEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance().addSoftEnumValues(ShopObjectName, { event.getTarget() });
        });

        this->mImpl->ShopDeleteEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::ShopDeleteEvent>([](LOICollection::ServerEvents::ShopDeleteEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance().removeSoftEnumValues(ShopObjectName, { event.getTarget() });
        });
    }

    void ShopPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->ShopCreateEventListener);
        eventBus.removeListener(this->mImpl->ShopDeleteEventListener);
    }

    void ShopPlugin::create(const std::string& id, const nlohmann::ordered_json& data) {
        if (!this->isValid())
            return;

        if (!this->getDatabase()->has(id))
            this->getDatabase()->set(id, data);
        this->getDatabase()->save();
    }

    void ShopPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->remove(id);
        this->getDatabase()->save();
    }
    
    void ShopPlugin::executeCommand(Player& player, std::string cmd) {
        if (!this->isValid())
            return;

        ll::string_utils::replaceAll(cmd, "${player}", std::string(player.mName));

        ServerCommandOrigin origin = ServerCommandOrigin(
            "Server", ll::service::getLevel()->asServer(), CommandPermissionLevel::Internal, player.getDimensionId()
        );
        Command* command = ll::service::getMinecraft()->mCommands->compileCommand(
            HashedString(cmd), origin, static_cast<CurrentCmdVersion>(CommandVersion::CurrentVersion()),
            [this](std::string const& message) -> void {
                this->getLogger()->error("Command error: {}", message);
            }
        );
        if (command) {
            CommandOutput output(CommandOutputType::AllOutput);
            command->run(origin, output);
        }
    }

    bool ShopPlugin::checkModifiedData(Player& player, nlohmann::ordered_json data, int number) {
        if (!this->isValid() || !data.contains("scores"))
            return true;

        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if ((it.value().get<int>() * number) > ScoreboardUtils::getScore(player, it.key()))
                return false;
        }

        for (nlohmann::ordered_json::iterator it = data["scores"].begin(); it != data["scores"].end(); ++it)
            ScoreboardUtils::reduceScore(player, it.key(), (it.value().get<int>() * number));
        
        return true;
    }

    bool ShopPlugin::has(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has(id);
    }

    bool ShopPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool ShopPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Shop)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_unique<JsonStorage>(mDataPath / "shop.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool ShopPlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        return true;
    }

    bool ShopPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;
        
        this->registeryCommand();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool ShopPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->save();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("ShopPlugin", LOICollection::Plugins::ShopPlugin, LOICollection::Plugins::ShopPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
