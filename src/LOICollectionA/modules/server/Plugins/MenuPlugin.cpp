#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/server/Events/modules/MenuEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/CommandUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    enum class MenuObject;

    constexpr inline auto MenuObjectName = ll::command::enum_name_v<MenuObject>;

    struct MenuPlugin::operation {
        ll::command::SoftEnum<MenuObject> Object;
    };

    struct MenuPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        Config::C_Menu options;
        
        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerUseItemEventListener;
        ll::event::ListenerPtr MenuCreateEventListener;
        ll::event::ListenerPtr MenuDeleteEventListener;
    };

    MenuPlugin::MenuPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<MenuGui>(*this)) {};
    MenuPlugin::~MenuPlugin() = default;

    MenuPlugin& MenuPlugin::getInstance() {
        static MenuPlugin instance;
        return instance;
    }

    std::shared_ptr<JsonStorage> MenuPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> MenuPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void MenuPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(MenuObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("menu", tr({}, "commands.menu.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").optional("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->mGui->open(player, param.Object.empty() ? 
                    this->mImpl->options.EntranceKey : std::string(param.Object)
                );
                
                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
            });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->edit(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("clock").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            auto itemStack = std::make_unique<ItemStack>();
            itemStack->reinit(this->mImpl->options.MenuItemId, 1, 0);
            
            if (!itemStack || itemStack->isNull())
                return output.error(tr(origin.getLocaleCode(), "commands.menu.error.item.null"));
            if (InventoryUtils::isItemInInventory(player, this->mImpl->options.MenuItemId, 1))
                return output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.menu.error.item.give")), player.getRealName());

            InventoryUtils::giveItem(player, *itemStack, 1);
            player.refreshInventory();

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void MenuPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([this](ll::event::PlayerUseItemEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            if (event.item().getTypeName() == this->mImpl->options.MenuItemId)
                this->mGui->open(event.self(), "main");
        });

        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            auto itemStack = std::make_unique<ItemStack>();
            itemStack->reinit(this->mImpl->options.MenuItemId, 1, 0);

            if (!itemStack || InventoryUtils::isItemInInventory(event.self(), this->mImpl->options.MenuItemId, 1))
                return;
            InventoryUtils::giveItem(event.self(), *itemStack, 1);
            event.self().refreshInventory();
        });

        this->mImpl->MenuCreateEventListener = eventBus.emplaceListener<LOICollection::server::Events::MenuCreateEvent>([](LOICollection::server::Events::MenuCreateEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(MenuObjectName, { event.getTarget() });
        });

        this->mImpl->MenuDeleteEventListener = eventBus.emplaceListener<LOICollection::server::Events::MenuDeleteEvent>([](LOICollection::server::Events::MenuDeleteEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(MenuObjectName, { event.getTarget() });
        });
    }

    void MenuPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerUseItemEventListener);
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->MenuCreateEventListener);
        eventBus.removeListener(this->mImpl->MenuDeleteEventListener);
    }

    void MenuPlugin::create(const std::string& id, const nlohmann::ordered_json& data) {
        if (!this->isValid())
            return;

        if (!this->getDatabase()->has(id))
            this->getDatabase()->set(id, data);
        this->getDatabase()->save();
    }

    void MenuPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->remove(id);
        this->getDatabase()->save();
    }

    void MenuPlugin::handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original) {
        if (!this->isValid() || action.empty())
            return;

        if (action.contains("permission")) {
            if (static_cast<int>(player.getCommandPermissionLevel()) < action["permission"])
                return CommandUtils::executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("permission", ""));
        }

        if (action.contains("scores")) {
            for (const auto& [key, value] : action["scores"].items()) {
                if (value.get<int>() > ScoreboardUtils::getScore(player, key))
                    return CommandUtils::executeCommand(player, original.value("info", nlohmann::ordered_json{}).value("score", ""));
            }
            for (const auto& [key, value] : action["scores"].items())
                ScoreboardUtils::reduceScore(player, key, value.get<int>());
        }

        if (action.value("type", "") == "button") {
            if (action["run"].is_string())
                return CommandUtils::executeCommand(player, action["run"].get<std::string>());
            
            for (const auto& cmd : action["run"])
                CommandUtils::executeCommand(player, cmd.get<std::string>());
            
            return;
        }

        this->mGui->open(player, action.value("run", ""));
    }

    bool MenuPlugin::has(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has(id);
    }

    bool MenuPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool MenuPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Menu.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "menu.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Menu;

        return true;
    }

    bool MenuPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool MenuPlugin::registry() {     
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool MenuPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->unlistenEvent();

        this->getDatabase()->save();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("MenuPlugin", LOICollection::server::Plugins::MenuPlugin, LOICollection::server::Plugins::MenuPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
