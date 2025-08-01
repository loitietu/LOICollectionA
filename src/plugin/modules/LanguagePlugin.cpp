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
#include <ll/api/utils/StringUtils.h>

#include <mc/certificates/WebToken.h>
#include <mc/network/ConnectionRequest.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "include/APIUtils.h"

#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "Include/LanguagePlugin.h"

using I18nUtilsTools::tr;

std::vector<std::string> keys() {
    std::vector<std::string> keys;
    for (const auto& item : I18nUtils::getInstance()->data)
        keys.push_back(item.first);
    return keys;
}

namespace LOICollection::Plugins::language {
    std::shared_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
            form.appendLabel(ll::string_utils::replaceAll(
                tr(mObjectLanguage, "language.gui.lang"), "${language}", tr(mObjectLanguage, "name")
            ));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), keys());
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return;

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                db->set("OBJECT$" + mObject, "language", std::get<std::string>(dt->at("dropdown")));
                
                logger->info(LOICollectionAPI::translateString(tr({}, "language.log"), pl));
            });
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("language", tr({}, "commands.language.description"), CommandPermissionLevel::Any);
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                std::string langcode = getLanguageCode(event.self());
                if (auto data = I18nUtils::getInstance()->data; data.find(langcode) == data.end())
                    langcode = I18nUtils::getInstance()->defaultLocale;

                std::string mObject = event.self().getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');

                if (!db->has("OBJECT$" + mObject))
                    db->create("OBJECT$" + mObject);
                if (!db->has("OBJECT$" + mObject, "language"))
                    db->set("OBJECT$" + mObject, "language", langcode);
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
        }
    }

    std::string getLanguageCode(Player& player) {
        std::string defaultLocale = I18nUtils::getInstance()->defaultLocale;

        if (player.isSimulatedPlayer())
            return defaultLocale;

        if (auto request = player.getConnectionRequest(); request)
            return request->mRawToken->mDataInfo.get("LanguageCode", defaultLocale).asString(defaultLocale);
        return defaultLocale;
    }

    std::string getLanguage(const std::string& mObject) {
        std::string defaultLocale = I18nUtils::getInstance()->defaultLocale;
        
        return db->get("OBJECT$" + mObject, "language", defaultLocale);
    }

    std::string getLanguage(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        return getLanguage(mObject);
    }

    void registery(void* database) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(database);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}