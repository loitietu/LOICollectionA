#include <atomic>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/TpaPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    enum SelectorType : int {
        tpa = 0,
        tphere = 1
    };

    struct TpaPlugin::operation {
        CommandSelector<Player> Target;
        SelectorType Type;
    };

    struct TpaPlugin::Impl {
        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;
        LRUKCache<std::string, bool> InviteCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Tpa options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : BlacklistCache(100, 100), InviteCache(100, 100) {}
    };

    TpaPlugin::TpaPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    TpaPlugin::~TpaPlugin() = default;

    TpaPlugin& TpaPlugin::getInstance() {
        static TpaPlugin instance;
        return instance;
    }
    
    std::shared_ptr<SQLiteStorage> TpaPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> TpaPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void TpaPlugin::gui::generic(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.setting.title"));
        form.appendLabel(tr(mObjectLanguage, "tpa.gui.setting.label"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "tpa.gui.setting.generic.switch1"), this->mParent.isInvite(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->setting(pl);

            this->mParent.mImpl->db2->set("Tpa", pl.getUuid().asString(), "invite",
                std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
            );
        });
    };

    void TpaPlugin::gui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(player, target)) {
            player.sendMessage(tr(mObjectLanguage, "tpa.gui.error"));

            this->setting(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Blacklist", target);

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

    void TpaPlugin::gui::blacklistAdd(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayers;
        std::vector<std::string> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&player, &mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer() || mTarget.getUuid() == player.getUuid())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid().asString());
            return true;
        });

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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

    void TpaPlugin::gui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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
            int mBlacklistCount = this->mParent.mImpl->options.BlacklistUpload;
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "tpa.tips2")), mBlacklistCount));

                this->setting(pl);
                return;
            }

            this->blacklistAdd(pl);
        });

        form->sendPage(player, 1);
    }

    void TpaPlugin::gui::setting(Player& player) {
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

    void TpaPlugin::gui::tpa(Player& player, Player& target, TpaType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(target);

        std::string mLabelId = (type == TpaType::tpa) ? "tpa.there" : "tpa.here";

        ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"), LOICollectionAPI::APIUtils::getInstance().translate(tr(mObjectLanguage, mLabelId), player),
            tr(mObjectLanguage, "tpa.yes"), tr(mObjectLanguage, "tpa.no")
        );
        form.sendTo(target, [this, type, &player](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                if (type == TpaType::tpa) {
                    player.teleport(pl.getPosition(), pl.getDimensionId());
                    
                    this->mParent.getLogger()->info(fmt::format(fmt::runtime(tr({}, "tpa.log1")), pl.getRealName(), player.getRealName()));
                    return;
                }

                pl.teleport(player.getPosition(), player.getDimensionId());
                
                this->mParent.getLogger()->info(fmt::format(fmt::runtime(tr({}, "tpa.log1")), player.getRealName(), pl.getRealName()));
                return;
            }
            
            player.sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(player), "tpa.no.tips")), pl.getRealName()));
        });
    }

    void TpaPlugin::gui::content(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "tpa.gui.label"));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "tpa.gui.dropdown"), { "tpa", "tphere" });
        form.sendTo(player, [this, &target, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);

            std::string mScoreboard = this->mParent.mImpl->options.TargetScoreboard;

            int mRequestRequired = this->mParent.mImpl->options.RequestRequired;
            if (mRequestRequired && ScoreboardUtils::getScore(pl, mScoreboard) < mRequestRequired) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "tpa.tips1")), mRequestRequired));
                return;
            }

            ScoreboardUtils::reduceScore(pl, mScoreboard, mRequestRequired);

            this->tpa(pl, target, 
                std::get<std::string>(dt->at("dropdown")) == "tpa" ? TpaType::tpa : TpaType::tphere
            );
        });
    }

    void TpaPlugin::gui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayers;
        std::vector<std::string> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([this, &player, &mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            std::vector<std::string> mList = this->mParent.getBlacklist(mTarget);
            if (mTarget.isSimulatedPlayer() || std::find(mList.begin(), mList.end(), player.getUuid().asString()) != mList.end() || this->mParent.isInvite(mTarget))
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid().asString());
            return true;
        });

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
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

    void TpaPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("tpa", tr({}, "commands.tpa.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("invite").required("Type").required("Target").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            std::string mObject = player.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            auto mResults = results | std::views::filter([this, mObject](Player*& mTarget) -> bool { 
                std::vector<std::string> mList = this->getBlacklist(*mTarget);
                return !mTarget->isSimulatedPlayer() && std::find(mList.begin(), mList.end(), mObject) == mList.end() && !this->isInvite(*mTarget); 
            });

            std::string mScoreboard = this->mImpl->options.TargetScoreboard;

            int mRequestRequired = this->mImpl->options.RequestRequired;
            int mMoney = mRequestRequired * static_cast<int>(std::ranges::distance(mResults));
            if (mRequestRequired && ScoreboardUtils::getScore(player, mScoreboard) < mMoney) {
                output.error(fmt::runtime(tr({}, "commands.tpa.error.invite")), mMoney);
                return;
            }

            ScoreboardUtils::reduceScore(player, mScoreboard, mMoney);

            for (Player*& pl : mResults) {
                this->mGui->tpa(player, *pl, param.Type == SelectorType::tpa
                    ? TpaType::tpa : TpaType::tphere
                );

                output.success(fmt::runtime(tr({}, "commands.tpa.success.invite")), pl->getRealName());
            }
        });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
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

    void TpaPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();

            if (!this->mImpl->db2->has("Tpa", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "invite", "false" }
                };

                this->mImpl->db2->set("Tpa", mObject, mData);
            }
        });
    }

    void TpaPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
    }

    void TpaPlugin::addBlacklist(Player& player, Player& target) {
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

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "tpa.log2"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTargetObject);
            });
    }

    void TpaPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid()) 
            return;

        if (!this->hasBlacklist(player, target)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "TpaPlugin");

            return;
        }
        
        std::string mId = this->getDatabase()->find("Blacklist", {
            { "name", target },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Blacklist", mId);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "tpa.log3"), player)), target);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [target](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), target), mList->end());
        });
    }

    std::vector<std::string> TpaPlugin::getBlacklist(Player& player) {
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

    bool TpaPlugin::hasBlacklist(Player& player, const std::string& uuid) {
        if (!this->isValid())
            return false;

        return !this->getDatabase()->find("Blacklist", {
            { "target", uuid },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool TpaPlugin::isInvite(Player& player) {
        if (!this->isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->InviteCache.contains(mObject))
            return *this->mImpl->InviteCache.get(mObject).value();
        
        bool result = this->mImpl->db2->get("Tpa", mObject, "invite", "false") == "true";

        this->mImpl->InviteCache.put(mObject, result);
        return result;
    }

    bool TpaPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool TpaPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Tpa.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "tpa.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Tpa;

        return true;
    }

    bool TpaPlugin::unload() {
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

    bool TpaPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db2->create("Tpa", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("invite");
        });

        this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("target");
            ctor("author");
            ctor("time");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool TpaPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("TpaPlugin", LOICollection::Plugins::TpaPlugin, LOICollection::Plugins::TpaPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
