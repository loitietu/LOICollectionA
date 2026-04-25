#include <atomic>
#include <memory>
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
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

#include "LOICollectionA/include/server/Events/modules/ShopEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    enum class ShopObject;

    constexpr inline auto ShopObjectName = ll::command::enum_name_v<ShopObject>;

    struct ShopPlugin::operation {
        ll::command::SoftEnum<ShopObject> Object;
    };

    struct ShopPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;

        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr ShopCreateEventListener;
        ll::event::ListenerPtr ShopDeleteEventListener;
    };

    ShopPlugin::ShopPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<ShopGui>(*this)) {};
    ShopPlugin::~ShopPlugin() = default;

    ShopPlugin& ShopPlugin::getInstance() {
        static ShopPlugin instance;
        return instance;
    }
    
    std::shared_ptr<JsonStorage> ShopPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> ShopPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void ShopPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(ShopObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("shop", tr({}, "commands.shop.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").required("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->mGui->open(player, param.Object);

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
    }

    void ShopPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->ShopCreateEventListener = eventBus.emplaceListener<LOICollection::server::Events::ShopCreateEvent>([](LOICollection::server::Events::ShopCreateEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(ShopObjectName, { event.getTarget() });
        });

        this->mImpl->ShopDeleteEventListener = eventBus.emplaceListener<LOICollection::server::Events::ShopDeleteEvent>([](LOICollection::server::Events::ShopDeleteEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(ShopObjectName, { event.getTarget() });
        });
    }

    void ShopPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->ShopCreateEventListener);
        eventBus.removeListener(this->mImpl->ShopDeleteEventListener);
    }

    void ShopPlugin::create(const std::string& id, const nlohmann::ordered_json& data) {
        if (!this->isValid())
            return;

        if (!this->getDatabase()->has(id))
            this->getDatabase()->set(id, data);
        this->getDatabase()->save();
    }

    void ShopPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->remove(id);
        this->getDatabase()->save();
    }

    bool ShopPlugin::commodity(Player& player, int number, const nlohmann::ordered_json& data, ShopType type) {
        if (!this->isValid() || data.empty())
            return false;

        if (type == ShopType::buy) {
            if (this->checkModifiedData(player, data, number)) {
                auto itemStack = std::make_unique<ItemStack>();

                if (data.contains("nbt"))
                    itemStack = std::make_unique<ItemStack>(ItemStack::fromTag(CompoundTag::fromSnbt(data.value("nbt", ""))->mTags));
                else
                    itemStack->reinit(data.value("id", ""), 1, 0);
                
                InventoryUtils::giveItem(player, *itemStack, number);
                player.refreshInventory();

                return true;
            }

            return false;
        }

        if (InventoryUtils::isItemInInventory(player, data.value("id", ""), number)) {
            nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json());
            for (auto it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                ScoreboardUtils::addScore(player, it.key(), (it.value().get<int>() * number));

            InventoryUtils::clearItem(player, data.value("id", ""), number);
            player.refreshInventory();

            return true;
        }

        return false;
    }
    
    bool ShopPlugin::title(Player& player, const nlohmann::ordered_json& data, ShopType type) {
        if (!this->isValid() || data.empty())
            return false;

        std::string id = data.value("id", "None");

        if (type == ShopType::buy) {
            if (this->checkModifiedData(player, data, 1)) {
                if (data.contains("time")) {
                    ChatPlugin::getInstance().addTitle(player, id, data.value("time", 0));
                    return true;
                }

                ChatPlugin::getInstance().addTitle(player, id, 0);
                return true;
            }

            return false;
        }

        if (ChatPlugin::getInstance().isTitle(player, id)) {
            nlohmann::ordered_json mScoreboardBase = data.value("scores", nlohmann::ordered_json());
            for (auto it = mScoreboardBase.begin(); it != mScoreboardBase.end(); ++it)
                ScoreboardUtils::addScore(player, it.key(), it.value().get<int>());

            ChatPlugin::getInstance().delTitle(player, id);
            return true;
        }

        return false;
    }

    bool ShopPlugin::checkModifiedData(Player& player, nlohmann::ordered_json data, int number) {
        if (!this->isValid() || !data.contains("scores"))
            return true;

        for (auto it = data["scores"].begin(); it != data["scores"].end(); ++it) {
            if ((it.value().get<int>() * number) > ScoreboardUtils::getScore(player, it.key()))
                return false;
        }

        for (auto it = data["scores"].begin(); it != data["scores"].end(); ++it)
            ScoreboardUtils::reduceScore(player, it.key(), (it.value().get<int>() * number));
        
        return true;
    }

    bool ShopPlugin::has(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has(id);
    }

    bool ShopPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool ShopPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Shop)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "shop.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool ShopPlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        return true;
    }

    bool ShopPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;
        
        this->registeryCommand();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool ShopPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->save();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("ShopPlugin", LOICollection::server::Plugins::ShopPlugin, LOICollection::server::Plugins::ShopPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
