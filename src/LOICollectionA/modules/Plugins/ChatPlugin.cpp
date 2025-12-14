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

        C_Config::C_Plugins::C_Chat options;

        std::unique_ptr<SQLiteStorage> db;
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

    SQLiteStorage* ChatPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* ChatPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void ChatPlugin::gui::contentAdd(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "chat.gui.manager.add.input1"), tr(mObjectLanguage, "chat.gui.manager.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "chat.gui.manager.add.input2"), tr(mObjectLanguage, "chat.gui.manager.add.input2.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, &target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->add(pl);

            std::string mTitle = std::get<std::string>(dt->at("Input1"));

            if (mTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->add(pl);
            }

            int mTime = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            this->mParent.addTitle(target, mTitle, mTime);
        });
    }
    
    void ChatPlugin::gui::contentRemove(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "chat.gui.label"));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.manager.remove.dropdown"), this->mParent.getTitles(target));
        form.sendTo(player, [this, &target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->remove(pl);

            this->mParent.delTitle(target, std::get<std::string>(dt->at("dropdown")));
        });
    }

    void ChatPlugin::gui::add(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.add.label"));
        ll::service::getLevel()->forEachPlayer([this, &form](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->contentAdd(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void ChatPlugin::gui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.manager.remove.label"));
        ll::service::getLevel()->forEachPlayer([this, &form](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->contentRemove(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void ChatPlugin::gui::title(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "chat.gui.title"));
        form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translateString(tr(mObjectLanguage, "chat.gui.setTitle.label"), player));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "chat.gui.setTitle.dropdown"), this->mParent.getTitles(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->setting(pl);

            std::string mObject = pl.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            this->mParent.mImpl->db2->set("OBJECT$" + mObject, "Chat_Title", std::get<std::string>(dt->at("dropdown")));
            
            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "chat.log1"), pl));
        });
    }

    void ChatPlugin::gui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::string mObjectLabel = tr(mObjectLanguage, "chat.gui.setBlacklist.set.label");

        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), 
            fmt::format(fmt::runtime(mObjectLabel), target,
                this->mParent.getDatabase()->get("Blacklist", mObject + "." + target + "_NAME", "None"),
                SystemUtils::toFormatTime(this->mParent.getDatabase()->get("Blacklist", mObject + "." + target + "_TIME", "None"), "None")
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
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.setBlacklist.add.label"));
        ll::service::getLevel()->forEachPlayer([this, &form, &player](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->mParent.addBlacklist(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->blacklist(pl);
        });
    }

    void ChatPlugin::gui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "chat.gui.title"), tr(mObjectLanguage, "chat.gui.label"));
        form.appendButton(tr(mObjectLanguage, "chat.gui.setBlacklist.add"), [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = (int) this->mParent.getBlacklist(pl).size();
            if (((int) this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "chat.gui.setBlacklist.tips1")), mBlacklistCount));
                return this->setting(pl);
            }

            this->blacklistAdd(pl);
        });
        form.appendDivider();
        for (std::string& mTarget : this->mParent.getBlacklist(player)) {
            form.appendButton(mTarget, [this, mTarget](Player& pl) -> void {
                this->blacklistSet(pl, mTarget);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->setting(pl);
        });
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
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("chat", tr({}, "commands.chat.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("add").required("Target").required("Title").optional("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));

            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            for (Player*& pl : results) {
                this->addTitle(*pl, param.Title, param.Time);

                output.success(fmt::runtime(tr({}, "commands.chat.success.add")), param.Title, pl->getRealName());
            }
        });
        command.overload<operation>().text("remove").required("Target").required("Title").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));

            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            for (Player*& pl : results) {
                this->delTitle(*pl, param.Title);

                output.success(fmt::runtime(tr({}, "commands.chat.success.remove")), pl->getRealName(), param.Title);
            }
        });
        command.overload<operation>().text("set").required("Target").required("Title").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));

            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            for (Player*& pl : results) {
                std::string mObject = pl->getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                
                this->mImpl->db2->set("OBJECT$" + mObject, "Chat_Title", param.Title);

                output.success(fmt::runtime(tr({}, "commands.chat.success.set")), pl->getRealName(), param.Title);
            }
        });
        command.overload<operation>().text("list").required("Target").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));

            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            for (Player*& player : results) {
                std::vector<std::string> mObjectList = this->getTitles(*player);
                
                if (mObjectList.empty())
                    return output.success(fmt::runtime(tr({}, "commands.chat.success.list")), player->getRealName(), "None");

                output.success(fmt::runtime(tr({}, "commands.chat.success.list")), player->getRealName(), fmt::join(mObjectList, ", "));
            }
        });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));

            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->mGui->setting(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void ChatPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            if (!this->mImpl->db2->has("OBJECT$" + mObject, "Chat_Title"))
                this->mImpl->db2->set("OBJECT$" + mObject, "Chat_Title", "None");
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || MutePlugin::getInstance().isMute(event.self()))
                return;

            event.cancel();

            std::string mChat = LOICollectionAPI::APIUtils::getInstance().translateString(this->mImpl->options.FormatText, event.self());
            
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

    void ChatPlugin::addTitle(Player& player, const std::string& text, int time) {
        if (!this->isValid()) 
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        
        this->getDatabase()->set("Titles", mObject + "." + text, time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time, "None") : "None");
        
        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "chat.log2"), player)), text);
    }

    void ChatPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        this->getDatabase()->set("Blacklist", mObject + "." + mTargetObject + "_NAME", target.getRealName());
        this->getDatabase()->set("Blacklist", mObject + "." + mTargetObject + "_TIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "chat.log5"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::vector<std::string>& mList) -> void {
                mList.push_back(mTargetObject);
            });
    }

    void ChatPlugin::delTitle(Player& player, const std::string& text) {
        if (!this->isValid())
             return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (this->getDatabase()->has("Titles", mObject + "." + text))
            this->getDatabase()->del("Titles", mObject + "." + text);

        if (this->mImpl->db2->get("OBJECT$" + mObject, "Chat_Title", "None") == text)
            this->mImpl->db2->set("OBJECT$" + mObject, "Chat_Title", "None");

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "chat.log3"), player)), text);
    }

    void ChatPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (this->getDatabase()->hasByPrefix("Blacklist", mObject + "." + target + "_TIME", 2))
            this->getDatabase()->delByPrefix("Blacklist", mObject + "." + target);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "chat.log6"), player)), target);

        this->mImpl->BlacklistCache.update(mObject, [target](std::vector<std::string>& mList) -> void {
            mList.erase(std::remove(mList.begin(), mList.end(), target), mList.end());
        });
    }

    std::string ChatPlugin::getTitle(Player& player) {
        if (!this->isValid())
            return "None";

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::string mTitle = this->mImpl->db2->get("OBJECT$" + mObject, "Chat_Title", "None");
        
        if (SystemUtils::isPastOrPresent(this->getDatabase()->get("Titles", mObject + "." + mTitle, "None"))) {
            this->mImpl->db2->set("OBJECT$" + mObject, "Chat_Title", "None");
            this->getDatabase()->del("Titles", mObject + "." + mTitle);
            
            return "None";
        }

        return mTitle;
    }

    std::string ChatPlugin::getTitleTime(Player& player, const std::string& text) {
        if (!this->isValid())
            return "None";

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return this->getDatabase()->get("Titles", mObject + "." + text, "None"); 
    }

    std::vector<std::string> ChatPlugin::getTitles(Player& player) {
        if (!this->isValid())
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Titles", mObject + ".");

        std::vector<std::string> mResult(mKeys.size());
        std::transform(mKeys.begin(), mKeys.end(), mResult.begin(), [](const std::string& mKey) -> std::string {
            size_t mPos = mKey.find('.');
            return (mPos != std::string::npos) ? mKey.substr(mPos + 1) : "";
        });

        return mResult;
    }

    std::vector<std::string> ChatPlugin::getBlacklist(Player& player) {
        if (!this->isValid()) 
            return {};

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (this->mImpl->BlacklistCache.contains(mObject))
             return this->mImpl->BlacklistCache.get(mObject).value();

        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Blacklist", mObject + ".%\\_NAME");

        std::vector<std::string> mResult(mKeys.size());
        std::transform(mKeys.begin(), mKeys.end(), mResult.begin(), [](const std::string& mKey) -> std::string {
            size_t mPos = mKey.find('.');
            size_t mPos2 = mKey.find_last_of('_');
            return (mPos != std::string::npos && mPos2 != std::string::npos && mPos < mPos2) ? mKey.substr(mPos + 1, mPos2 - mPos - 1) : "";
        });

        this->mImpl->BlacklistCache.put(mObject, mResult);

        return mResult;
    }

    bool ChatPlugin::isTitle(Player& player, const std::string& text) {
        if (!this->isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return this->getDatabase()->has("Titles", mObject + "." + text);
    }

    bool ChatPlugin::isBlacklist(Player& player, Player& target) {
        if (!this->isValid()) 
            return false;

        std::string mObjcet = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::replace(mObjcet.begin(), mObjcet.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        if (this->mImpl->BlacklistCache.contains(mObjcet)) {
            std::vector<std::string> mList = this->mImpl->BlacklistCache.get(mObjcet).value();
            return std::find(mList.begin(), mList.end(), mTargetObject) != mList.end();
        }

        return this->getDatabase()->has("Blacklist", mObjcet + "." + mTargetObject + "_TIME");
    }

    bool ChatPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool ChatPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Chat.ModuleEnabled)
            return false;

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            return true;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "chat.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Chat;

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

        this->getDatabase()->create("Blacklist");
        this->getDatabase()->create("Titles");
        
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

REGISTRY_HELPER("ChatPlugin", LOICollection::Plugins::ChatPlugin, LOICollection::Plugins::ChatPlugin::getInstance())
