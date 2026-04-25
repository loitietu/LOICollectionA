#include <string>
#include <unordered_map>

#include <ll/api/form/ModalForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/TpaPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/TpaGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void TpaGui::generic(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.setting.title"));
        form.appendLabel(tr(mObjectLanguage, "tpa.gui.setting.label"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "tpa.gui.setting.generic.switch1"), this->mParent.isInvite(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->setting(pl);

            this->mParent.setInvite(pl, std::get<uint64>(dt->at("Toggle1")));
        });
    };

    void TpaGui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(player, target)) {
            player.sendMessage(tr(mObjectLanguage, "tpa.gui.error"));

            this->setting(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getBlacklistData(target);

        std::string mObjectLabel = tr(mObjectLanguage, "tpa.gui.setting.blacklist.set.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), 
            fmt::format(fmt::runtime(mObjectLabel),
                mData.at("target"),
                mData.at("name"),
                SystemUtils::toFormatTime(mData.at("time"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist.set.remove"), [this, target](Player& pl) -> void {
            this->mParent.delBlacklist(pl, target);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->blacklist(pl);
        });
    }

    void TpaGui::blacklistAdd(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

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
            tr(mObjectLanguage, "tpa.gui.setting.title"),
            tr(mObjectLanguage, "tpa.gui.setting.blacklist.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "tpa.gui.error"));

                this->setting(pl);
                return;
            }

            this->mParent.addBlacklist(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->setting(pl);
        });

        form->sendPage(player, 1);
    }

    void TpaGui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "tpa.gui.setting.title"),
            tr(mObjectLanguage, "tpa.gui.setting.label"),
            this->mParent.getBlacklist(player)
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->blacklistSet(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->setting(pl);
        });
        form->appendDivider();
        form->appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist.add"), "textures/ui/editIcon", [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = this->mParent.getBlacklistUpload();
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "tpa.tips2")), mBlacklistCount));

                this->setting(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void TpaGui::setting(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.label"));
        form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.generic"), "textures/ui/icon_setting", "path", [this](Player& pl) -> void {
            this->generic(pl);
        });
        form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist"), "textures/ui/icon_lock", "path", [this](Player& pl) -> void {
            this->blacklist(pl);
        });
        form.sendTo(player);
    }

    void TpaGui::tpa(Player& player, Player& target, TpaType type) {
        int mRequestUpload = this->mParent.getRequestUpload();
        if (this->mParent.getRequestCount(player) >= mRequestUpload) {
            player.sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(player), "tpa.tips5")), mRequestUpload));
            
            return;
        }

        std::string mId = SystemUtils::getCurrentTimestamp();
        this->mParent.sendRequest(player, target, mId, type);

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(target);

        ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"),
            LOICollectionAPI::APIUtils::getInstance().translate(tr(mObjectLanguage, (type == TpaType::tpa) ? "tpa.there" : "tpa.here"), player),
            tr(mObjectLanguage, "tpa.yes"),
            tr(mObjectLanguage, "tpa.no")
        );
        form.sendTo(target, [this, mObjectLanguage, mId](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                if (!this->mParent.acceptRequest(pl, mId)) 
                    pl.sendMessage(tr(mObjectLanguage, "tpa.gui.error"));

                return;
            }
            
            this->mParent.rejectRequest(pl, mId);
        });
    }

    void TpaGui::content(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "tpa.gui.label"));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "tpa.gui.dropdown"), { "tpa", "tphere" });
        form.sendTo(player, [this, mObjectLanguage, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);

            Player* mTarget = ll::service::getLevel()->getPlayer(target);
            if (!mTarget) {
                pl.sendMessage(tr(mObjectLanguage, "tpa.gui.error"));

                this->open(pl);
                return;
            }

            if (!this->mParent.forTpaContent(pl)) {
                pl.sendMessage(tr(mObjectLanguage, "tpa.tips1"));

                return;
            }

            this->tpa(pl, *mTarget, 
                std::get<std::string>(dt->at("dropdown")) == "tpa" ? TpaType::tpa : TpaType::tphere
            );
        });
    }

    void TpaGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayers;
        std::vector<mce::UUID> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([this, &player, &mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            std::vector<std::string> mList = this->mParent.getBlacklist(mTarget);
            if (mTarget.isSimulatedPlayer() || std::find(mList.begin(), mList.end(), player.getUuid().asString()) != mList.end() || this->mParent.isInvite(mTarget) || mTarget.getUuid() == player.getUuid())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid());
            return true;
        });

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "tpa.gui.title"),
            tr(mObjectLanguage, "tpa.gui.label2"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "tpa.gui.error"));

                return;
            }

            this->content(pl, *mPlayer);
        });

        form->sendPage(player, 1);
    }
}
