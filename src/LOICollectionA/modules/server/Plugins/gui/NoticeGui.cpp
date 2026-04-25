#include <string>
#include <ranges>
#include <unordered_map>

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
    void NoticeGui::setting(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.setting.switch1"), this->mParent.isClose(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return;

            this->mParent.setClose(pl, std::get<uint64>(dt->at("Toggle1")));
        });
    }

    void NoticeGui::content(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "notice.gui.error"));
            
            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendInput("Input", tr(mObjectLanguage, "notice.gui.edit.title"), "", this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""));

        std::string mObjectLine = tr(mObjectLanguage, "notice.gui.edit.line");

        auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
        for (const auto& [index, line] : std::views::enumerate(content))
            form.appendInput("Content" + std::to_string(index), fmt::format(fmt::runtime(mObjectLine), index + 1), "", line);

        form.appendToggle("Toggle", tr(mObjectLanguage, "notice.gui.edit.show"), this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/poiontout", false));
        form.appendStepSlider("StepSlider", tr(mObjectLanguage, "notice.gui.edit.operation"), { "no", "add", "remove" });
        form.sendTo(player, [this, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->edit(pl);

            this->mParent.getDatabase()->set_ptr("/" + id + "/title", std::get<std::string>(dt->at("Input")));
            this->mParent.getDatabase()->set_ptr("/" + id + "/poiontout", static_cast<bool>(std::get<uint64>(dt->at("Toggle"))));

            auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
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
            this->mParent.getDatabase()->save();

            this->content(pl, id);

            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log1"), pl));
        });
    }

    void NoticeGui::contentAdd(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "notice.gui.add.input1"), tr(mObjectLanguage, "notice.gui.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "notice.gui.add.input2"), tr(mObjectLanguage, "notice.gui.add.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "notice.gui.add.input3"), tr(mObjectLanguage, "notice.gui.add.input3.placeholder"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.add.switch"), false);
        form.sendTo(player, [this, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->edit(pl);

            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

            if (mObjectId.empty() || mObjectTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->edit(pl);
            }

            this->mParent.create(mObjectId, mObjectTitle, 
                SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 0), static_cast<bool>(std::get<uint64>(dt->at("Toggle1")))
            );

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log2"), pl)), mObjectId);
        });
    }
    
    void NoticeGui::contentRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "notice.gui.error"));
            
            this->edit(player);
            return;
        }

        ll::form::ModalForm form(tr(mObjectLanguage, "notice.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "notice.gui.remove.content")), id),
            tr(mObjectLanguage, "notice.gui.remove.yes"), tr(mObjectLanguage, "notice.gui.remove.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper) 
                return this->edit(pl);

            this->mParent.remove(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log3"), pl)), id);
        });
    }

    void NoticeGui::contentRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "notice.gui.title"),
            tr(mObjectLanguage, "notice.gui.label"),
            this->mParent.getDatabase()->keys()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->contentRemoveInfo(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->edit(pl);
        });

        form->sendPage(player, 1);
    }

    void NoticeGui::edit(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
        form.appendButton(tr(mObjectLanguage, "notice.gui.add"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
            this->contentAdd(pl);
        });
        form.appendButton(tr(mObjectLanguage, "notice.gui.remove"), "textures/ui/book_trash_default", "path", [this](Player& pl) -> void {
            this->contentRemove(pl);
        });
        form.appendDivider();
        for (const std::string& key : this->mParent.getDatabase()->keys()) {
            form.appendButton(key, [this, key](Player& pl) {
                this->content(pl, key);
            });
        }
        form.sendTo(player);
    }

    void NoticeGui::notice(Player& player) {
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
    }

    void NoticeGui::notice(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "notice.gui.error"));
            
            return;
        }

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""), player));
        for (const auto& line : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content"))
            form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(line, player));
        
        form.sendTo(player);
    }

    void NoticeGui::open(Player& player) {
        nlohmann::ordered_json data = this->mParent.getDatabase()->get();

        std::vector<std::pair<std::string, int>> mContent;
        for (auto it = data.begin(); it != data.end(); ++it)
            mContent.emplace_back(it.key(), it.value().value("priority", 0));

        std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mNoticeNames;
        std::vector<std::string> mNoticeIds;

        for (const auto& pair : mContent) {
            mNoticeNames.push_back(LOICollectionAPI::APIUtils::getInstance().translate(data.at(pair.first).value("title", ""), player));
            mNoticeIds.push_back(pair.first);
        }

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "notice.gui.title"),
            tr(mObjectLanguage, "notice.gui.label"),
            mNoticeNames
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mNoticeIds = std::move(mNoticeIds)](Player& pl, int index) -> void {
            this->notice(pl, mNoticeIds.at(index));
        });

        form->sendPage(player, 1);
    }
}
