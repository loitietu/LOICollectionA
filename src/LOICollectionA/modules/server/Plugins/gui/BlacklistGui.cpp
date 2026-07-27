#include <string>
#include <vector>
#include <unordered_map>

#include <ll/api/Expected.h>
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
    ll::Expected<void> BlacklistGui::info(Player& player, const std::string& id) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player, id](const std::string& language) -> ll::Expected<void> {
                return this->mParent.hasBlacklist(id)
                    .and_then([this, &player, id, language](bool exists) -> ll::Expected<void> { 
                        if (!exists) {
                            player.sendMessage(tr(language, "blacklist.gui.error"));

                            return this->remove(player);
                        }

                        auto data = this->mParent.getBlacklistData(id);
                        if (!data.has_value())
                            return ll::Unexpected(data.error());

                        std::string mObjectLabel = tr(language, "blacklist.gui.info.label");
                        ll::form::SimpleForm form(tr(language, "blacklist.gui.remove.title"), 
                            fmt::format(fmt::runtime(mObjectLabel), id, 
                                data.value().at("name"),
                                data.value().at("cause"),
                                SystemUtils::toFormatTime(data.value().at("subtime"), "None"),
                                SystemUtils::toFormatTime(data.value().at("time"), "None")
                            )
                        );
                        form.appendButton(tr(language, "blacklist.gui.info.remove"), [this, id](Player&) -> void {
                            this->mParent.delBlacklist(id)
                                .or_else([](ll::Error e) -> ll::Expected<void> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == BlacklistPlugin::makeErrorCode(BlacklistPluginErrorCode::NotFound))
                                        return {};

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->remove(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> BlacklistGui::content(Player& player, Player& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, &target](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "blacklist.gui.add.title"));
                form.appendLabel(tr(language, "blacklist.gui.label"));
                form.appendInput("Input1", tr(language, "blacklist.gui.add.input1"), tr(language, "blacklist.gui.add.input1.placeholder"));
                form.appendInput("Input2", tr(language, "blacklist.gui.add.input2"), tr(language, "blacklist.gui.add.input2.placeholder"));
                form.sendTo(player, [this, language, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->add(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);

                        return;
                    }

                    Player* mTarget = ll::service::getLevel()->getPlayer(target);
                    if (!mTarget) {
                        pl.sendMessage(tr(language, "blacklist.gui.error"));

                        this->add(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);

                        return;
                    }

                    std::string mCause = std::get<std::string>(dt->at("Input1"));
                    
                    if (mCause.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));

                        this->add(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);

                        return;
                    }

                    int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                    
                    this->mParent.addBlacklist(*mTarget, mCause, time).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                });
            });
    }

    ll::Expected<void> BlacklistGui::add(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
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
                    tr(language, "blacklist.gui.add.title"),
                    tr(language, "blacklist.gui.add.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "blacklist.gui.error"));

                        this->open(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);

                        return;
                    }

                    this->content(pl, *mPlayer).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->open(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> BlacklistGui::remove(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getBlacklists()
                    .transform([this, &player, language](const std::vector<std::string>& blacklists) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "blacklist.gui.remove.title"),
                            tr(language, "blacklist.gui.remove.label"),
                            blacklists
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this](Player& pl, const std::string& response) -> void {
                            this->info(pl, response).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->open(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> BlacklistGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "blacklist.gui.title"), tr(language, "blacklist.gui.label"));
                form.appendButton(tr(language, "blacklist.gui.addBlacklist"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
                    this->add(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                });
                form.appendButton(tr(language, "blacklist.gui.removeBlacklist"), "textures/ui/free_download_symbol", "path", [this](Player& pl) -> void {
                    this->remove(pl).or_else(modules::defaultErrorHandler<BlacklistPlugin>);
                });
                form.sendTo(player);
            });
    }
}
