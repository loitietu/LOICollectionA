#include <memory>
#include <filesystem>

#include <ll/api/Config.h>
#include <ll/api/io/FileUtils.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/Mod/RegisterHelper.h>

#include "LangPlugin.h"
#include "Utils/toolUtils.h"
#include "Utils/I18nUtils.h"
#include "Utils/JsonUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/plugin/HookPlugin.h"
#include "Include/plugin/languagePlugin.h"
#include "Include/plugin/blacklistPlugin.h"
#include "Include/plugin/mutePlugin.h"
#include "Include/plugin/cdkPlugin.h"

#include "LOICollectionA.h"

namespace LOICollection {
    static std::unique_ptr<A> instance;

    A& A::getInstance() { return *instance; }

    bool A::load() {
        auto& logger = mSelf.getLogger();
        const auto& dataFilePath = this->mSelf.getDataDir();
        const auto& configFilePath = this->mSelf.getConfigDir() / "config.json";

        logger.setFile("./logs/LOICollectionA.log", false);

        toolUtils::init(&mSelf);
        toolUtils::SynchronousPluginConfigVersion(&this->config);
        logger.info("Loading LOICollection - A (Version {})", toolUtils::getVersion());
        if (!std::filesystem::exists(configFilePath)) {
            logger.info("Configurations not found.");
            logger.info("Saving default configurations.");
            if (!ll::config::saveConfig(config, configFilePath)) {
                logger.error("Failed to save default configurations.");
                return false;
            }
        }
        if (!ll::config::loadConfig(config, configFilePath)) {
            logger.info("Update configurations.");
        }

        logger.info("Initialization of configurations completed.");
        std::filesystem::create_directory(dataFilePath);
        this->LanguageDB = std::make_unique<SQLiteStorage>(dataFilePath / "language.db");
        this->BlacklistDB = std::make_unique<SQLiteStorage>(dataFilePath / "blacklist.db");
        this->MuteDB = std::make_unique<SQLiteStorage>(dataFilePath / "mute.db");
        this->TpaDB = std::make_unique<SQLiteStorage>(dataFilePath / "tpa.db");
        this->PvpDB = std::make_unique<SQLiteStorage>(dataFilePath / "pvp.db");
        this->ChatDB = std::make_unique<SQLiteStorage>(dataFilePath / "chat.db");
        this->MarketDB = std::make_unique<SQLiteStorage>(dataFilePath / "market.db");
        this->CdkDB = std::make_unique<JsonUtils>(dataFilePath / "cdk.json");
        this->MenuDB = std::make_unique<JsonUtils>(dataFilePath / "menu.json");
        this->ShopDB = std::make_unique<JsonUtils>(dataFilePath / "shop.json");
        this->AnnounCementDB = std::make_unique<JsonUtils>(dataFilePath / "announcement.json");
        logger.info("Initialization of database file completed.");

        if (this->config.language.update) {
            ll::file_utils::writeFile(this->mSelf.getConfigDir() / "language.json", defaultLangData.dump(4));
        }
        I18nUtils::load(mSelf.getConfigDir() / "language.json");
        logger.info("Initialization of language file completed.");

        HookPlugin::setFakeSeed(this->config.FakeSeed);
        logger.info("Initialization of HookPlugin completed.");
        return true;
    }

    bool A::enable() {
        HookPlugin::registery();
        languagePlugin::registery(&this->LanguageDB);
        if (this->config.Blacklist) blacklistPlugin::registery(&this->BlacklistDB);
        if (this->config.Mute) mutePlugin::registery(&this->MuteDB);
        if (this->config.Cdk) cdkPlugin::registery(&this->CdkDB);
        this->mSelf.getLogger().info("Register Event completed.");
        return true;
    }

    bool A::disable() {
        languagePlugin::unregistery();
        if (this->config.Blacklist) blacklistPlugin::unregistery();
        this->mSelf.getLogger().info("Unregister Event completed.");
        return true;
    }
}

LL_REGISTER_MOD(LOICollection::A, LOICollection::instance);
