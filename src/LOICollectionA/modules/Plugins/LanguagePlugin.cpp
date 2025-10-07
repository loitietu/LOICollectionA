#include <memory>
#include <string>

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
#include <ll/api/event/player/PlayerJoinEvent.h>

#include <mc/certificates/WebToken.h>
#include <mc/network/ConnectionRequest.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/Include/Plugins/LanguagePlugin.h"

using I18nUtilsTools::tr;

std::vector<std::string> keys() {
    std::vector<std::string> keys;
    for (const auto& item : I18nUtils::getInstance()->data)
        keys.push_back(item.first);
    return keys;
}

namespace LOICollection::Plugins {
    struct LanguagePlugin::Impl {
        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
    }; 

    LanguagePlugin::LanguagePlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    LanguagePlugin::~LanguagePlugin() = default;

    ll::io::Logger* LanguagePlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void LanguagePlugin::gui::open(Player& player) {
        std::string mObjectLanguage = this->mParent.getLanguage(player);
        
        ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
        form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "language.gui.lang")), tr(mObjectLanguage, "name")));
        form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), keys());
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return;

            std::string mObject = pl.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            this->mParent.mImpl->db->set("OBJECT$" + mObject, "language", std::get<std::string>(dt->at("dropdown")));
            
            this->mParent.getLogger()->info(LOICollectionAPI::getVariableString(tr({}, "language.log"), pl));
        });
    }

    void LanguagePlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("language", tr({}, "commands.language.description"), CommandPermissionLevel::Any);
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
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
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
        });
    }

    void LanguagePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
    }

    std::string LanguagePlugin::getLanguageCode(Player& player) {
        std::string defaultLocale = I18nUtils::getInstance()->defaultLocale;

        if (player.isSimulatedPlayer())
            return defaultLocale;

        if (auto request = player.getConnectionRequest(); request)
            return request->mRawToken->mDataInfo.get("LanguageCode", defaultLocale).asString(defaultLocale);
        return defaultLocale;
    }

    std::string LanguagePlugin::getLanguage(const std::string& mObject) {
        std::string defaultLocale = I18nUtils::getInstance()->defaultLocale;
        
        return this->mImpl->db->get("OBJECT$" + mObject, "language", defaultLocale);
    }

    std::string LanguagePlugin::getLanguage(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return this->getLanguage(mObject);
    }

    bool LanguagePlugin::load() {
        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        return true;
    }

    bool LanguagePlugin::registry() {
        this->registeryCommand();
        this->listenEvent();

        return true;
    }

    bool LanguagePlugin::unregistry() {
        this->unlistenEvent();

        this->mImpl->db->exec("VACUUM;");

        return true;
    }
}

REGISTRY_HELPER("LanguagePlugin", LOICollection::Plugins::LanguagePlugin, LOICollection::Plugins::LanguagePlugin::getInstance())
