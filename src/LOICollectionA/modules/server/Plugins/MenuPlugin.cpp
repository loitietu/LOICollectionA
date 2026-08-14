#include <atomic>
#include <memory>
#include <string>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/CommandUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/include/form/GUIManager.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct MenuPlugin::operation {
        std::string Id;
    };

    struct MenuPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        Config::C_Menu options;

        bool ModuleEnabled = false;

        std::shared_ptr<ll::io::Logger> logger;

        std::filesystem::path mConfigPath;
        
        std::string mGuiFile;

        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerUseItemEventListener;
    };

    MenuPlugin::MenuPlugin() : mImpl(std::make_unique<Impl>()) {};
    MenuPlugin::~MenuPlugin() = default;

    std::shared_ptr<MenuPlugin> MenuPlugin::getShared() {
        static auto instance = std::shared_ptr<MenuPlugin>(new MenuPlugin());
        return instance;
    }

    std::error_code MenuPlugin::makeErrorCode(MenuPluginErrorCode e) {
        static MenuPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }

    std::shared_ptr<ll::io::Logger> MenuPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void MenuPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("menu", tr({}, "commands.menu.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").optional("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->open(player, param.Id.empty() ? this->mImpl->options.EntranceKey : param.Id)
                    .or_else(modules::defaultErrorHandler<MenuPlugin>);

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
            });
        command.overload().text("clock").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
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
        command.overload().text("reload").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

            output.success(tr(origin.getLocaleCode(), "commands.generic.reload"));
            
            this->registeryUI()
                .transform([&origin, &output]() -> void {
                    output.success(tr(origin.getLocaleCode(), "commands.generic.reload.success"));
                })
                .or_else(modules::defaultErrorHandler<MenuPlugin>);
        });
    }

    ll::Expected<void> MenuPlugin::registeryUI() {
        auto resolve = [this](const std::string& file) -> std::filesystem::path {
            std::filesystem::path path(file);
            return path.is_absolute() ? path : this->mImpl->mConfigPath / path;
        };

        return form::GUIManager::getInstance().load("menu", resolve(this->mImpl->mGuiFile).string());
    }

    ll::Expected<void> MenuPlugin::open(Player& player, const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::Invalid));

        return form::GUIManager::getInstance().open("menu", id, player);
    }

    void MenuPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([this](ll::event::PlayerUseItemEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            if (event.item().getTypeName() == this->mImpl->options.MenuItemId)
                this->open(event.self(), this->mImpl->options.EntranceKey).or_else(modules::defaultErrorHandler<MenuPlugin>);
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
    }

    void MenuPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerUseItemEventListener);
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
    }

    ll::Expected<MenuActionResult> MenuPlugin::handleAction(Player& player, const MenuItemData& action, [[maybe_unused]] const MenuData& original) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(MenuPluginErrorCode::Invalid));

        if (action.type.empty())
            return MenuActionResult::Success;

        if (static_cast<int>(player.getCommandPermissionLevel()) < action.permission)
            return MenuActionResult::PermissionDenied;

        for (const auto& score : action.scores) {
            if (score.value > ScoreboardUtils::getScore(player, score.objective))
                return MenuActionResult::InsufficientScore;
        }

        for (const auto& score : action.scores)
            ScoreboardUtils::reduceScore(player, score.objective, score.value);

        if (action.type == "button") {
            for (const auto& command : action.run)
                CommandUtils::executeCommand(player, command);
        }

        return MenuActionResult::Success;
    }

    bool MenuPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->ModuleEnabled;
    }

    std::string MenuPlugin::getName() {
        return "MenuPlugin";
    }

    modules::ModulePriority MenuPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> MenuPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Menu.ModuleEnabled)
            return false;

        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Menu;
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;
        this->mImpl->mConfigPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());
        this->mImpl->mGuiFile = this->mImpl->options.GuiPath;

        return true;
    }

    ll::Expected<bool> MenuPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->logger.reset();
        this->mImpl->options = {};
        this->mImpl->ModuleEnabled = false;

        return true;
    }

    ll::Expected<bool> MenuPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        return this->registeryUI()
            .transform([this]() -> bool {
                this->registeryCommand();
                this->listenEvent();

                this->mImpl->mRegistered.store(true, std::memory_order_release);

                return true;
            });
    }

    ll::Expected<bool> MenuPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}
