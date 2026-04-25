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

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/world/item/ItemStack.h>

#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <mc/safety/RedactableString.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/CdkPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct CdkPlugin::operation {
        std::string Id;
    };

    struct CdkPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;
        
        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<ll::io::Logger> logger;
    };

    CdkPlugin::CdkPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<CdkGui>(*this)) {};
    CdkPlugin::~CdkPlugin() = default;

    CdkPlugin& CdkPlugin::getInstance() {
        static CdkPlugin instance;
        return instance;
    }

    std::shared_ptr<JsonStorage> CdkPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> CdkPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void CdkPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("cdk", tr({}, "commands.cdk.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("convert").required("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->convert(player, param.Id);
                
                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.cdk.success.convert")), player.getRealName(), param.Id);
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->convert(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void CdkPlugin::create(const std::string& id, int time, bool personal) {
        if (!this->isValid())
            return;

        if (!this->getDatabase()->has(id))
            return;

        nlohmann::ordered_json data = {
            { "personal", personal },
            { "player", nlohmann::ordered_json::array() },
            { "scores", nlohmann::ordered_json::object() },
            { "item", nlohmann::ordered_json::array() },
            { "title", nlohmann::ordered_json::object() },
            { "time", SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 60, "0") }
        };

        this->getDatabase()->set(id, data);
        this->getDatabase()->save();
    }

    void CdkPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->remove(id);
        this->getDatabase()->save();
    }

    void CdkPlugin::convert(Player& player, const std::string& id) {
        if (!this->isValid()) 
            return;

        auto data = this->getDatabase()->get<nlohmann::ordered_json>(id);

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (data.is_null()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
            return;
        }

        if (SystemUtils::isPastOrPresent(data.value("time", ""))) {
            this->getDatabase()->remove(id);
            this->getDatabase()->save();

            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips1"));
            return;
        }

        std::string mUuid = player.getUuid().asString();
        if (auto it = data.value<nlohmann::ordered_json>("player", {}); std::find(it.begin(), it.end(), mUuid) != it.end()) {
            player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips2"));
            return;
        }

        for (auto elements = data.value<nlohmann::ordered_json>("title", {}); auto& element : elements.items())
            ChatPlugin::getInstance().addTitle(player, element.key(), element.value());

        for (auto elements = data.value<nlohmann::ordered_json>("scores", {}); auto& element : elements.items())
            ScoreboardUtils::addScore(player, element.key(), element.value());

        for (auto& value : data.value<nlohmann::ordered_json>("item", {})) {
            if (value.value("type", "") == "nbt") {
                ItemStack itemStack = ItemStack::fromTag(CompoundTag::fromSnbt(value.value("id", ""))->mTags);
                InventoryUtils::giveItem(player, itemStack, static_cast<int>(itemStack.mCount));
            } else {
                Bedrock::Safety::RedactableString mRedactableString;
                mRedactableString.mUnredactedString = value.value("name", "");
                
                auto itemStack = std::make_unique<ItemStack>();
                itemStack->reinit(value.value("id", ""), 0, value.value("specialvalue", 0));
                itemStack->setCustomName(mRedactableString);
                
                InventoryUtils::giveItem(player, *itemStack, value.value("quantity", 1));
            }
        }

        player.refreshInventory();
        player.sendMessage(tr(mObjectLanguage, "cdk.convert.tips3"));

        data.value("personal", false) ? static_cast<void>(this->getDatabase()->remove(id)) : data.at("player").push_back(mUuid);

        this->getDatabase()->set(id, data);
        this->getDatabase()->save();

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "cdk.log3"), player)), id);
    }

    std::vector<std::string> CdkPlugin::getCdks() {
        if (!this->isValid())
            return {};

        return this->getDatabase()->keys();
    }

    bool CdkPlugin::has(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has(id);
    }

    bool CdkPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool CdkPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Cdk)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "cdk.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool CdkPlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        return true;
    }

    bool CdkPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->registeryCommand();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool CdkPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->save();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("CdkPlugin", LOICollection::server::Plugins::CdkPlugin, LOICollection::server::Plugins::CdkPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
