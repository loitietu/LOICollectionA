#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/Mod/RegisterHelper.h>

#include "LOICollectionA/utils/SystemUtils.h"
#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/include/APIUtils.h"

#include "LOICollectionA/include/ModManager.h"
#include "LOICollectionA/include/ModRegistry.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/LOICollectionA.h"

namespace LOICollection {
    A& A::getInstance() {
        static A instance;
        return instance;
    }

    bool A::load() {
        ll::io::Logger& logger = this->mSelf.getLogger();
        const std::filesystem::path& dataFilePath = this->mSelf.getDataDir();
        const std::filesystem::path& langFilePath = this->mSelf.getLangDir();
        const std::filesystem::path& configDataPath = this->mSelf.getConfigDir();
        const std::filesystem::path& configFilePath = configDataPath / "config.json";

        Config::SynchronousPluginConfigVersion(this->config);
        logger.info("Loading LOICollection - A (Version {})", Config::GetVersion());
        logger.info("Protocol - Mojang Eula (https://account.mojang.com/documents/minecraft_eula)");
        
        if (!std::filesystem::exists(configFilePath)) {
            logger.info("Plugin - Configurations not found.");
            logger.info("Plugin - Saving default configurations.");
            if (!ll::config::saveConfig(this->config, configFilePath)) {
                logger.error("Failed to save default configurations.");
                return false;
            }
        }
        Config::SynchronousPluginConfigType(this->config, configFilePath.string());
        if (!ll::config::loadConfig(this->config, configFilePath)) {
            logger.info("Plugin - Update configurations.");
            if (!ll::config::saveConfig(this->config, configFilePath)) {
                logger.error("Failed to save default configurations.");
                return false;
            }
        }
        
        ServiceProvider::getInstance().registerInstance<ReadOnlyWrapper<C_Config>>(
            std::make_shared<ReadOnlyWrapper<C_Config>>(this->config), "Config"
        );

        logger.info("Initialization of configurations completed.");

        std::filesystem::create_directory(dataFilePath);
        ServiceProvider::getInstance().registerInstance<std::string>(std::make_shared<std::string>(dataFilePath.string()), "DataPath");
        ServiceProvider::getInstance().registerInstance<std::string>(std::make_shared<std::string>(configDataPath.string()), "ConfigPath");
        ServiceProvider::getInstance().registerSingleton<SQLiteStorage>([dataFilePath]() {
            return std::make_shared<SQLiteStorage>((dataFilePath / "settings.db").string());
        }, "SettingsDB");

        std::vector<std::string> mMods = modules::ModManager::getInstance().mods();
        std::for_each(mMods.begin(), mMods.end(), [&logger](const std::string& mod) -> void {
            modules::ModRegistry* mRegistry = modules::ModManager::getInstance().getRegistry(mod);

            if (!mRegistry) {
                logger.error("Failed to get mod registry for mod {}", mod);
                return;
            }

            mRegistry->onLoad();
        });

        logger.info("Initialization of database completed.");
        
        std::filesystem::create_directory(langFilePath);
        I18nUtils::getInstance() = std::make_unique<I18nUtils>(langFilePath.string());
        I18nUtils::getInstance()->defaultLocale = this->config.ConsoleLanguage == "system" ?
            SystemUtils::getSystemLocaleCode("zh_CN") : this->config.ConsoleLanguage;
        logger.info("Initialization of language completed.");

        logger.info("Initialization of plugins completed.");

        return true;
    }

    bool A::enable() {
        LOICollectionAPI::initialization();

        ll::io::Logger& logger = this->mSelf.getLogger();

        std::vector<std::string> mMods = modules::ModManager::getInstance().mods();
        std::for_each(mMods.begin(), mMods.end(), [&logger](const std::string& mod) -> void {
            modules::ModRegistry* mRegistry = modules::ModManager::getInstance().getRegistry(mod);

            if (!mRegistry) {
                logger.error("Failed to get mod registry for mod {}", mod);
                return;
            }

            mRegistry->onRegistry();
        });

        return true;
    }

    bool A::disable() {
        ll::io::Logger& logger = this->mSelf.getLogger();

        std::vector<std::string> mMods = modules::ModManager::getInstance().mods();
        std::for_each(mMods.begin(), mMods.end(), [&logger](const std::string& mod) -> void {
            modules::ModRegistry* mRegistry = modules::ModManager::getInstance().getRegistry(mod);

            if (!mRegistry) {
                logger.error("Failed to get mod registry for mod {}", mod);
                return;
            }

            mRegistry->onUnregistry();
        });

        return true;
    }
}

LL_REGISTER_MOD(LOICollection::A, LOICollection::A::getInstance());
