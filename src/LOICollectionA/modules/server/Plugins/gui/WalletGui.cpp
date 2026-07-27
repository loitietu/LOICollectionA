#include <string>

#include <fmt/format.h>

#include <ll/api/Expected.h>
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
    ll::Expected<void> WalletGui::content(Player& player, const std::string& target, WalletTransferType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, target, type, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getPlayerInfo(target)
                    .transform([this, language, target, type, &player](const std::string& name) -> void {
                        std::string mTargetName = type == WalletTransferType::online ? 
                            ll::service::getLevel()->getPlayer(mce::UUID::fromString(target))->getRealName() : name;

                        std::string mLabel = tr(language, "wallet.gui.label") + "\n" + tr(language, "wallet.gui.transfer.label2");

                        ll::form::CustomForm form(tr(language, "wallet.gui.title"));
                        form.appendLabel(fmt::format(fmt::runtime(mLabel), 
                            ScoreboardUtils::getScore(player, this->mParent.getTargetScoreboard()),
                            std::to_string(this->mParent.getExchangeRate() * 100) + "%%",
                            mTargetName
                        ));
                        form.appendInput("Input", tr(language, "wallet.gui.transfer.input"), tr(language, "wallet.gui.transfer.input.placeholder"));
                        form.sendTo(player, [this, language, target, mTargetName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                            if (!dt) {
                                this->transfer(pl, type).or_else(modules::defaultErrorHandler<WalletPlugin>);

                                return;
                            }

                            int mMoney = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                            if (!this->mParent.forTransfer(pl, target, mTargetName, mMoney)) {
                                pl.sendMessage(tr(language, "wallet.tips.transfer"));

                                this->transfer(pl, type).or_else(modules::defaultErrorHandler<WalletPlugin>);
                            }
                        });
                    });
            });
    }

    ll::Expected<void> WalletGui::transfer(Player& player, WalletTransferType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, type, &player](const std::string& language) -> ll::Expected<void> {
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
                        auto result = this->mParent.getPlayerInfo();
                        if (!result.has_value())
                            return ll::Unexpected(result.error());

                        for (auto& mTarget : result.value()) {
                            mPlayerNames.push_back(mTarget.second);
                            mPlayerUuids.push_back(mTarget.first);
                        }

                        break;
                    }
                }

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "wallet.gui.title"),
                    tr(language, "wallet.gui.transfer.label1"),
                    mPlayerNames
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, type, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    this->content(pl, mPlayerUuids.at(index), type).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->open(pl).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });

                form->sendPage(player, 1);

                return {};
            });
    }

    ll::Expected<void> WalletGui::redenvelope(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "wallet.gui.title"));
                form.appendLabel(fmt::format(fmt::runtime(tr(language, "wallet.gui.label")),
                    ScoreboardUtils::getScore(player, this->mParent.getTargetScoreboard()), std::to_string(this->mParent.getExchangeRate() * 100) + "%%"
                ));
                form.appendInput("Input1", tr(language, "wallet.gui.redenvelope.input1"), tr(language, "wallet.gui.redenvelope.input1.placeholder"));
                form.appendInput("Input2", tr(language, "wallet.gui.redenvelope.input2"), tr(language, "wallet.gui.redenvelope.input2.placeholder"));
                form.appendInput("Input3", tr(language, "wallet.gui.redenvelope.input3"), tr(language, "wallet.gui.redenvelope.input3.placeholder"));
                form.sendTo(player, [this, language](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->open(pl).or_else(modules::defaultErrorHandler<WalletPlugin>);

                        return;
                    }

                    std::string mObjectKey = std::get<std::string>(dt->at("Input3"));
                    if (mObjectKey.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));

                        this->open(pl).or_else(modules::defaultErrorHandler<WalletPlugin>);

                        return;
                    }

                    int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input1")), 0);
                    int mCount = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

                    std::string mScoreboard = this->mParent.getTargetScoreboard();
                    if (mScore <= 0 || mCount <= 0 || ScoreboardUtils::getScore(pl, mScoreboard) < mScore * mCount) {
                        pl.sendMessage(tr(language, "wallet.tips.redenvelope"));

                        this->open(pl).or_else(modules::defaultErrorHandler<WalletPlugin>);

                        return;
                    }

                    this->mParent.redenvelope(pl, mObjectKey, mScore, mCount).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });
            });
    }

    ll::Expected<void> WalletGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "wallet.gui.title"), 
                    fmt::format(fmt::runtime(tr(language, "wallet.gui.label")), 
                        ScoreboardUtils::getScore(player, this->mParent.getTargetScoreboard()),
                        std::to_string(this->mParent.getExchangeRate() * 100) + "%%"
                    )
                );
                form.appendButton(tr(language, "wallet.gui.transfer"), "textures/ui/MCoin", "path", [this](Player& pl) -> void {
                    this->transfer(pl, WalletTransferType::online).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });
                form.appendButton(tr(language, "wallet.gui.offlineTransfer"), "textures/ui/icon_best3", "path", [this](Player& pl) -> void {
                    this->transfer(pl, WalletTransferType::offline).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });
                form.appendButton(tr(language, "wallet.gui.redenvelope"), "textures/ui/comment", "path", [this](Player& pl) -> void {
                    this->redenvelope(pl).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });
                form.appendButton(tr(language, "wallet.gui.wealth"), "textures/ui/creative_icon", "path", [this](Player& pl) -> void {
                    this->mParent.wealth(pl).or_else(modules::defaultErrorHandler<WalletPlugin>);
                });
                form.sendTo(player);
            });
    }
}
