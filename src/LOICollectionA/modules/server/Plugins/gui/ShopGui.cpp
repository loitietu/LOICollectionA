#include <string>
#include <unordered_map>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/Expected.h>
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
    ll::Expected<void> ShopGui::editNewInfo(Player& player, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, type, &player](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "shop.gui.title"));
                form.appendLabel(tr(language, "shop.gui.label"));
                form.appendInput("Input1", tr(language, "shop.gui.button1.input1"), tr(language, "shop.gui.button1.input1.placeholder"));
                form.appendInput("Input2", tr(language, "shop.gui.button1.input2"), tr(language, "shop.gui.button1.input2.placeholder"));
                form.appendInput("Input3", tr(language, "shop.gui.button1.input3"), tr(language, "shop.gui.button1.input3.placeholder"));

                switch (type) {
                    case ShopType::buy:
                        form.appendInput("Input4", tr(language, "shop.gui.button1.input4"), tr(language, "shop.gui.button1.input4.placeholder"));
                        form.appendInput("Input5", tr(language, "shop.gui.button1.input5"), tr(language, "shop.gui.button1.input5.placeholder"));
                        break;
                    case ShopType::sell:
                        form.appendInput("Input4", tr(language, "shop.gui.button1.input4"), tr(language, "shop.gui.button1.input4.placeholder"));
                        form.appendInput("Input6", tr(language, "shop.gui.button1.input6"), tr(language, "shop.gui.button1.input6.placeholder"));
                        form.appendInput("Input7", tr(language, "shop.gui.button1.input7"), tr(language, "shop.gui.button1.input7.placeholder"));
                        break;
                };

                form.sendTo(player, [this, language, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                    if (!dt) {
                        this->editNew(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);

                        return;
                    }
                    
                    std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                    std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
                    std::string mObjectContent = std::get<std::string>(dt->at("Input3"));

                    if (mObjectId.empty() || mObjectTitle.empty() || mObjectContent.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));
                        
                        this->editNew(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);

                        return;
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

                    this->mParent.create(mObjectId, data).or_else(modules::defaultErrorHandler<ShopPlugin>);

                    this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log1"), pl)), mObjectId);
                });
            });
    }

    ll::Expected<void> ShopGui::editNew(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "shop.gui.title"), tr(language, "shop.gui.label"));
                form.appendButton("Buy", [this](Player& pl) {
                    this->editNewInfo(pl, ShopType::buy).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.appendButton("Sell", [this](Player& pl) {
                    this->editNewInfo(pl, ShopType::sell).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                    if (id == -1)
                        this->edit(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
            });
    }

    ll::Expected<void> ShopGui::editRemoveInfo(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "shop.gui.error"));

                            return this->edit(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .transform([this, language, id, &player](bool exists) -> void {
                        if (!exists)
                            return;

                        ll::form::ModalForm form(tr(language, "shop.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "shop.gui.button2.content")), id),
                            tr(language, "shop.gui.button2.yes"), tr(language, "shop.gui.button2.no")
                        );
                        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
                            if (result != ll::form::ModalFormSelectedButton::Upper)
                                return;

                            this->mParent.remove(id).or_else(modules::defaultErrorHandler<ShopPlugin>);

                            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log2"), pl)), id);
                        });
                    });
            });
    }

    ll::Expected<void> ShopGui::editRemove(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "shop.gui.title"),
                    tr(language, "shop.gui.label"),
                    this->mParent.getDatabase()->keys()
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this](Player& pl, const std::string& response) -> void {
                    this->editRemoveInfo(pl, response).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->edit(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ShopGui::editAwardSetting(Player& player, const std::string& id, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, type, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "shop.gui.error"));

                            return this->edit(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .transform([this, language, id, type, &player](bool exists) -> void {
                        if (!exists)
                            return;

                        ll::form::CustomForm form(tr(language, "shop.gui.title"));
                        form.appendLabel(tr(language, "shop.gui.label"));
                        form.appendInput("Input2", tr(language, "shop.gui.button3.input2"), tr(language, "shop.gui.button3.input2.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title").value_or(""));
                        form.appendInput("Input3", tr(language, "shop.gui.button3.input3"), tr(language, "shop.gui.button3.input3.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/content").value_or(""));
                        form.appendInput("Input4", tr(language, "shop.gui.button3.input4"), tr(language, "shop.gui.button3.input4.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/exit").value_or(""));

                        switch (type) {
                            case ShopType::buy:
                                form.appendInput("Input5", tr(language, "shop.gui.button3.input5"), tr(language, "shop.gui.button3.input5.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/score").value_or(""));
                                break;
                            case ShopType::sell:
                                form.appendInput("Input6", tr(language, "shop.gui.button3.input6"), tr(language, "shop.gui.button3.input6.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/title").value_or(""));
                                form.appendInput("Input7", tr(language, "shop.gui.button3.input7"), tr(language, "shop.gui.button3.input7.placeholder"), this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/info/item").value_or(""));
                                break;
                        };

                        form.sendTo(player, [this, language, id, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->editAwardContent(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);

                                return;
                            }

                            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));
                            std::string mObjectContent = std::get<std::string>(dt->at("Input3"));

                            if (mObjectTitle.empty() || mObjectContent.empty()) {
                                pl.sendMessage(tr(language, "generic.tips.noinput"));
                                
                                this->editAwardSetting(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);

                                return;
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

                            this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<ShopPlugin>);

                            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log4"), pl)), id);
                        });
                    });
            });
    }

    ll::Expected<void> ShopGui::editAwardNewInfo(Player& player, const std::string& id, ShopType type, ShopAwardType awardType) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, type, awardType, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "shop.gui.error"));

                            return this->edit(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .transform([this, language, id, type, awardType, &player](bool exists) -> void {
                        if (!exists)
                            return;

                        ll::form::CustomForm form(tr(language, "shop.gui.title"));
                        form.appendLabel(tr(language, "shop.gui.label"));
                        form.appendInput("Input1", tr(language, "shop.gui.button3.new.input1"), tr(language, "shop.gui.button3.new.input1.placeholder"));
                        form.appendInput("Input2", tr(language, "shop.gui.button3.new.input2"), tr(language, "shop.gui.button3.new.input2.placeholder"));

                        switch (awardType) {
                            case ShopAwardType::commodity:
                                form.appendInput("Input3", tr(language, "shop.gui.button3.new.input3"), tr(language, "shop.gui.button3.new.input3.placeholder"));
                                form.appendInput("Input6", tr(language, "shop.gui.button3.new.input6"), tr(language, "shop.gui.button3.new.input6.placeholder"));
                                form.appendInput("Input4", tr(language, "shop.gui.button3.new.input4"), tr(language, "shop.gui.button3.new.input4.placeholder"));
                                form.appendInput("Input5", tr(language, "shop.gui.button3.new.input5"), tr(language, "shop.gui.button3.new.input5.placeholder"));
                                form.appendInput("Input7", tr(language, "shop.gui.button3.new.input7"), tr(language, "shop.gui.button3.new.input7.placeholder"));
                                break;
                            case ShopAwardType::title:
                                form.appendInput("Input3", tr(language, "shop.gui.button3.new.input3"), tr(language, "shop.gui.button3.new.input3.placeholder"));
                                form.appendInput("Input6", tr(language, "shop.gui.button3.new.input6"), tr(language, "shop.gui.button3.new.input6.placeholder"));
                                
                                if (type == ShopType::buy)
                                    form.appendInput("Input9", tr(language, "shop.gui.button3.new.input9"), tr(language, "shop.gui.button3.new.input9.placeholder"));
                                
                                form.appendInput("Input4", tr(language, "shop.gui.button3.new.input4"), tr(language, "shop.gui.button3.new.input4.placeholder"));
                                form.appendInput("Input5", tr(language, "shop.gui.button3.new.input5"), tr(language, "shop.gui.button3.new.input5.placeholder"));
                                form.appendInput("Input7", tr(language, "shop.gui.button3.new.input7"), tr(language, "shop.gui.button3.new.input7.placeholder"));
                                form.appendInput("Input8", tr(language, "shop.gui.button3.new.input8"), tr(language, "shop.gui.button3.new.input8.placeholder"));
                                break;
                            case ShopAwardType::from:
                                form.appendInput("Input6", tr(language, "shop.gui.button3.new.input6"), tr(language, "shop.gui.button3.new.input6.placeholder"));
                                break;
                        };

                        form.sendTo(player, [this, language, id, type, awardType](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->editAwardContent(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);

                                return;
                            }

                            std::string mObjectTitle = std::get<std::string>(dt->at("Input1"));
                            std::string mObjectImage = std::get<std::string>(dt->at("Input2"));

                            if (mObjectTitle.empty() || mObjectImage.empty()) {
                                pl.sendMessage(tr(language, "generic.tips.noinput"));
                                
                                this->editAwardContent(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);

                                return;
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

                            int mIndex = static_cast<int>(this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").value_or(nlohmann::ordered_json::array()).size());

                            this->mParent.getDatabase()->set_ptr("/" + id + "/classiflcation/" + std::to_string(mIndex), data);
                            this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<ShopPlugin>);

                            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log5"), pl)), id);
                        });
                    });
            });
    }

    ll::Expected<void> ShopGui::editAwardNew(Player& player, const std::string& id, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, id, type, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "shop.gui.title"), tr(language, "shop.gui.label"));
                form.appendButton("Commodity", [this, id, type](Player& pl) -> void {
                    this->editAwardNewInfo(pl, id, type, ShopAwardType::commodity).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.appendButton("Title", [this, id, type](Player& pl) -> void {
                    this->editAwardNewInfo(pl, id, type, ShopAwardType::title).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.appendButton("From", [this, id, type](Player& pl) -> void {
                    this->editAwardNewInfo(pl, id, type, ShopAwardType::from).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.sendTo(player, [this, ids = id, type](Player& pl, int id, ll::form::FormCancelReason) -> void {
                    if (id == -1)
                        this->editAwardContent(pl, ids, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
            });
    }

    ll::Expected<void> ShopGui::editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, packageid, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "shop.gui.error"));

                            return this->edit(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .transform([this, language, id, packageid, &player](bool exists) -> void {
                        if (!exists)
                            return;

                        ll::form::ModalForm form(tr(language, "menu.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "shop.gui.button3.remove.content")), packageid),
                            tr(language, "shop.gui.button3.remove.yes"), tr(language, "shop.gui.button3.remove.no")
                        );
                        form.sendTo(player, [this, id, packageid](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
                            if (result != ll::form::ModalFormSelectedButton::Upper) {
                                this->edit(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);

                                return;
                            }

                            auto mContent = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").value_or(nlohmann::ordered_json::array());
                            for (int i = static_cast<int>(mContent.size() - 1); i >= 0; i--) {
                                if (mContent.at(i).value("title", "") == packageid)
                                    mContent.erase(i);
                            }

                            this->mParent.getDatabase()->set_ptr("/" + id + "/classiflcation", mContent);
                            this->mParent.getDatabase()->save().or_else(modules::defaultErrorHandler<ShopPlugin>);

                            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "shop.log3"), pl)), id, packageid);
                        });
                    });
            });
    }

    ll::Expected<void> ShopGui::editAwardRemove(Player& player, const std::string& id, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, type, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "shop.gui.error"));

                            return this->edit(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .transform([this, language, id, type, &player](bool exists) -> void {
                        if (!exists)
                            return;

                        std::vector<std::string> mNames;
                        for (nlohmann::ordered_json& item : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/classiflcation").value_or(nlohmann::ordered_json::array()))
                            mNames.push_back(item.value("title", ""));

                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "shop.gui.title"),
                            tr(language, "shop.gui.label"),
                            mNames
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this, id](Player& pl, const std::string& response) -> void {
                            this->editAwardRemoveInfo(pl, id, response).or_else(modules::defaultErrorHandler<ShopPlugin>);
                        });
                        form->setCloseCallback([this, id, type](Player& pl) -> void {
                            this->editAwardContent(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> ShopGui::editAwardContent(Player& player, const std::string& id, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, type, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, &player](bool exists) -> ll::Expected<bool> {
                        if (!exists) {
                            player.sendMessage(tr(language, "shop.gui.error"));

                            return this->edit(player)
                                .transform([]() -> bool {
                                    return false;
                                });
                        }

                        return true;
                    })
                    .transform([this, language, id, type, &player](bool exists) -> void {
                        if (!exists)
                            return;

                        ll::form::SimpleForm form(tr(language, "shop.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "shop.gui.button3.label")), id)
                        );
                        form.appendButton(tr(language, "shop.gui.button3.setting"), "textures/ui/icon_setting", "path", [this, id, type](Player& pl) -> void {
                            this->editAwardSetting(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                        });

                        form.appendButton(tr(language, "shop.gui.button3.new"), "textures/ui/icon_sign", "path", [this, id, type](Player& pl) -> void {
                            this->editAwardNew(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                        });
                        form.appendButton(tr(language, "shop.gui.button3.remove"), "textures/ui/icon_trash", "path", [this, id, type](Player& pl) -> void {
                            this->editAwardRemove(pl, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->editAward(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                        });
                    });
            });
    }

    ll::Expected<void> ShopGui::editAward(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "shop.gui.title"),
                    tr(language, "shop.gui.label"),
                    this->mParent.getDatabase()->keys()
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this](Player& pl, const std::string& response) -> void {
                    auto mObjectType = this->mParent.getDatabase()->get_ptr<std::string>("/" + response + "/type").value_or("buy");

                    switch (ll::hash_utils::doHash(mObjectType)) {
                        case ll::hash_utils::doHash("buy"):
                            this->editAwardContent(pl, response, ShopType::buy).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        case ll::hash_utils::doHash("sell"):
                            this->editAwardContent(pl, response, ShopType::sell).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        default:
                            modules::defaultErrorHandler<ShopPlugin>(ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::UnknownType)));
                            break;
                    }
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->edit(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ShopGui::edit(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "shop.gui.title"), tr(language, "shop.gui.label"));
                form.appendButton(tr(language, "shop.gui.button1"), "textures/ui/achievements", "path", [this](Player& pl) -> void {
                    this->editNew(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.appendButton(tr(language, "shop.gui.button2"), "textures/ui/world_glyph_color", "path", [this](Player& pl) -> void {
                    this->editRemove(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.appendButton(tr(language, "shop.gui.button3"), "textures/ui/editIcon", "path", [this](Player& pl) -> void {
                    this->editAward(pl).or_else(modules::defaultErrorHandler<ShopPlugin>);
                });
                form.sendTo(player);
            });
    }

    ll::Expected<void> ShopGui::menu(Player& player, const std::string& id, ShopType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this,id, type, &player](const std::string& language) -> void {
                auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

                std::vector<std::pair<std::string, std::string>> mItems;
                std::vector<nlohmann::ordered_json> mItemIds;

                for (auto& item : data.value("classiflcation", nlohmann::ordered_json::array())) {
                    mItems.emplace_back(LOICollectionAPI::APIUtils::getInstance().translate(item.value("title", ""), player), item.value("image", ""));
                    mItemIds.emplace_back(item);
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
                    LOICollectionAPI::APIUtils::getInstance().translate(data.value("content", ""), player),
                    mItems
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, type, id, mItemIds = std::move(mItemIds)](Player& pl, int index) -> void {
                    const nlohmann::ordered_json& item = mItemIds.at(index);

                    switch (ll::hash_utils::doHash(item.value("type", ""))) {
                        case ll::hash_utils::doHash("commodity"):
                            this->commodity(pl, index, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        case ll::hash_utils::doHash("title"):
                            this->title(pl, index, id, type).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        case ll::hash_utils::doHash("from"):
                            this->open(pl, item).or_else(modules::defaultErrorHandler<ShopPlugin>);
                            break;
                        default:
                            modules::defaultErrorHandler<ShopPlugin>(ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::UnknownType)));
                            break;
                    };
                });
                form->setCloseCallback([data](Player& pl) -> void {
                    CommandUtils::executeCommand(pl, data.value("info", nlohmann::ordered_json()).value("exit", ""));
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ShopGui::commodity(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});
        auto data = original.at("classiflcation").at(index);

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player));
        form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player));
        form.appendInput("Input", LOICollectionAPI::APIUtils::getInstance().translate(data.value("number", ""), player), "", "1");
        form.sendTo(player, [this, original = std::move(original), data = std::move(data), type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value("exit", ""));

            int mNumber = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
            if (mNumber > 2304 || mNumber <= 0)
                return;

            if (!this->mParent.commodity(pl, mNumber, data, type))
                return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value(type == ShopType::buy ? "score" : "item", ""));
        });

        return {};
    }

    ll::Expected<void> ShopGui::title(Player& player, int index, const std::string& id, ShopType type) {
        auto original = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});
        auto data = original.at("classiflcation").at(index);

        ll::form::ModalForm form(
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("title", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("introduce", ""), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("confirmButton", "confirm"), player),
            LOICollectionAPI::APIUtils::getInstance().translate(data.value("cancelButton", "cancel"), player)
        );
        form.sendTo(player, [this, original = std::move(original), data = std::move(data), type](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                if (!this->mParent.title(pl, data, type))
                    return CommandUtils::executeCommand(pl, original.value("info", nlohmann::ordered_json()).value(type == ShopType::buy ? "score" : "title", ""));
            }
        });

        return {};
    }

    ll::Expected<void> ShopGui::open(Player& player, const std::string& id) {
        if (this->mParent.getDatabase()->has(id)) {
            auto data = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id).value_or(nlohmann::ordered_json{});

            if (data.empty())
                return {};
            
            switch (ll::hash_utils::doHash(data.value("type", ""))) {
                case ll::hash_utils::doHash("buy"): return this->menu(player, id, ShopType::buy);
                case ll::hash_utils::doHash("sell"): return this->menu(player, id, ShopType::sell);
            };

            return ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::UnknownType));
        }

        return ll::makeErrorCodeError(ShopPlugin::makeErrorCode(ShopPluginErrorCode::NotFound));
    }
}
