#include <atomic>
#include <memory>
#include <string>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/Expected.h>
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

    std::shared_ptr<CdkPlugin> CdkPlugin::getShared() {
        static auto instance = std::shared_ptr<CdkPlugin>(new CdkPlugin());
        return instance;
    }

    std::error_code CdkPlugin::makeErrorCode(CdkPluginErrorCode e) {
        static CdkPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
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
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                this->convert(player, param.Id)
                    .or_else([](ll::Error e) -> ll::Expected<void> {
                        if (e.isA<ll::ErrorCodeError>() 
                            && (e.as<ll::ErrorCodeError>().ec == makeErrorCode(CdkPluginErrorCode::NotFound)
                                || e.as<ll::ErrorCodeError>().ec == makeErrorCode(CdkPluginErrorCode::Received)))
                            return {};

                        return ll::Unexpected(e);
                    })
                    .transform([&output, &origin, name = player.getRealName(), id = param.Id]() -> void {
                        output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.cdk.success.convert")), name, id);
                    })
                    .or_else(modules::defaultErrorHandler<CdkPlugin>);
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->convert(player).or_else(modules::defaultErrorHandler<CdkPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));
            
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player).or_else(modules::defaultErrorHandler<CdkPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    ll::Expected<void> CdkPlugin::create(const std::string& id, int time, bool personal) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Invalid));

        if (this->getDatabase()->has(id))
            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Exists));

        nlohmann::ordered_json data = {
            { "personal", personal },
            { "player", nlohmann::ordered_json::array() },
            { "scores", nlohmann::ordered_json::object() },
            { "item", nlohmann::ordered_json::array() },
            { "title", nlohmann::ordered_json::object() },
            { "time", SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 60, "0") }
        };

        this->getDatabase()->set(id, data);
        
        return this->getDatabase()->save();
    }

    ll::Expected<void> CdkPlugin::remove(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Invalid));

        this->getDatabase()->remove(id);

        return this->getDatabase()->save();
    }

    ll::Expected<void> CdkPlugin::convert(Player& player, const std::string& id) {
        if (!this->isValid()) 
            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Invalid));

        return LanguagePlugin::getShared()->getLanguage(player)
            .and_then([this, id, &player](const std::string& language) -> ll::Expected<void> {
                return this->getDatabase()->get<nlohmann::ordered_json>(id)
                    .and_then([this, id, language, &player](nlohmann::ordered_json data) -> ll::Expected<void> {
                        if (data.is_null()) {
                            player.sendMessage(tr(language, "cdk.convert.tips1"));

                            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::NotFound));
                        }

                        if (SystemUtils::isPastOrPresent(data.value("time", ""))) {
                            player.sendMessage(tr(language, "cdk.convert.tips1"));

                            this->getDatabase()->remove(id);

                            return this->getDatabase()->save();
                        }

                        std::string mUuid = player.getUuid().asString();
                        if (auto it = data.value<nlohmann::ordered_json>("player", {}); std::find(it.begin(), it.end(), mUuid) != it.end()) {
                            player.sendMessage(tr(language, "cdk.convert.tips2"));

                            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Received));
                        }

                        for (auto elements = data.value<nlohmann::ordered_json>("title", {}); auto& element : elements.items()) {
                            auto result = ChatPlugin::getShared()->addTitle(player, element.key(), element.value());
                            if (!result.has_value()) {
                                if (!result.error().isA<ll::ErrorCodeError>() || result.error().as<ll::ErrorCodeError>().ec != ChatPlugin::makeErrorCode(ChatPluginErrorCode::Invalid))
                                    return ll::Unexpected(result.error());
                            }
                        }

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
                                itemStack->reinit(value.value("id", ""), 1, value.value("specialvalue", 0));
                                itemStack->setCustomName(mRedactableString);
                                
                                InventoryUtils::giveItem(player, *itemStack, value.value("quantity", 1));
                            }
                        }

                        player.refreshInventory();
                        player.sendMessage(tr(language, "cdk.convert.tips3"));

                        if (data.value("personal", false))
                            this->getDatabase()->remove(id);
                        else {
                            data.at("player").push_back(mUuid);

                            this->getDatabase()->set(id, data);
                        }

                        return this->getDatabase()->save()
                            .transform([this, id, &player]() -> void {
                                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "cdk.log3"), player)), id);
                            });
                    });
            });
    }

    ll::Expected<std::vector<std::string>> CdkPlugin::getCdks() {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Invalid));

        return this->getDatabase()->keys();
    }

    ll::Expected<bool> CdkPlugin::has(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(CdkPluginErrorCode::Invalid));

        return this->getDatabase()->has(id);
    }

    bool CdkPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    std::string CdkPlugin::getName() {
        return "CdkPlugin";
    }

    modules::ModulePriority CdkPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> CdkPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Cdk)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "cdk.json");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return this->mImpl->db->load()
            .transform([]() -> bool {
                return true;
            });
    }

    ll::Expected<bool> CdkPlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        return true;
    }

    ll::Expected<bool> CdkPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->registeryCommand();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    ll::Expected<bool> CdkPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        return this->getDatabase()->save()
            .transform([this]() -> bool {
                this->mImpl->mRegistered.store(false, std::memory_order_release);

                return true;
            });
    }
}
