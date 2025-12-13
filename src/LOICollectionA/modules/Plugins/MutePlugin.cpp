#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <execution>
#include <filesystem>
#include <functional>
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

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

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
        std::vector<std::function<void(const std::string&)>> onMuteAdds;
        std::vector<std::function<void(const std::string&)>> onMuteDels;

        bool ModuleEnabled = false;

        std::unique_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerChatEventListener;
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
        
        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->getByPrefix("Mute", id + ".");

        std::string mObjectLabel = tr(mObjectLanguage, "mute.gui.info.label");

        ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), 
            fmt::format(fmt::runtime(mObjectLabel), id,
                mData.at(id + ".NAME"),
                mData.at(id + ".CAUSE"),
                SystemUtils::toFormatTime(mData.at(id + ".SUBTIME"), "None"),
                SystemUtils::toFormatTime(mData.at(id + ".TIME"), "None")
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

        ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.add.title"), tr(mObjectLanguage, "mute.gui.add.label"));
        ll::service::getLevel()->forEachPlayer([this, &form](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->content(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void MutePlugin::gui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), tr(mObjectLanguage, "mute.gui.remove.label"));
        for (std::string& mItem : this->mParent.getMutes()) {
            form.appendButton(mItem, [this, mItem](Player& pl) -> void {
                this->info(pl, mItem);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
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
        ll::command::CommandRegistrar::getInstance().tryRegisterSoftEnum(MuteObjectName, getMutes());

        this->onMuteAdd([](const std::string& id) {
            ll::command::CommandRegistrar::getInstance().addSoftEnumValues(MuteObjectName, { id });
        });
        this->onMuteDel([](const std::string& id) {
            ll::command::CommandRegistrar::getInstance().removeSoftEnumValues(MuteObjectName, { id });
        });

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
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
            if (!this->getDatabase()->hasByPrefix("Mute", param.Object + ".", 5))
                return output.error(tr({}, "commands.mute.error.remove"));

            this->delMute(param.Object);

            output.success(fmt::runtime(tr({}, "commands.mute.success.remove")), param.Object);
        });
        command.overload<operation>().text("info").required("Object").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::unordered_map<std::string, std::string> mEvent = this->getDatabase()->getByPrefix("Mute", param.Object + ".");
            
            if (mEvent.empty())
                return output.error(tr({}, "commands.mute.error.info"));

            output.success(tr({}, "commands.mute.success.info"));
            std::for_each(mEvent.begin(), mEvent.end(), [&output, id = param.Object](const std::pair<std::string, std::string>& mPair) {
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

            std::unordered_map<std::string, std::string> mData = this->getDatabase()->getByPrefix("Mute", mId + ".");

            if (SystemUtils::isPastOrPresent(mData.at(mId + ".TIME")))
                return this->delMute(event.self());

            std::string mObjectTips = fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(event.self()), "mute.tips")), 
                mData.at(mId + ".CAUSE"), SystemUtils::toFormatTime(mData.at(mId + ".TIME"), "None")
            );
            
            event.self().sendMessage(mObjectTips);
            event.cancel();
        }, ll::event::EventPriority::Highest);
    }

    void MutePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);
    }

    void MutePlugin::addMute(Player& player, const std::string& cause, int time) {
        if (!this->isValid() || player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors)
            return;

        std::string mCause = cause.empty() ? "None" : cause;
        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        SQLiteStorageTransaction transaction(*this->getDatabase());
        auto connection = transaction.connection();

        this->getDatabase()->set(connection, "Mute", mTimestamp + ".NAME", player.getRealName());
        this->getDatabase()->set(connection, "Mute", mTimestamp + ".CAUSE", mCause);
        this->getDatabase()->set(connection, "Mute", mTimestamp + ".TIME", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time, "0") : "0");
        this->getDatabase()->set(connection, "Mute", mTimestamp + ".SUBTIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));
        this->getDatabase()->set(connection, "Mute", mTimestamp + ".DATA", player.getUuid().asString());

        transaction.commit();

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "mute.log1"), player)), mCause);

        for (auto& fn : this->mImpl->onMuteAdds)
            fn(mTimestamp);
    }

    void MutePlugin::delMute(Player& player) {
        if (!this->isValid())
            return;

        this->delMute(this->getMute(player));
    }

    void MutePlugin::delMute(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->delByPrefix("Mute", id + ".");

        this->getLogger()->info(fmt::runtime(tr({}, "mute.log2")), id);

        for (auto& fn : this->mImpl->onMuteDels)
            fn(id);
    }

    void MutePlugin::onMuteAdd(std::function<void(const std::string&)> fn) {
        this->mImpl->onMuteAdds.push_back(fn);
    }

    void MutePlugin::onMuteDel(std::function<void(const std::string&)> fn) {
        this->mImpl->onMuteDels.push_back(fn);
    }

    std::string MutePlugin::getMute(Player& player) {
        if (!this->isValid())
            return {};

        std::string mUuid = player.getUuid().asString();

        std::vector<std::string> mKeys = this->getMutes();
        auto it = std::find_if(std::execution::par, mKeys.begin(), mKeys.end(), [this, &mUuid](const std::string& mId) -> bool {
            return this->getDatabase()->get("Mute", mId + ".DATA") == mUuid;
        });

        return it != mKeys.end() ? *it : "";
    }

    std::vector<std::string> MutePlugin::getMutes(int limit) {
        if (!this->isValid())
            return {};
        
        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Mute", "%.NAME");

        std::vector<std::string> mResult(mKeys.size());
        std::transform(mKeys.begin(), mKeys.end(), mResult.begin(), [](const std::string& mKey) -> std::string {
            size_t mPos = mKey.find('.');
            return mPos != std::string::npos ? mKey.substr(0, mPos) : "";
        });

        if (limit > 0 && (int) mResult.size() > limit)
            mResult.resize(limit);

        return mResult;
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
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Mute)
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

        return true;
    }

    bool MutePlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->create("Mute");
        
        this->registeryCommand();
        this->listenEvent();

        return true;
    }
    
    bool MutePlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        return true;
    }
}

REGISTRY_HELPER("MutePlugin", LOICollection::Plugins::MutePlugin, LOICollection::Plugins::MutePlugin::getInstance())
