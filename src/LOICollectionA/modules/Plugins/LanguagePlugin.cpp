#include <atomic>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerConnectEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/Include/Plugins/LanguagePlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct LanguagePlugin::Impl {
        LRUKCache<std::string, std::string> Cache;

        std::atomic<bool> mRegistered{ false };

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerConnectEventListener;

        Impl() : Cache(100, 100) {}
    }; 

    LanguagePlugin::LanguagePlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    LanguagePlugin::~LanguagePlugin() = default;

    LanguagePlugin& LanguagePlugin::getInstance() {
        static LanguagePlugin instance;
        return instance;
    }

    ll::io::Logger* LanguagePlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void LanguagePlugin::gui::open(Player& player) {
        std::string mObjectLanguage = this->mParent.getLanguage(player);

        std::vector<std::string> keys = std::views::keys(I18nUtils::getInstance()->data)
            | std::ranges::to<std::vector<std::string>>();
        
        ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
        form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "language.gui.lang")), tr(mObjectLanguage, "name")));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), keys);
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return;

            this->mParent.set(pl, std::get<std::string>(dt->at("dropdown")));
            
            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "language.log"), pl));
        });
    }

    void LanguagePlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("language", tr({}, "commands.language.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void LanguagePlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerConnectEventListener = eventBus.emplaceListener<ll::event::PlayerConnectEvent>([this](ll::event::PlayerConnectEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string langcode = this->getLanguageCode(event.self());
            if (auto data = I18nUtils::getInstance()->data; data.find(langcode) == data.end())
                langcode = I18nUtils::getInstance()->defaultLocale;

            std::string mObject = event.self().getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            if (!this->mImpl->db->has("OBJECT$" + mObject))
                this->mImpl->db->create("OBJECT$" + mObject);

            if (!this->mImpl->db->has("OBJECT$" + mObject, "language"))
                this->mImpl->db->set("OBJECT$" + mObject, "language", langcode);

            if (!this->mImpl->db->has("OBJECT$" + mObject, "name"))
                this->mImpl->db->set("OBJECT$" + mObject, "name", event.self().getRealName());
        }, ll::event::EventPriority::High);
    }

    void LanguagePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerConnectEventListener);
    }

    std::string LanguagePlugin::getLanguageCode(Player& player) {
        std::string defaultLocale = I18nUtils::getInstance()->defaultLocale;

        if (player.isSimulatedPlayer())
            return defaultLocale;
        
        if (const std::string& langcode = player.getLocaleCode(); !langcode.empty())
            return langcode;

        return defaultLocale;
    }

    std::string LanguagePlugin::getLanguage(const std::string& mObject) {
        std::string defaultLocale = I18nUtils::getInstance()->defaultLocale;

        if (this->mImpl->Cache.contains(mObject))
            return this->mImpl->Cache.get(mObject).value();
        
        std::string langcode = this->mImpl->db->get("OBJECT$" + mObject, "language", defaultLocale);
        
        this->mImpl->Cache.put(mObject, langcode);
        return langcode;
    }

    std::string LanguagePlugin::getLanguage(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return this->getLanguage(mObject);
    }

    void LanguagePlugin::set(Player& player, const std::string& langcode) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        this->mImpl->db->set("OBJECT$" + mObject, "language", langcode);

        this->mImpl->Cache.put(mObject, langcode);
    }

    bool LanguagePlugin::load() {
        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        return true;
    }

    bool LanguagePlugin::unload() {
        this->mImpl->db.reset();
        this->mImpl->logger.reset();

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool LanguagePlugin::registry() {
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool LanguagePlugin::unregistry() {
        this->unlistenEvent();

        this->mImpl->db->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("LanguagePlugin", LOICollection::Plugins::LanguagePlugin, LOICollection::Plugins::LanguagePlugin::getInstance(), LOICollection::modules::ModulePriority::Normal)
