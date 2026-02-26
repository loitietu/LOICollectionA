#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerChatEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/Plugins/MutePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/ChatPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct ChatPlugin::operation {
        CommandSelector<Player> Target;
        std::string Title;
        int Time = 0;
    };

    struct ChatPlugin::Impl {
        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Chat options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr PlayerChatEventListener;
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : BlacklistCache(100, 100) {}
    };

    ChatPlugin::ChatPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    ChatPlugin::~ChatPlugin() = default;

    ChatPlugin& ChatPlugin::getInstance() {
        static ChatPlugin instance;
        return instance;
    }

    std::shared_ptr<SQLiteStorage> ChatPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> ChatPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void ChatPlugin::gui::contentAdd(Player& player, Player& target) {
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
    
    void ChatPlugin::gui::contentRemove(Player& player, Player& target) {
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

    void ChatPlugin::gui::add(Player& player) {
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

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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

    void ChatPlugin::gui::remove(Player& player) {
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

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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

    void ChatPlugin::gui::title(Player& player) {
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

    void ChatPlugin::gui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(player, target)) {
            player.sendMessage(tr(mObjectLanguage, "chat.gui.error"));

            this->blacklist(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Blacklist", target);

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

    void ChatPlugin::gui::blacklistAdd(Player& player) {
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

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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

    void ChatPlugin::gui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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
            int mBlacklistCount = this->mParent.mImpl->options.BlacklistUpload;
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "chat.gui.setBlacklist.tips1")), mBlacklistCount));
                
                this->setting(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void ChatPlugin::gui::setting(Player& player) {
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

    void ChatPlugin::gui::open(Player& player) {
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

    void ChatPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("chat", tr({}, "commands.chat.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("add").required("Target").required("Title").optional("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& pl : results) {
                    this->addTitle(*pl, param.Title, param.Time);

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.add")), param.Title, pl->getRealName());
                }
            });
        command.overload<operation>().text("remove").required("Target").required("Title").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& pl : results) {
                    this->delTitle(*pl, param.Title);

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.remove")), pl->getRealName(), param.Title);
                }
            });
        command.overload<operation>().text("set").required("Target").required("Title").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& pl : results) {
                    this->setTitle(*pl, param.Title);

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.set")), pl->getRealName(), param.Title);
                }
            });
        command.overload<operation>().text("list").required("Target").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& player : results) {
                    std::vector<std::string> mObjectList = this->getTitles(*player);
                    
                    if (mObjectList.empty())
                        return output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.list")), player->getRealName(), "None");

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.list")), player->getRealName(), fmt::join(mObjectList, ", "));
                }
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->mGui->setting(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void ChatPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            
            if (!this->mImpl->db2->has("Chat", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "title", "None" }
                };

                this->mImpl->db2->set("Chat", mObject, mData);
            }
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || MutePlugin::getInstance().isMute(event.self()))
                return;

            event.cancel();

            std::string mChat = LOICollectionAPI::APIUtils::getInstance().translate(this->mImpl->options.FormatText, event.self());
            
            TextPacket packet = TextPacket::createChat("", 
                fmt::format(fmt::runtime(mChat), event.message()), 
                "", event.self().getXuid(), ""
            );

            ll::service::getLevel()->forEachPlayer([this, packet, &player = event.self()](Player& mTarget) -> bool {
                if (!mTarget.isSimulatedPlayer() && !this->isBlacklist(mTarget, player))
                    packet.sendTo(mTarget);
                return true;
            });
        }, ll::event::EventPriority::Normal);
    }

    void ChatPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);
    }

    void ChatPlugin::setTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return;

        this->mImpl->db2->set("Chat", player.getUuid().asString(), "title", text);
    }

    void ChatPlugin::addTitle(Player& player, const std::string& text, int time) {
        if (!this->isValid()) 
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "title", text },
            { "author", mObject },
            { "time", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 60, "None") : "None" }
        };

        this->getDatabase()->set("Titles", mTismestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log2"), player)), text);
    }

    void ChatPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", target.getRealName() },
            { "target", mTargetObject },
            { "author", mObject },
            { "time", SystemUtils::getNowTime("%Y%m%d%H%M%S") }
        };

        this->getDatabase()->set("Blacklist", mTismestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log5"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTargetObject);
            });
    }

    void ChatPlugin::delTitle(Player& player, const std::string& text) {
        if (!this->isValid())
             return;

        if (!this->hasTitle(player, text)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "ChatPlugin");

            return;
        }

        std::string mId = this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Titles", mId);

        std::string mObject = player.getUuid().asString();
        if (this->mImpl->db2->get("Chat", mObject, "title", "None") == text)
            this->setTitle(player, "None");

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log3"), player)), text);
    }

    void ChatPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid())
            return;

        if (!this->hasBlacklist(player, target)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "ChatPlugin");

            return;
        }

        std::string mId = this->getDatabase()->find("Blacklist", {
            { "name", target },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Blacklist", mId);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log6"), player)), target);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [target](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), target), mList->end());
        });
    }

    std::string ChatPlugin::getTitle(Player& player) {
        if (!this->isValid())
            return "None";

        std::string mObject = player.getUuid().asString();

        std::string mTitle = this->mImpl->db2->get("Chat", mObject, "title", "None");
        std::string mId = this->getDatabase()->find("Titles", {
            { "title", mTitle },
            { "author", mObject }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return "None";

        std::unordered_map<std::string, std::string> mData = this->getDatabase()->get("Titles", mId);
        if (SystemUtils::isPastOrPresent(mData.at("time"))) {
            this->setTitle(player, "None");

            this->getDatabase()->del("Titles", mId);
            return "None";
        }

        return mTitle;
    }

    std::string ChatPlugin::getTitleTime(Player& player, const std::string& text) {
        if (!this->isValid())
            return "None";

        std::string mId = this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return "None";

        return this->getDatabase()->get("Titles", mId, "title", "None");
    }

    std::vector<std::string> ChatPlugin::getTitles(Player& player) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Titles", "title", {
            { "author", player.getUuid().asString() }
        });
    }

    std::vector<std::string> ChatPlugin::getBlacklist(Player& player) {
        if (!this->isValid()) 
            return {};

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->BlacklistCache.contains(mObject))
            return *this->mImpl->BlacklistCache.get(mObject).value();

        std::vector<std::string> mKeys = this->getDatabase()->find("Blacklist", {
            { "author", mObject }
        }, SQLiteStorage::FindCondition::AND);

        this->mImpl->BlacklistCache.put(mObject, mKeys);
        return mKeys;
    }

    bool ChatPlugin::hasTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return false;

        return !this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool ChatPlugin::hasBlacklist(Player& player, const std::string& uuid) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mList = this->mImpl->BlacklistCache.get(mObject).value();

            return std::find(mList->begin(), mList->end(), uuid) != mList->end();
        }

        return !this->getDatabase()->find("Blacklist", {
            { "target", uuid },
            { "author", mObject }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool ChatPlugin::isTitle(Player& player, const std::string& text) {
        if (!this->isValid()) 
            return false;

        return !this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool ChatPlugin::isBlacklist(Player& player, Player& target) {
        if (!this->isValid()) 
            return false;

        return this->hasBlacklist(player, target.getUuid().asString());
    }

    bool ChatPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool ChatPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Chat.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "chat.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Chat;

        return true;
    }

    bool ChatPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->db2.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool ChatPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db2->create("Chat", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("title");
        });

        this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("target");
            ctor("author");
            ctor("time");
        });
        this->getDatabase()->create("Titles", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("title");
            ctor("author");
            ctor("time");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool ChatPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("ChatPlugin", LOICollection::Plugins::ChatPlugin, LOICollection::Plugins::ChatPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
