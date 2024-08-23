#include <memory>
#include <filesystem>

#include <ll/api/Config.h>
#include <ll/api/io/FileUtils.h>
#include <ll/api/data/KeyValueDB.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/Mod/RegisterHelper.h>

#include "LangPlugin.h"
#include "Utils/toolUtils.h"
#include "Utils/JsonUtils.h"
#include "Utils/I18nUtils.h"

#include "Include/language.h"
#include "Include/blacklist.h"

#include "LOICollectionA.h"

namespace LOICollection {
    static std::unique_ptr<A> instance;

    A& A::getInstance() { return *instance; }

    bool A::load() {
        auto& logger = mSelf.getLogger();
        const auto& dataFilePath = this->mSelf.getDataDir();
        const auto& configFilePath = this->mSelf.getConfigDir() / "config.json";

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
        this->LanguageDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "language");
        this->BlacklistDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "blacklist");
        this->MuteDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "mute");
        this->TpaDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "tpa");
        this->PvpDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "pvp");
        this->ChatDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "chat");
        this->MarketDB = std::make_unique<ll::data::KeyValueDB>(dataFilePath / "market");
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
        return true;
    }

    bool A::enable() {
        languagePlugin::registery(&this->LanguageDB);
        if (this->config.Blacklist) blacklistPlugin::registery(&this->BlacklistDB);
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
