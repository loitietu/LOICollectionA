#include <string>
#include <vector>
#include <unordered_map>

#include <ll/api/Expected.h>
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
    ll::Expected<void> ChatGui::contentAdd(Player& player, Player& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player, &target](const std::string& language) -> void {
                ll::form::CustomForm form(tr(language, "chat.gui.title"));
                form.appendLabel(tr(language, "chat.gui.label"));
                form.appendInput("Input1", tr(language, "chat.gui.manager.add.input1"), tr(language, "chat.gui.manager.add.input1.placeholder"));
                form.appendInput("Input2", tr(language, "chat.gui.manager.add.input2"), tr(language, "chat.gui.manager.add.input2.placeholder"));
                form.sendTo(player, [this, language, target = target.getUuid()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                    if (!dt) {
                        this->add(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);

                        return;
                    }

                    Player* mTarget = ll::service::getLevel()->getPlayer(target);
                    if (!mTarget) {
                        pl.sendMessage(tr(language, "chat.gui.error"));

                        this->add(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);

                        return;
                    }

                    std::string mTitle = std::get<std::string>(dt->at("Input1"));

                    if (mTitle.empty()) {
                        pl.sendMessage(tr(language, "generic.tips.noinput"));

                        this->add(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);

                        return;
                    }

                    int mTime = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

                    this->mParent.addTitle(*mTarget, mTitle, mTime).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
            });
    }
    
    ll::Expected<void> ChatGui::contentRemove(Player& player, Player& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player, &target](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getTitles(player)
                    .transform([this, language, &player, &target](const std::vector<std::string>& titles) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "chat.gui.title"),
                            tr(language, "chat.gui.manager.remove.select.label"),
                            titles
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this, language, target = target.getUuid()](Player& pl, const std::string& response) -> void {
                            Player* mTarget = ll::service::getLevel()->getPlayer(target);
                            if (!mTarget) {
                                pl.sendMessage(tr(language, "chat.gui.error"));

                                this->remove(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                                return;
                            }

                            this->mParent.delTitle(*mTarget, response)
                                .or_else([](ll::Error e) -> ll::Expected<void> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == ChatPlugin::makeErrorCode(ChatPluginErrorCode::TitleNotFound))
                                        return {};

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->remove(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> ChatGui::add(Player& player) {
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
                    tr(language, "chat.gui.title"),
                    tr(language, "chat.gui.setBlacklist.add.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "chat.gui.error"));

                        this->open(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        return;
                    }

                    this->contentAdd(pl, *mPlayer).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->open(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ChatGui::remove(Player& player) {
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
                    tr(language, "chat.gui.title"),
                    tr(language, "chat.gui.manager.remove.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "chat.gui.error"));

                        this->open(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        return;
                    }

                    this->contentRemove(pl, *mPlayer).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->open(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ChatGui::title(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getTitles(player)
                    .transform([this, language, &player](const std::vector<std::string>& titles) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "chat.gui.title"),
                            LOICollectionAPI::APIUtils::getInstance().translate(tr(language, "chat.gui.setTitle.label"), player),
                            titles
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this, language](Player& pl, const std::string& response) -> void {
                            this->mParent.setTitle(pl, response).or_else(modules::defaultErrorHandler<ChatPlugin>);
            
                            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log1"), pl));
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->setting(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> ChatGui::blacklistSet(Player& player, const std::string& target) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player, &target](const std::string& language) -> ll::Expected<void> {
                return this->mParent.hasBlacklist(player, target)
                    .and_then([this, language, &player, &target](bool exists) -> ll::Expected<void> {
                        if (!exists) {
                            player.sendMessage(tr(language, "chat.gui.error"));

                            return this->blacklist(player);
                        }

                        auto data = this->mParent.getBlacklistData(target);
                        if (!data.has_value())
                            return ll::Unexpected(data.error());

                        ll::form::SimpleForm form(tr(language, "chat.gui.title"), 
                            fmt::format(fmt::runtime(tr(language, "chat.gui.setBlacklist.set.label")),
                                data.value().at("target"),
                                data.value().at("name"),
                                SystemUtils::toFormatTime(data.value().at("time"), "None")
                            )
                        );
                        form.appendButton(tr(language, "chat.gui.setBlacklist.set.remove"), [this, target](Player& pl) -> void {
                            this->mParent.delBlacklist(pl, target)
                                .or_else([](ll::Error e) -> ll::Expected<void> {
                                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == ChatPlugin::makeErrorCode(ChatPluginErrorCode::BlacklistNotFound))
                                        return {};

                                    return ll::Unexpected(e);
                                })
                                .or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });
                        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
                            if (id == -1)
                                this->blacklist(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });

                        return {};
                    });
            });
    }

    ll::Expected<void> ChatGui::blacklistAdd(Player& player) {
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
                    tr(language, "chat.gui.title"),
                    tr(language, "chat.gui.setBlacklist.add.label"),
                    mPlayers
                );
                form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                form->setNextButton(tr(language, "generic.gui.page.next"));
                form->setChooseButton(tr(language, "generic.gui.page.choose"));
                form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                form->setCallback([this, language, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
                    Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
                    if (!mPlayer) {
                        pl.sendMessage(tr(language, "chat.gui.error"));

                        this->blacklist(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        return;
                    }

                    this->mParent.addBlacklist(pl, *mPlayer).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form->setCloseCallback([this](Player& pl) -> void {
                    this->blacklist(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });

                form->sendPage(player, 1);
            });
    }

    ll::Expected<void> ChatGui::blacklist(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, &player](const std::string& language) -> ll::Expected<void> {
                return this->mParent.getBlacklist(player)
                    .transform([this, language, &player](const std::vector<std::string>& ids) -> void {
                        std::shared_ptr<form::PaginatedForm> form = std::make_shared<form::PaginatedForm>(
                            tr(language, "chat.gui.title"),
                            tr(language, "chat.gui.label"),
                            ids
                        );
                        form->setPreviousButton(tr(language, "generic.gui.page.previous"));
                        form->setNextButton(tr(language, "generic.gui.page.next"));
                        form->setChooseButton(tr(language, "generic.gui.page.choose"));
                        form->setChooseInput(tr(language, "generic.gui.page.choose.input"));
                        form->setCallback([this](Player& pl, const std::string& response) -> void {
                            this->blacklistSet(pl, response).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });
                        form->setCloseCallback([this](Player& pl) -> void {
                            this->setting(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });

                        form->appendDivider();
                        form->appendButton(tr(language, "chat.gui.setBlacklist.add"), "", [this, language](Player& pl) -> void {
                            int mBlacklistCount = this->mParent.getBlacklistUpload();

                            this->mParent.getBlacklist(pl)
                                .and_then([this, language, mBlacklistCount, &pl](const std::vector<std::string>& ids) -> ll::Expected<void> {
                                    if (static_cast<int>(ids.size()) >= mBlacklistCount) {
                                        pl.sendMessage(fmt::format(fmt::runtime(tr(language, "chat.gui.setBlacklist.tips1")), mBlacklistCount));
                                        
                                        return this->setting(pl);
                                    }

                                    return this->blacklistAdd(pl);
                                })
                                .or_else(modules::defaultErrorHandler<ChatPlugin>);
                        });

                        form->sendPage(player, 1);
                    });
            });
    }

    ll::Expected<void> ChatGui::setting(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "chat.gui.title"), tr(language, "chat.gui.label"));
                form.appendButton(tr(language, "chat.gui.setTitle"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
                    this->title(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form.appendButton(tr(language, "chat.gui.setBlacklist"), "textures/ui/icon_book_writable", "path", [this](Player& pl) -> void {
                    this->blacklist(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form.sendTo(player);
            });
    }

    ll::Expected<void> ChatGui::open(Player& player) {
        return LanguagePlugin::getShared()->getLanguage(player)
            .transform([this, &player](const std::string& language) -> void {
                ll::form::SimpleForm form(tr(language, "chat.gui.title"), tr(language, "chat.gui.label"));
                form.appendButton(tr(language, "chat.gui.manager.add"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
                    this->add(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form.appendButton(tr(language, "chat.gui.manager.remove"), "textures/ui/free_download_symbol", "path", [this](Player& pl) -> void {
                    this->remove(pl).or_else(modules::defaultErrorHandler<ChatPlugin>);
                });
                form.sendTo(player);
            });
    }
}
