#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <filesystem>

#include <ll/api/Config.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/Mod/RegisterHelper.h>

#include "Utils/toolUtils.h"
#include "Utils/I18nUtils.h"
#include "Utils/JsonUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/APIUtils.h"
#include "Include/HookPlugin.h"
#include "Include/languagePlugin.h"
#include "Include/blacklistPlugin.h"
#include "Include/mutePlugin.h"
#include "Include/cdkPlugin.h"
#include "Include/menuPlugin.h"
#include "Include/tpaPlugin.h"
#include "Include/shopPlugin.h"
#include "Include/monitorPlugin.h"
#include "Include/pvpPlugin.h"
#include "Include/walletPlugin.h"
#include "Include/chatPlugin.h"
#include "Include/acPlugin.h"
#include "Include/marketPlugin.h"

#include "LangPlugin.h"

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
        
        try {
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
                if (!ll::config::saveConfig(config, configFilePath)) {
                    logger.error("Failed to save default configurations.");
                    return false;
                }
            }
        } catch (...) {
            logger.warn("Configurations are corrupted.");
            ll::config::saveConfig(config, configFilePath);
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
            JsonUtils mObjectLanguage(this->mSelf.getConfigDir() / "language.json");
            mObjectLanguage.set("zh_CN", CNLangData);
            mObjectLanguage.save();
        }
        I18nUtils::load(mSelf.getConfigDir() / "language.json");
        logger.info("Initialization of language file completed.");

        HookPlugin::setFakeSeed(this->config.FakeSeed);
        logger.info("Initialization of Hook completed.");
        return true;
    }

    bool A::enable() {
        LOICollectionAPI::initialization();

        HookPlugin::registery();
        languagePlugin::registery(&this->LanguageDB);
        if (this->config.Blacklist) blacklistPlugin::registery(&this->BlacklistDB);
        if (this->config.Mute) mutePlugin::registery(&this->MuteDB);
        if (this->config.Cdk) cdkPlugin::registery(&this->CdkDB);
        if (this->config.Menu.Enable) menuPlugin::registery(&this->MenuDB, this->config.Menu.ItemId);
        if (this->config.Tpa) tpaPlugin::registery(&this->TpaDB);
        if (this->config.Shop) shopPlugin::registery(&this->ShopDB);
        if (this->config.Monitor.Enable) {
            std::map<std::string, std::variant<std::string, std::vector<std::string>>> options;
            options["join"] = this->config.Monitor.join;
            options["target"] = this->config.Monitor.target;
            options["changed"] = this->config.Monitor.changed;
            options["command"] = this->config.Monitor.command;
            options["tips"] = this->config.Monitor.tips;
            monitorPlugin::registery(options);
        }
        if (this->config.Pvp) pvpPlugin::registery(&this->PvpDB);
        if (this->config.Wallet.Enable) {
            std::map<std::string, std::variant<std::string, double>> options;
            options["score"] = this->config.Wallet.score;
            options["tax"] = this->config.Wallet.tax;
            walletPlugin::registery(options);
        }
        if (this->config.Chat.Enable) chatPlugin::registery(&this->ChatDB, this->config.Chat.chat);
        if (this->config.AnnounCement) announcementPlugin::registery(&this->AnnounCementDB);
        if (this->config.Market.Enable) {
            std::map<std::string, std::string> options;
            options["score"] = this->config.Market.score;
            marketPlugin::registery(&this->MarketDB, options);
        }
        this->mSelf.getLogger().info("Register Event completed.");
        return true;
    }

    bool A::disable() {
        languagePlugin::unregistery();
        if (this->config.Blacklist) blacklistPlugin::unregistery();
        if (this->config.Menu.Enable) menuPlugin::unregistery();
        if (this->config.Tpa) tpaPlugin::unregistery();
        if (this->config.Monitor.Enable) monitorPlugin::unregistery();
        if (this->config.Pvp) pvpPlugin::unregistery();
        if (this->config.Chat.Enable) chatPlugin::unregistery();
        if (this->config.AnnounCement) announcementPlugin::unregistery();
        if (this->config.Market.Enable) marketPlugin::unregistery();
        this->mSelf.getLogger().info("Unregister Event completed.");
        return true;
    }
}

LL_REGISTER_MOD(LOICollection::A, LOICollection::instance);
