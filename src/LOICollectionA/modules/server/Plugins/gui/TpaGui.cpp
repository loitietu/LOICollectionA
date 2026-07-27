#include <string>
#include <unordered_map>

#include <ll/api/Expected.h>
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
    ll::Expected<void> TpaGui::generic(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.isInvite(player)
                    .transform([this, language, &player](bool exists) -> void {
                        ll::form::CustomForm form(tr(language, "tpa.gui.setting.title"));
                        form.appendLabel(tr(language, "tpa.gui.setting.label"));
                        form.appendToggle("Toggle1", tr(language, "tpa.gui.setting.generic.switch1"), exists);
                        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                            if (!dt) {
                                this->setting(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);

                                return;
                            }

                            this->mParent.setInvite(pl, std::get<uint64>(dt->at("Toggle1"))).or_else(modules::defaultErrorHandler<TpaPlugin>);
                        });
                    });
            });
    };

    ll::Expected<void> TpaGui::blacklistSet(Player& player, const std::string& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player, &target](const std::string& language) -> ll::Expected<void> {
                return this->mParent.hasBlacklist(player, target)
                    .and_then([this, language, &player, &target](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "tpa.gui.error"));

                            return this->setting(player);
                        }

                        auto data = this->mParent.getBlacklistData(target);
                        if (!data.has_value())
                            return ll::Unexpected(data.error());

                        ll::form::SimpleForm form(tr(language, "tpa.gui.setting.title"), 
                            fmt::format(fmt::runtime(tr(language, "tpa.gui.setting.blacklist.set.label")),
                                data.value().at("target"),
                                data.value().at("name"),
                                SystemUtils::toFormatTime(data.value().at("time"), "None")
                            )
                        );
                        form.appendButton(tr(language, "tpa.gui.setting.blacklist.set.remove"), [this, target](Player& pl) -> void {
                            this->mParent.delBlacklist(pl, target)
                                .or_else([](ll::Error e) -> ll::Expected<void> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == TpaPlugin::makeErrorCode(TpaPluginErrorCode::BlacklistNotFound))
                                        return {};

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<TpaPlugin>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->blacklist(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> TpaGui::blacklistAdd(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
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
                    tr(language, "tpa.gui.setting.title"),
                    tr(language, "tpa.gui.setting.blacklist.add.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "tpa.gui.error"));

                        this->setting(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);

                        return;
                    }

                    this->mParent.addBlacklist(pl, *mPlayer).or_else(modules::defaultErrorHandler<TpaPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->setting(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> TpaGui::blacklist(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getBlacklist(player)
                    .transform([this, language, &player](const std::vector<std::string>& blacklists) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "tpa.gui.setting.title"),
                            tr(language, "tpa.gui.setting.label"),
                            blacklists
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this](Player& pl, const std::string& response) -> void {
                            this->blacklistSet(pl, response).or_else(modules::defaultErrorHandler<TpaPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->setting(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);
                        });
                        form->appendDivider();
                        form->appendButton(tr(language, "tpa.gui.setting.blacklist.add"), "textures/ui/editIcon", [this, language](Player& pl) -> void {
                            int mBlacklistCount = this->mParent.getBlacklistUpload();

                            this->mParent.getBlacklist(pl)
                                .and_then([this, language, mBlacklistCount, &pl](const std::vector<std::string>& ids) -> ll::Expected<void> {
                                    if (static_cast<int>(ids.size()) >= mBlacklistCount) {
                                        pl.sendMessage(fmt::format(fmt::runtime(tr(language, "tpa.tips2")), mBlacklistCount));
                                        
                                        return this->setting(pl);
                                    }

                                    return this->blacklistAdd(pl);
                                })
                                .or_else(modules::defaultErrorHandler<TpaPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> TpaGui::setting(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "tpa.gui.setting.title"), tr(language, "tpa.gui.setting.label"));
                form.appendButton(tr(language, "tpa.gui.setting.generic"), "textures/ui/icon_setting", "path", [this](Player& pl) -> void {
                    this->generic(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);
                });
                form.appendButton(tr(language, "tpa.gui.setting.blacklist"), "textures/ui/icon_lock", "path", [this](Player& pl) -> void {
                    this->blacklist(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);
                });
                form.sendTo(player);
            });
    }

    ll::Expected<void> TpaGui::tpa(Player& player, Player& target, TpaType type) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<bool> {
                int mRequestUpload = this->mParent.getRequestUpload();
                if (this->mParent.getRequestCount(player) >= mRequestUpload) {
                    player.sendMessage(fmt::format(fmt::runtime(tr(language, "tpa.tips5")), mRequestUpload));
                    
                    return false;
                }

                return true;
            })
            .and_then([&target](bool exists) -> ll::Expected<std::string> {
                if (!exists)
                    return {};

                return LanguagePlugin::getShared()->getLanguage(target);
            })
            .and_then([this, &player, &target, type](const std::string& language) -> ll::Expected<void> {
                if (language.empty())
                    return {};

                std::string id = SystemUtils::getCurrentTimestamp();

                auto result = this->mParent.sendRequest(player, target, id, type)
                    .or_else([](ll::Error e) -> ll::Expected<void> {
                        if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == TpaPlugin::makeErrorCode(TpaPluginErrorCode::RequestExists))
                            return {};

                        return ll::Unexpected(e);
                    });

                if (!result.has_value())
                    return ll::Unexpected(result.error());

                ll::form::ModalForm form(tr(language, "tpa.gui.title"),
                    LOICollectionAPI::APIUtils::getInstance().translate(tr(language, (type == TpaType::tpa) ? "tpa.there" : "tpa.here"), player),
                    tr(language, "tpa.yes"),
                    tr(language, "tpa.no")
                );
                form.sendTo(target, [this, language, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                    if (result == ll::form::ModalFormSelectedButton::Upper) {
                        this->mParent.acceptRequest(pl, id)
                            .or_else([](ll::Error e) -> ll::Expected<bool> {
                                if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == TpaPlugin::makeErrorCode(TpaPluginErrorCode::RequestNotFound))
                                    return false;

                                return ll::Unexpected(e);
                            })
                            .transform([language, &pl](bool exists) -> void {
                                if (!exists) 
                                    pl.sendMessage(tr(language, "tpa.gui.error"));
                            })
                            .or_else(modules::defaultErrorHandler<TpaPlugin>);

                        return;
                    }
                    
                    this->mParent.rejectRequest(pl, id)
                        .or_else([](ll::Error e) -> ll::Expected<bool> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == TpaPlugin::makeErrorCode(TpaPluginErrorCode::RequestNotFound))
                                return {};

                            return ll::Unexpected(e);
                        })
                        .or_else(modules::defaultErrorHandler<TpaPlugin, bool>);
                });

                return {};
            });
    }

    ll::Expected<void> TpaGui::content(Player& player, Player& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, &target](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "tpa.gui.title"));
                form.appendLabel(tr(language, "tpa.gui.label"));
                form.appendDropdown("dropdown", tr(language, "tpa.gui.dropdown"), { "tpa", "tphere" });
                form.sendTo(player, [this, language, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->open(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);

                        return;
                    }

                    Player* mTarget = ll::service::getLevel()->getPlayer(target);
                    if (!mTarget) {
                        pl.sendMessage(tr(language, "tpa.gui.error"));

                        this->open(pl).or_else(modules::defaultErrorHandler<TpaPlugin>);

                        return;
                    }

                    this->mParent.forTpaContent(pl)
                        .and_then([this, language, &pl, &mTarget, &dt](bool exists) -> ll::Expected<void> {
                            if (!exists) {
                                pl.sendMessage(tr(language, "tpa.tips1"));

                                return {};
                            }

                            return this->tpa(pl, *mTarget, 
                                std::get<std::string>(dt->at("dropdown")) == "tpa" ? TpaType::tpa : TpaType::tphere
                            );
                        })
                        .or_else(modules::defaultErrorHandler<TpaPlugin>);
                });
            });
    }

    ll::Expected<void> TpaGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                std::vector<std::string> mPlayers;
                std::vector<mce::UUID> mPlayerUuids;

                ll::service::getLevel()->forEachPlayer([this, &player, &mPlayers, &mPlayerUuids](Player& target) -> bool {
                    std::string uuid = player.getUuid().asString();

                    auto result = this->mParent.getBlacklist(target)
                        .and_then([this](const std::vector<std::string>& ids) -> ll::Expected<std::vector<std::string>> {
                            return this->mParent.getBlacklistFromTarget(ids);
                        })
                        .transform([uuid, &target](const std::vector<std::string>& ids) -> bool {
                            return !target.isSimulatedPlayer() && std::find(ids.begin(), ids.end(), uuid) == ids.end();
                        })
                        .and_then([this, uuid, &target](bool exists) -> ll::Expected<bool> {
                            return this->mParent.isInvite(target)
                                .transform([uuid, exists, &target](bool invite) -> bool { 
                                    return exists && !invite && target.getUuid().asString() != uuid;
                                });
                        });

                    if (!result.has_value()) {
                        modules::defaultErrorHandler<TpaPlugin>(result.error());

                        return true;
                    }

                    if (result.value()) {
                        mPlayers.push_back(target.getRealName());
                        mPlayerUuids.push_back(target.getUuid());
                    }

                    return true;
                });

                std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                    tr(language, "tpa.gui.title"),
                    tr(language, "tpa.gui.label2"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "tpa.gui.error"));

                        return;
                    }

                    this->content(pl, *mPlayer).or_else(modules::defaultErrorHandler<TpaPlugin>);
                });

                form->sendPage(player, 1);
            });
    }
}
