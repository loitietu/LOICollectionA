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

        C_Config::C_Plugins::C_Tpa options;

        std::unique_ptr<SQLiteStorage> db;
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
    
    SQLiteStorage* TpaPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* TpaPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void TpaPlugin::gui::generic(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.setting.title"));
        form.appendLabel(tr(mObjectLanguage, "tpa.gui.setting.label"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "tpa.gui.setting.generic.switch1"), this->mParent.isInvite(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->setting(pl);

            std::string mObject = pl.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            
            this->mParent.mImpl->db2->set("OBJECT$" + mObject, "Tpa_Toggle1",
                std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
            );
        });
    };

    void TpaPlugin::gui::blacklistSet(Player& player, const std::string& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        std::string mObjectLabel = tr(mObjectLanguage, "tpa.gui.setting.blacklist.set.label");

        ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), 
            fmt::format(fmt::runtime(mObjectLabel), target,
                this->mParent.getDatabase()->get("Mute", mObject + "." + target + "_NAME", "None"),
                SystemUtils::toFormatTime(this->mParent.getDatabase()->get("Mute", mObject + "." + target + "_TIME", "None"), "None")
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
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.blacklist.add.label"));
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

    void TpaPlugin::gui::blacklist(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.setting.title"), tr(mObjectLanguage, "tpa.gui.setting.label"));
        form.appendButton(tr(mObjectLanguage, "tpa.gui.setting.blacklist.add"), "textures/ui/editIcon", "path", [this, mObjectLanguage](Player& pl) -> void {
            int mBlacklistCount = this->mParent.mImpl->options.BlacklistUpload;
            if (static_cast<int>(this->mParent.getBlacklist(pl).size()) >= mBlacklistCount) {
                pl.sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "tpa.tips2")), mBlacklistCount));
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
            if (id == -1) return this->setting(pl);
        });
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

        ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"), LOICollectionAPI::APIUtils::getInstance().translateString(tr(mObjectLanguage, mLabelId), player),
            tr(mObjectLanguage, "tpa.yes"), tr(mObjectLanguage, "tpa.no")
        );
        form.sendTo(target, [this, type, &player](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
            if (result == ll::form::ModalFormSelectedButton::Upper) {
                std::string logString = tr({}, "tpa.log1");

                if (type == TpaType::tpa) {
                    player.teleport(pl.getPosition(), pl.getDimensionId());
                    
                    logString = fmt::format(fmt::runtime(logString), pl.getRealName(), player.getRealName());
                } else {
                    pl.teleport(player.getPosition(), player.getDimensionId());
                    
                    logString = fmt::format(fmt::runtime(logString), player.getRealName(), pl.getRealName());
                }

                this->mParent.getLogger()->info(logString);
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

        std::string mObject = player.getUuid().asString();

        ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.title"), tr(mObjectLanguage, "tpa.gui.label2"));
        ll::service::getLevel()->forEachPlayer([this, &form, mObject](Player& mTarget) -> bool {
            std::vector<std::string> mList = this->mParent.getBlacklist(mTarget);
            if (mTarget.isSimulatedPlayer() || std::find(mList.begin(), mList.end(), mObject) != mList.end() || this->mParent.isInvite(mTarget))
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->content(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player);
    }

    void TpaPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
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
            int mMoney = mRequestRequired * static_cast<int>(std::distance(mResults.begin(), mResults.end()));
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
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            
            if (!this->mImpl->db2->has("OBJECT$" + mObject, "Tpa_Toggle1"))
                this->mImpl->db2->set("OBJECT$" + mObject, "Tpa_Toggle1", "false");
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
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        std::replace(mTargetObject.begin(), mTargetObject.end(), '-', '_');

        this->getDatabase()->set("Blacklist", mObject + "." + mTargetObject + "_NAME", target.getRealName());
        this->getDatabase()->set("Blacklist", mObject + "." + mTargetObject + "_TIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "tpa.log2"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::vector<std::string>& mList) -> void {
                mList.push_back(mTargetObject);
            });
    }

    void TpaPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid()) 
            return;
        
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (this->getDatabase()->hasByPrefix("Blacklist", mObject + "." + target, 2))
            this->getDatabase()->delByPrefix("Blacklist", mObject + "." + target);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().getVariableString(tr({}, "tpa.log3"), player)), target);

        this->mImpl->BlacklistCache.update(mObject, [target](std::vector<std::string>& mList) -> void {
            mList.erase(std::remove(mList.begin(), mList.end(), target), mList.end());
        });
    }

    std::vector<std::string> TpaPlugin::getBlacklist(Player& player) {
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

    bool TpaPlugin::isInvite(Player& player) {
        if (!this->isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (this->mImpl->InviteCache.contains(mObject))
            return this->mImpl->InviteCache.get(mObject).value();
        
        bool result = this->mImpl->db2->get("OBJECT$" + mObject, "Tpa_Toggle1") == "true";

        this->mImpl->InviteCache.put(mObject, result);

        return result;
    }

    bool TpaPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool TpaPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Tpa.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "tpa.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Tpa;

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

        this->getDatabase()->create("Blacklist");
        
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

REGISTRY_HELPER("TpaPlugin", LOICollection::Plugins::TpaPlugin, LOICollection::Plugins::TpaPlugin::getInstance())
