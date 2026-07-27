#include <string>
#include <ranges>
#include <unordered_map>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/NoticePlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/include/server/Plugins/gui/NoticeGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    ll::Expected<void> NoticeGui::setting(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.isClose(player)
                    .transform([this, language, &player](bool exists) -> void {
                        ll::form::CustomForm form(tr(language, "notice.gui.title"));
                        form.appendLabel(tr(language, "notice.gui.label"));
                        form.appendToggle("Toggle1", tr(language, "notice.gui.setting.switch1"), exists);
                        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) return;

                            this->mParent.setClose(pl, std::get<uint64>(dt->at("Toggle1")))
                                .or_else(modules::defaultErrorHandler<NoticePlugin>);
                        });
                    });
            });
    }

    ll::Expected<void> NoticeGui::content(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .transform([this, language, id, &player](bool exists) -> void {
                        if (!exists) {
                            player.sendMessage(tr(language, "notice.gui.error"));
                            
                            return;
                        }

                        ll::form::CustomForm form(tr(language, "notice.gui.title"));
                        form.appendLabel(tr(language, "notice.gui.label"));
                        form.appendInput("Input", tr(language, "notice.gui.edit.title"), "", this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title").value_or(""));

                        std::string mObjectLine = tr(language, "notice.gui.edit.line");

                        auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content").value_or(nlohmann::ordered_json::array());
                        for (const auto& [index, line] : std::views::enumerate(content))
                            form.appendInput("Content" + std::to_string(index), fmt::format(fmt::runtime(mObjectLine), index + 1), "", line);

                        form.appendToggle("Toggle", tr(language, "notice.gui.edit.show"), this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/poiontout").value_or(false));
                        form.appendStepSlider("StepSlider", tr(language, "notice.gui.edit.operation"), { "no", "add", "remove" });
                        form.sendTo(player, [this, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                            if (!dt) {
                                this->edit(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);

                                return;
                            }

                            this->mParent.getDatabase()->set_ptr("/" + id + "/title", std::get<std::string>(dt->at("Input")));
                            this->mParent.getDatabase()->set_ptr("/" + id + "/poiontout", static_cast<bool>(std::get<uint64>(dt->at("Toggle"))));

                            auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content").value_or(nlohmann::ordered_json::array());
                            switch (ll::hash_utils::doHash(std::get<std::string>(dt->at("StepSlider")))) {
                                case ll::hash_utils::doHash("add"): 
                                    content.push_back("");
                                    break;
                                case ll::hash_utils::doHash("remove"): 
                                    content.erase(content.end() - 1);
                                    break;
                                default:
                                    for (auto&& [index, line] : std::views::enumerate(content))
                                        line = std::get<std::string>(dt->at("Content" + std::to_string(index)));
                            }

                            this->mParent.getDatabase()->set_ptr("/" + id + "/content", content);
                           
                            this->mParent.getDatabase()->save()
                                .and_then([this, id, &pl]() -> ll::Expected<void> {
                                    return this->content(pl, id);
                                })
                                .or_else(modules::defaultErrorHandler<NoticePlugin>);

                            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log1"), pl));
                        });
                    });
            });
    }

    ll::Expected<void> NoticeGui::contentAdd(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "notice.gui.title"));
                form.appendLabel(tr(language, "notice.gui.label"));
                form.appendInput("Input1", tr(language, "notice.gui.add.input1"), tr(language, "notice.gui.add.input1.placeholder"));
                form.appendInput("Input2", tr(language, "notice.gui.add.input2"), tr(language, "notice.gui.add.input2.placeholder"));
                form.appendInput("Input3", tr(language, "notice.gui.add.input3"), tr(language, "notice.gui.add.input3.placeholder"));
                form.appendToggle("Toggle1", tr(language, "notice.gui.add.switch"), false);
                form.sendTo(player, [this, language](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
                    if (!dt) {
                        this->edit(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);

                        return;
                    }

                    std::string mObjectId = std::get<std::string>(dt->at("Input1"));
                    std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

                    if (mObjectId.empty() || mObjectTitle.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));
                        
                        this->edit(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);

                        return;
                    }

                    this->mParent.create(mObjectId, mObjectTitle, 
                        SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 0),
                        static_cast<bool>(std::get<uint64>(dt->at("Toggle1")))
                    ).or_else(modules::defaultErrorHandler<NoticePlugin>);

                    this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log2"), pl)), mObjectId);
                });
            });
    }
    
    ll::Expected<void> NoticeGui::contentRemoveInfo(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .and_then([this, language, id, &player](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "notice.gui.error"));
            
                            return this->edit(player);
                        }

                        ll::form::ModalForm form(tr(language, "notice.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "notice.gui.remove.content")), id),
                            tr(language, "notice.gui.remove.yes"), tr(language, "notice.gui.remove.no")
                        );
                        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
                            if (result != ll::form::ModalFormSelectedButton::Upper) {
                                this->edit(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);

                                return;
                            }

                            this->mParent.remove(id).or_else(modules::defaultErrorHandler<NoticePlugin>);

                            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log3"), pl)), id);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> NoticeGui::contentRemove(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "notice.gui.title"),
                    tr(language, "notice.gui.label"),
                    this->mParent.getDatabase()->keys()
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this](Player& pl, const std::string& response) -> void {
                    this->contentRemoveInfo(pl, response).or_else(modules::defaultErrorHandler<NoticePlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->edit(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> NoticeGui::edit(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "notice.gui.title"), tr(language, "notice.gui.label"));
                form.appendButton(tr(language, "notice.gui.add"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
                    this->contentAdd(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);
                });
                form.appendButton(tr(language, "notice.gui.remove"), "textures/ui/book_trash_default", "path", [this](Player& pl) -> void {
                    this->contentRemove(pl).or_else(modules::defaultErrorHandler<NoticePlugin>);
                });
                form.appendDivider();
                for (const std::string& key : this->mParent.getDatabase()->keys()) {
                    form.appendButton(key, [this, key](Player& pl) -> void {
                        this->content(pl, key).or_else(modules::defaultErrorHandler<NoticePlugin>);
                    });
                }
                form.sendTo(player);
            });
    }

    ll::Expected<void> NoticeGui::notice(Player& player) {
        nlohmann::ordered_json data = this->mParent.getDatabase()->get();

        std::vector<std::pair<std::string, int>> mContent;
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (!it.value().value("poiontout", false))
                continue;

            mContent.emplace_back(it.key(), it.value().value("priority", 0));
        }

        std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        for (const auto& pair : mContent) {
            const nlohmann::ordered_json& mObject = data.at(pair.first);
            
            ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(mObject.value("title", ""), player));
            for (const auto& line : mObject.value("content", nlohmann::ordered_json::array()))
                form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(line, player));

            form.sendTo(player);
        }

        return {};
    }

    ll::Expected<void> NoticeGui::notice(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.has(id)
                    .transform([this, language, id, &player](bool exists) -> void {
                        if (!exists) {
                            player.sendMessage(tr(language, "notice.gui.error"));

                            return {};
                        }

                        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title").value_or(""), player));
                        for (const auto& line : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content").value_or(nlohmann::ordered_json::array()))
                            form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(line, player));
                        
                        form.sendTo(player);
                    });
            });
    }

    ll::Expected<void> NoticeGui::open(Player& player) {
        nlohmann::ordered_json data = this->mParent.getDatabase()->get();

        std::vector<std::pair<std::string, int>> mContent;
        for (auto it = data.begin(); it != data.end(); ++it)
            mContent.emplace_back(it.key(), it.value().value("priority", 0));

        std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, &mContent, &data](const std::string& language) -> void {
                std::vector<std::string> mNoticeNames;
                std::vector<std::string> mNoticeIds;

                for (const auto& pair : mContent) {
                    mNoticeNames.push_back(LOICollectionAPI::APIUtils::getInstance().translate(data.at(pair.first).value("title", ""), player));
                    mNoticeIds.push_back(pair.first);
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "notice.gui.title"),
                    tr(language, "notice.gui.label"),
                    mNoticeNames
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, mNoticeIds = std::move(mNoticeIds)](Player& pl, int index) -> void {
                    this->notice(pl, mNoticeIds.at(index)).or_else(modules::defaultErrorHandler<NoticePlugin>);
                });

                form->sendPage(player, 1);
            });
    }
}
