#include <string>
#include <vector>
#include <unordered_map>

#include <ll/api/io/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/ChatGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void ChatGui::contentAdd(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "chat.gui.manager.add.input1"), tr(mObjectLanguage, "chat.gui.manager.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "chat.gui.manager.add.input2"), tr(mObjectLanguage, "chat.gui.manager.add.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->add(pl);

            Player* mTarget = ll::service::getLevel()->getPlayer(target);
            if (!mTarget) {
                pl.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

                this->add(pl);
                return;
            }

            std::string mTitle = std::get<std::string>(dt->at("Input1"));

            if (mTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->add(pl);
            }

            int mTime = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            this->mParent.addTitle(*mTarget, mTitle, mTime);
        });
    }
    
    void ChatGui::contentRemove(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.remove.dropdown"), this->mParent.getTitles(target));
        form.sendTo(player, [this, mObjectLanguage, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->remove(pl);

            Player* mTarget = ll::service::getLevel()->getPlayer(target);
            if (!mTarget) {
                pl.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

                this->remove(pl);
                return;
            }

            this->mParent.delTitle(*mTarget, std::get<std::string>(dt->at("dropdown")));
        });
    }

    void ChatGui::add(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::vector<std::string> mPlayers;
        std::vector<mce::UUID> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid());
            return true;
        });

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "chat.gui.title"),
            tr(mObjectLanguage, "chat.gui.setBlacklist.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

                this->open(pl);
                return;
            }

            this->contentAdd(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void ChatGui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayers;
        std::vector<mce::UUID> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid());
            return true;
        });

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "chat.gui.title"),
            tr(mObjectLanguage, "chat.gui.manager.remove.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

                this->open(pl);
                return;
            }

            this->contentRemove(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void ChatGui::title(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
        form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(tr(mObjectLanguage, "chat.gui.setTitle.label"), player));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.setTitle.dropdown"), this->mParent.getTitles(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->setting(pl);

            this->mParent.setTitle(pl, std::get<std::string>(dt->at("dropdown")));
            
            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log1"), pl));
        });
    }

    void ChatGui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(player, target)) {
            player.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

            this->blacklist(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getBlacklistData(target);

        std::string mObjectLabel = tr(mObjectLanguage, "chat.gui.setBlacklist.set.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), 
            fmt::format(fmt::runtime(mObjectLabel),
                mData.at("target"),
                mData.at("name"),
                SystemUtils::toFormatTime(mData.at("time"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist.set.remove"), [this, target](Player& pl) -> void {
            this->mParent.delBlacklist(pl, target);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->blacklist(pl);
        });
    }

    void ChatGui::blacklistAdd(Player& player) {
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
            tr(mObjectLanguage, "chat.gui.title"),
            tr(mObjectLanguage, "chat.gui.setBlacklist.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

                this->blacklist(pl);
                return;
            }

            this->mParent.addBlacklist(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->blacklist(pl);
        });

        form->sendPage(player, 1);
    }

    void ChatGui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "chat.gui.title"),
            tr(mObjectLanguage, "chat.gui.label"),
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
        form->appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist.add"), "", [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = this->mParent.getBlacklistUpload();
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "chat.gui.setBlacklist.tips1")), mBlacklistCount));
                
                this->setting(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void ChatGui::setting(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
        form.appendButton(tr(mObjectLanguage, "chat.gui.setTitle"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
            this->title(pl);
        });
        form.appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist"), "textures/ui/icon_book_writable", "path", [this](Player& pl) -> void {
            this->blacklist(pl);
        });
        form.sendTo(player);
    }

    void ChatGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
        form.appendButton(tr(mObjectLanguage, "chat.gui.manager.add"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
            this->add(pl);
        });
        form.appendButton(tr(mObjectLanguage, "chat.gui.manager.remove"), "textures/ui/free_download_symbol", "path", [this](Player& pl) -> void {
            this->remove(pl);
        });
        form.sendTo(player);
    }
}
