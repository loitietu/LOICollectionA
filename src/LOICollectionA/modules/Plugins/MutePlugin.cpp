#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <ranges>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerChatEvent.h>

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

#include "LOICollectionA/include/ServerEvents/modules/MuteEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/MutePlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    enum class MuteObject;

    constexpr inline auto MuteObjectName = ll::command::enum_name_v<MuteObject>;

    struct MutePlugin::operation {
        ll::command::SoftEnum<MuteObject> Object;

        CommandSelector<Player> Target;
        
        std::string Cause;
        int Time = 0;
        int Limit = 100;
    };

    struct MutePlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;

        std::unique_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerChatEventListener;
        ll::event::ListenerPtr MuteAddEventListener;
        ll::event::ListenerPtr MuteRemoveEventListener;
    };

    MutePlugin::MutePlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    MutePlugin::~MutePlugin() = default;

    MutePlugin& MutePlugin::getInstance() {
        static MutePlugin instance;
        return instance;
    }
    
    SQLiteStorage* MutePlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* MutePlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void MutePlugin::gui::info(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasMute(id)) {
            player.sendMessage(tr(mObjectLanguage, "mute.gui.error"));

            this->remove(player);
            return;
        }
        
        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Mute", id);

        std::string mObjectLabel = tr(mObjectLanguage, "mute.gui.info.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), 
            fmt::format(fmt::runtime(mObjectLabel), id,
                mData.at("name"),
                mData.at("cause"),
                SystemUtils::toFormatTime(mData.at("subtime"), "None"),
                SystemUtils::toFormatTime(mData.at("time"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "mute.gui.info.remove"), [this, id](Player&) -> void {
            this->mParent.delMute(id);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->remove(pl);
        });
    }

    void MutePlugin::gui::content(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.add.title"));
        form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "mute.gui.add.input1"), tr(mObjectLanguage, "mute.gui.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "mute.gui.add.input2"), tr(mObjectLanguage, "mute.gui.add.input2.placeholder"));
        form.sendTo(player, [this, &target, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->add(pl);

            std::string mCause = std::get<std::string>(dt->at("Input1"));

            if (mCause.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->add(pl);
            }

            int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            this->mParent.addMute(target, mCause, time);
        });
    }

    void MutePlugin::gui::add(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayers;
        std::vector<std::string> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid().asString());
            return true;
        });

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "mute.gui.add.title"),
            tr(mObjectLanguage, "mute.gui.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "mute.gui.error"));

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

    void MutePlugin::gui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "mute.gui.remove.title"),
            tr(mObjectLanguage, "mute.gui.remove.label"),
            this->mParent.getMutes()
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

    void MutePlugin::gui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.title"), tr(mObjectLanguage, "mute.gui.label"));
        form.appendButton(tr(mObjectLanguage, "mute.gui.addMute"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
            this->add(pl);
        });
        form.appendButton(tr(mObjectLanguage, "mute.gui.removeMute"), "textures/ui/free_download_symbol", "path", [this](Player& pl) -> void {
            this->remove(pl);
        });
        form.sendTo(player);
    }

    void MutePlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(MuteObjectName, getMutes());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("mute", tr({}, "commands.mute.description"), CommandPermissionLevel::GameDirectors, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("add").required("Target").optional("Cause").optional("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            for (Player*& pl : results) {
                if (this->isMute(*pl) || pl->getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors || pl->isSimulatedPlayer()) {
                    output.error(fmt::runtime(tr({}, "commands.mute.error.add")), pl->getRealName());
                    continue;
                }

                this->addMute(*pl, param.Cause, param.Time);

                output.success(fmt::runtime(tr({}, "commands.mute.success.add")), pl->getRealName());
            }
        });
        command.overload<operation>().text("remove").text("target").required("Target").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));
            
            for (Player*& pl : results) {
                if (!this->isMute(*pl)) {
                    output.error(fmt::runtime(tr({}, "commands.mute.error.remove")), pl->getRealName());
                    continue;
                }

                this->delMute(*pl);

                output.success(fmt::runtime(tr({}, "commands.mute.success.remove")), pl->getRealName());
            }
        });
        command.overload<operation>().text("remove").text("id").required("Object").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            if (!this->hasMute(param.Object))
                return output.error(tr({}, "commands.mute.error.remove"));

            this->delMute(param.Object);

            output.success(fmt::runtime(tr({}, "commands.mute.success.remove")), param.Object);
        });
        command.overload<operation>().text("info").required("Object").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::unordered_map<std::string, std::string> mEvent = this->getDatabase()->get("Mute", param.Object);
            
            if (mEvent.empty())
                return output.error(tr({}, "commands.mute.error.info"));

            output.success(tr({}, "commands.mute.success.info"));
            std::for_each(mEvent.begin(), mEvent.end(), [&output](const std::pair<std::string, std::string>& mPair) {
                std::string mKey = mPair.first.substr(mPair.first.find_first_of('.') + 1);

                output.success("{0}: {1}", mKey, mPair.second);
            });
        });
        command.overload<operation>().text("list").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mObjectList = this->getMutes(param.Limit);
            
            if (mObjectList.empty())
                return output.success(fmt::runtime(tr({}, "commands.mute.success.list")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.mute.success.list")), param.Limit, fmt::join(mObjectList, ", "));
        });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void MutePlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) -> void {
            std::string mId = this->getMute(event.self());

            if (mId.empty())
                return;

            std::unordered_map<std::string, std::string> mData = this->getDatabase()->get("Mute", mId);

            if (SystemUtils::isPastOrPresent(mData.at("time")))
                return this->delMute(event.self());

            std::string mObjectTips = fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(event.self()), "mute.tips")), 
                mData.at("cause"), SystemUtils::toFormatTime(mData.at("time"), "None")
            );
            
            event.self().sendMessage(mObjectTips);
            event.cancel();
        }, ll::event::EventPriority::Highest);

        this->mImpl->MuteAddEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::MuteAddEvent>([this](LOICollection::ServerEvents::MuteAddEvent& event) -> void {
            std::string mId = this->getMute(event.self());

            if (mId.empty())
                return;
            
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(MuteObjectName, { mId });
        });

        this->mImpl->MuteRemoveEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::MuteRemoveEvent>([](LOICollection::ServerEvents::MuteRemoveEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(MuteObjectName, { event.getTarget() });
        });
    }

    void MutePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);
        eventBus.removeListener(this->mImpl->MuteAddEventListener);
        eventBus.removeListener(this->mImpl->MuteRemoveEventListener);
    }

    void MutePlugin::addMute(Player& player, const std::string& cause, int time) {
        if (!this->isValid() || player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors)
            return;

        std::string mCause = cause.empty() ? "None" : cause;
        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", player.getRealName() },
            { "cause", mCause },
            { "time", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 3600, "0") : "0" },
            { "subtime", SystemUtils::getNowTime("%Y%m%d%H%M%S") },
            { "data", player.getUuid().asString() }
        };

        this->getDatabase()->set("Mute", mTimestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "mute.log1"), player)), mCause);
    }

    void MutePlugin::delMute(Player& player) {
        if (!this->isValid())
            return;

        this->delMute(this->getMute(player));
    }

    void MutePlugin::delMute(const std::string& id) {
        if (!this->isValid())
            return;

        if (!this->hasMute(id)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "MutePlugin");

            return;
        }

        this->getDatabase()->del("Mute", id);

        this->getLogger()->info(fmt::runtime(tr({}, "mute.log2")), id);
    }

    std::string MutePlugin::getMute(Player& player) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Mute", {
            { "data", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);
    }

    std::vector<std::string> MutePlugin::getMutes(int limit) {
        if (!this->isValid())
            return {};
        
        std::vector<std::string> mKeys = this->getDatabase()->list("Mute");

        return mKeys
            | std::views::take(limit > 0 ? limit : static_cast<int>(mKeys.size()))
            | std::ranges::to<std::vector<std::string>>();
    }

    bool MutePlugin::hasMute(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has("Mute", id);
    }

    bool MutePlugin::isMute(Player& player) {
        if (!this->isValid())
            return false;

        std::string mId = this->getMute(player);

        return !mId.empty();
    }

    bool MutePlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool MutePlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Mute)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "mute.db").string());
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool MutePlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool MutePlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->create("Mute", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("cause");
            ctor("time");
            ctor("subtime");
            ctor("data");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }
    
    bool MutePlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("MutePlugin", LOICollection::Plugins::MutePlugin, LOICollection::Plugins::MutePlugin::getInstance(), LOICollection::modules::ModulePriority::High)
