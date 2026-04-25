#include <string>
#include <vector>
#include <unordered_map>

#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/BlacklistPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/BlacklistGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void BlacklistGui::info(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(id)) {
            player.sendMessage(tr(mObjectLanguage, "blacklist.gui.error"));
            
            this->remove(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getBlacklistData(id);

        std::string mObjectLabel = tr(mObjectLanguage, "blacklist.gui.info.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), 
            fmt::format(fmt::runtime(mObjectLabel), id, 
                mData.at("name"),
                mData.at("cause"),
                SystemUtils::toFormatTime(mData.at("subtime"), "None"),
                SystemUtils::toFormatTime(mData.at("time"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "blacklist.gui.info.remove"), [this, id](Player&) -> void {
            this->mParent.delBlacklist(id);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->remove(pl);
        });
    }

    void BlacklistGui::content(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.add.title"));
        form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "blacklist.gui.add.input1"), tr(mObjectLanguage, "blacklist.gui.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "blacklist.gui.add.input2"), tr(mObjectLanguage, "blacklist.gui.add.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->add(pl);

            Player* mTarget = ll::service::getLevel()->getPlayer(target);
            if (!mTarget) {
                pl.sendMessage(tr(mObjectLanguage, "blacklist.gui.error"));

                this->add(pl);
                return;
            }

            std::string mCause = std::get<std::string>(dt->at("Input1"));
            
            if (mCause.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->add(pl);
            }

            int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
            
            this->mParent.addBlacklist(*mTarget, mCause, time);
        });
    }

    void BlacklistGui::add(Player& player) {
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
            tr(mObjectLanguage, "blacklist.gui.add.title"),
            tr(mObjectLanguage, "blacklist.gui.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "blacklist.gui.error"));

                this->open(pl);
                return;
            }

            this->content(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void BlacklistGui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "blacklist.gui.remove.title"),
            tr(mObjectLanguage, "blacklist.gui.remove.label"),
            this->mParent.getBlacklists()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->info(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void BlacklistGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.title"), tr(mObjectLanguage, "blacklist.gui.label"));
        form.appendButton(tr(mObjectLanguage, "blacklist.gui.addBlacklist"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
            this->add(pl);
        });
        form.appendButton(tr(mObjectLanguage, "blacklist.gui.removeBlacklist"), "textures/ui/free_download_symbol", "path", [this](Player& pl) -> void {
            this->remove(pl);
        });
        form.sendTo(player);
    }
}
