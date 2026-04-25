#include <string>
#include <unordered_map>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/CommandUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/include/server/Plugins/gui/ShopGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void ShopGui::editNewInfo(Player& player, ShopType type) {
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

    void ShopGui::editNew(Player& player) {
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

    void ShopGui::editRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "shop.gui.error"));

            this->edit(player);
            return;
        }

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

    void ShopGui::editRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void ShopGui::editAwardSetting(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "shop.gui.error"));

            this->edit(player);
            return;
        }

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

    void ShopGui::editAwardNewInfo(Player& player, const std::string& id, ShopType type, ShopAwardType awardType) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "shop.gui.error"));

            this->edit(player);
            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "shop.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "shop.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "shop.gui.button3.new.input1"), tr(mObjectLanguage, "shop.gui.button3.new.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "shop.gui.button3.new.input2"), tr(mObjectLanguage, "shop.gui.button3.new.input2.placeholder"));

        switch (awardType) {
            case ShopAwardType::commodity:
                form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), tr(mObjectLanguage, "shop.gui.button3.new.input3.placeholder"));
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), tr(mObjectLanguage, "shop.gui.button3.new.input4.placeholder"));
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), tr(mObjectLanguage, "shop.gui.button3.new.input5.placeholder"));
                form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), tr(mObjectLanguage, "shop.gui.button3.new.input7.placeholder"));
                break;
            case ShopAwardType::title:
                form.appendInput("Input3", tr(mObjectLanguage, "shop.gui.button3.new.input3"), tr(mObjectLanguage, "shop.gui.button3.new.input3.placeholder"));
                form.appendInput("Input6", tr(mObjectLanguage, "shop.gui.button3.new.input6"), tr(mObjectLanguage, "shop.gui.button3.new.input6.placeholder"));
                
                if (type == ShopType::buy)
                    form.appendInput("Input9", tr(mObjectLanguage, "shop.gui.button3.new.input9"), tr(mObjectLanguage, "shop.gui.button3.new.input9.placeholder"));
                
                form.appendInput("Input4", tr(mObjectLanguage, "shop.gui.button3.new.input4"), tr(mObjectLanguage, "shop.gui.button3.new.input4.placeholder"));
                form.appendInput("Input5", tr(mObjectLanguage, "shop.gui.button3.new.input5"), tr(mObjectLanguage, "shop.gui.button3.new.input5.placeholder"));
                form.appendInput("Input7", tr(mObjectLanguage, "shop.gui.button3.new.input7"), tr(mObjectLanguage, "shop.gui.button3.new.input7.placeholder"));
                form.appendInput("Input8", tr(mObjectLanguage, "shop.gui.button3.new.input8"), tr(mObjectLanguage, "shop.gui.button3.new.input8.placeholder"));
                break;
            case ShopAwardType::from:
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
                case ShopAwardType::commodity: {
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
                case ShopAwardType::title: {
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
                case ShopAwardType::from: {
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

    void ShopGui::editAwardNew(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "shop.gui.title"), tr(mObjectLanguage, "shop.gui.label"));
        form.appendButton("Commodity", [this, id, type](Player& pl) -> void {
            this->editAwardNewInfo(pl, id, type, ShopAwardType::commodity);
        });
        form.appendButton("Title", [this, id, type](Player& pl) -> void {
            this->editAwardNewInfo(pl, id, type, ShopAwardType::title);
        });
        form.appendButton("From", [this, id, type](Player& pl) -> void {
            this->editAwardNewInfo(pl, id, type, ShopAwardType::from);
        });
        form.sendTo(player, [this, ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->editAwardContent(pl, ids, type);
        });
    }

    void ShopGui::editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid) {
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

    void ShopGui::editAwardRemove(Player& player, const std::string& id, ShopType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "shop.gui.error"));

            this->edit(player);
            return;
        }

        std::vector<std::string> mNames;
        for (nlohmann::ordered_json& item : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation"))
            mNames.push_back(item.value("title", ""));

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void ShopGui::editAwardContent(Player& player, const std::string& id, ShopType type) {
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

    void ShopGui::editAward(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
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

    void ShopGui::edit(Player& player) {
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

    void ShopGui::menu(Player& player, const std::string& id, ShopType type) {
        auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);

        std::vector<std::pair<std::string, std::string>> mItems;
        std::vector<nlohmann::ordered_json> mItemIds;

        for (auto& item : data.value("classiflcation", nlohmann::ordered_json::array())) {
            mItems.emplace_back(LOICollectionAPI::APIUtils::getInstance().translate(item.value("title", ""), player), item.value("image", ""));
            mItemIds.emplace_back(item);
        }

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("content", ""), player),
            mItems
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, type, id, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
            const nlohmann::ordered_json& item = mItemIds.at(index);

            switch (ll::hash_utils::doHash(item.value("type", ""))) {
                case ll::hash_utils::doHash("commodity"):
                    this->commodity(pl, index, id, type);
                    break;
                case ll::hash_utils::doHash("title"):
                    this->title(pl, index, id, type);
                    break;
                case ll::hash_utils::doHash("from"):
                    this->open(pl, item);
                    break;
                default:
                    this->mParent.getLogger()->error("Unknown UI type {}.", item.value("type", ""));
                    break;
            };
        });
        form->setCloseCallback([data](Player& pl) -> void {
            CommandUtils::executeCommand(pl, data.value("info", nlohmann::ordered_json()).value("exit", ""));
        });

        form->sendPage(player, 1);
    }

    void ShopGui::commodity(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id);
        auto data = original.at("classiflcation").at(index);

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player));
        form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player));
        form.appendInput("Input", LOICollectionAPI::APIUtils::getInstance().translate(data.value("number", ""), player), "", "1");
        form.sendTo(player, [this, original, data, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value("exit", ""));

            int mNumber = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
            if (mNumber > 2304 || mNumber <= 0)
                return;

            if (!this->mParent.commodity(pl, mNumber, data, type))
                return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value(type == ShopType::buy ? "score" : "item", ""));
        });
    }

    void ShopGui::title(Player& player, int index, const std::string& id, ShopType type) {
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
                if (!this->mParent.title(pl, data, type))
                    return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value(type == ShopType::buy ? "score" : "title", ""));
            }
        });
    }

    void ShopGui::open(Player& player, const std::string& id) {
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
}
