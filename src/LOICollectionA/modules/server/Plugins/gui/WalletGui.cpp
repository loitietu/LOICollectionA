#include <string>

#include <fmt/format.h>

#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/WalletPlugin.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/server/Plugins/gui/WalletGui.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    void WalletGui::content(Player& player, const std::string& target, WalletTransferType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mTargetName = type == WalletTransferType::online ? 
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(target))->getRealName() : this->mParent.getPlayerInfo(target);

        std::string mLabel = tr(mObjectLanguage, "wallet.gui.label") + "\n" + tr(mObjectLanguage, "wallet.gui.transfer.label2");

        ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
        form.appendLabel(fmt::format(fmt::runtime(mLabel), 
            ScoreboardUtils::getScore(player, this->mParent.getTargetScoreboard()),
            std::to_string(this->mParent.getExchangeRate() * 100) + "%%",
            mTargetName
        ));
        form.appendInput("Input", tr(mObjectLanguage, "wallet.gui.transfer.input"), tr(mObjectLanguage, "wallet.gui.transfer.input.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, target, mTargetName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->transfer(pl, type);

            int mMoney = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
            if (!this->mParent.forTransfer(pl, target, mTargetName, mMoney)) {
                pl.sendMessage(tr(mObjectLanguage, "wallet.tips.transfer"));

                this->transfer(pl, type);
                return;
            }
        });
    }

    void WalletGui::transfer(Player& player, WalletTransferType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayerNames;
        std::vector<std::string> mPlayerUuids;

        switch (type) {
            case WalletTransferType::online: {
                ll::service::getLevel()->forEachPlayer([&mPlayerNames, &mPlayerUuids](Player& mTarget) -> bool {
                    if (mTarget.isSimulatedPlayer())
                        return true;

                    mPlayerNames.push_back(mTarget.getRealName());
                    mPlayerUuids.push_back(mTarget.getUuid().asString());
                    return true;
                });

                break;
            }
            case WalletTransferType::offline: {
                for (auto& mTarget : this->mParent.getPlayerInfo()) {
                    mPlayerNames.push_back(mTarget.second);
                    mPlayerUuids.push_back(mTarget.first);
                }

                break;
            }
        }

        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
            tr(mObjectLanguage, "wallet.gui.title"),
            tr(mObjectLanguage, "wallet.gui.transfer.label1"),
            mPlayerNames
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, type, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            this->content(pl, mPlayerUuids.at(index), type);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void WalletGui::redenvelope(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
        form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "wallet.gui.label")),
            ScoreboardUtils::getScore(player, this->mParent.getTargetScoreboard()), std::to_string(this->mParent.getExchangeRate() * 100) + "%%"
        ));
        form.appendInput("Input1", tr(mObjectLanguage, "wallet.gui.redenvelope.input1"), tr(mObjectLanguage, "wallet.gui.redenvelope.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "wallet.gui.redenvelope.input2"), tr(mObjectLanguage, "wallet.gui.redenvelope.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "wallet.gui.redenvelope.input3"), tr(mObjectLanguage, "wallet.gui.redenvelope.input3.placeholder"));
        form.sendTo(player, [this, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);

            std::string mObjectKey = std::get<std::string>(dt->at("Input3"));

            if (mObjectKey.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));

                this->open(pl);
                return;
            }

            int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input1")), 0);
            int mCount = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            std::string mScoreboard = this->mParent.getTargetScoreboard();
            if (mScore <= 0 || mCount <= 0 || ScoreboardUtils::getScore(pl, mScoreboard) < mScore * mCount) {
                pl.sendMessage(tr(mObjectLanguage, "wallet.tips.redenvelope"));

                this->open(pl);
                return;
            }

            this->mParent.redenvelope(pl, mObjectKey, mScore, mCount);
        });
    }

    void WalletGui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), 
            fmt::format(fmt::runtime(mLabel), 
                ScoreboardUtils::getScore(player, this->mParent.getTargetScoreboard()),
                std::to_string(this->mParent.getExchangeRate() * 100) + "%%"
            )
        );
        form.appendButton(tr(mObjectLanguage, "wallet.gui.transfer"), "textures/ui/MCoin", "path", [this](Player& pl) -> void {
            this->transfer(pl, WalletTransferType::online);
        });
        form.appendButton(tr(mObjectLanguage, "wallet.gui.offlineTransfer"), "textures/ui/icon_best3", "path", [this](Player& pl) -> void {
            this->transfer(pl, WalletTransferType::offline);
        });
        form.appendButton(tr(mObjectLanguage, "wallet.gui.redenvelope"), "textures/ui/comment", "path", [this](Player& pl) -> void {
            this->redenvelope(pl);
        });
        form.appendButton(tr(mObjectLanguage, "wallet.gui.wealth"), "textures/ui/creative_icon", "path", [this](Player& pl) -> void {
            this->mParent.wealth(pl);
        });
        form.sendTo(player);
    }
}
