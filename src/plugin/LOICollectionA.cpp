#include <memory>
#include <string>
#include <filesystem>

#include <ll/api/Config.h>
#include <ll/api/io/Logger.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/Mod/RegisterHelper.h>

#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/JsonStorage.h"
#include "data/SQLiteStorage.h"

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/BlacklistPlugin.h"
#include "include/MutePlugin.h"
#include "include/CdkPlugin.h"
#include "include/MenuPlugin.h"
#include "include/TpaPlugin.h"
#include "include/ShopPlugin.h"
#include "include/MonitorPlugin.h"
#include "include/PvpPlugin.h"
#include "include/WalletPlugin.h"
#include "include/ChatPlugin.h"
#include "include/NoticePlugin.h"
#include "include/MarketPlugin.h"
#include "include/BehaviorEventPlugin.h"

#include "include/ProtableTool/BasicHook.h"
#include "include/ProtableTool/RedStone.h"
#include "include/ProtableTool/OrderedUI.h"

#include "LangPlugin.h"

#include "LOICollectionA.h"

namespace LOICollection {
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
        Config::SetBaseConfigContext(this->config);
        logger.info("Initialization of configurations completed.");

        std::filesystem::create_directory(dataFilePath);
        this->SettingsDB = std::make_unique<SQLiteStorage>(dataFilePath / "settings.db");
        this->BehaviorEventDB = std::make_unique<SQLiteStorage>(dataFilePath / "behaviorevent.db");
        this->BlacklistDB = std::make_unique<SQLiteStorage>(dataFilePath / "blacklist.db");
        this->MuteDB = std::make_unique<SQLiteStorage>(dataFilePath / "mute.db");
        this->TpaDB = std::make_unique<SQLiteStorage>(dataFilePath / "tpa.db");
        this->ChatDB = std::make_unique<SQLiteStorage>(dataFilePath / "chat.db");
        this->MarketDB = std::make_unique<SQLiteStorage>(dataFilePath / "market.db");
        this->NoticeDB = std::make_unique<JsonStorage>(configDataPath / "notice.json");
        this->CdkDB = std::make_unique<JsonStorage>(configDataPath / "cdk.json");
        this->MenuDB = std::make_unique<JsonStorage>(configDataPath / "menu.json");
        this->ShopDB = std::make_unique<JsonStorage>(configDataPath / "shop.json");

        logger.info("Initialization of database completed.");
        
        std::filesystem::create_directory(langFilePath);
        if (this->config.Plugins.language.FileUpdate) {
            JsonStorage mObjectLanguage(langFilePath / "zh_CN.json");
            mObjectLanguage.get() = CNLangData;
            mObjectLanguage.save();
        }
        I18nUtils::getInstance() = std::make_unique<I18nUtils>(langFilePath.string());
        I18nUtils::getInstance()->defaultLocale = this->config.ConsoleLanguage == "system" ?
            SystemUtils::getSystemLocaleCode("zh_CN") : this->config.ConsoleLanguage;
        logger.info("Initialization of language completed.");

        logger.info("Initialization of plugins completed.");
        return true;
    }

    bool A::enable() {
        LOICollectionAPI::initialization();

        Plugins::language::registery(&this->SettingsDB);
        if (this->config.Plugins.Blacklist) Plugins::blacklist::registery(&this->BlacklistDB);
        if (this->config.Plugins.Mute) Plugins::mute::registery(&this->MuteDB);
        if (this->config.Plugins.Cdk) Plugins::cdk::registery(&this->CdkDB);
        if (this->config.Plugins.Menu.ModuleEnabled) Plugins::menu::registery(&this->MenuDB);
        if (this->config.Plugins.Tpa.ModuleEnabled) Plugins::tpa::registery(&this->TpaDB, &this->SettingsDB);
        if (this->config.Plugins.Shop) Plugins::shop::registery(&this->ShopDB);
        if (this->config.Plugins.Monitor.ModuleEnabled) Plugins::monitor::registery();
        if (this->config.Plugins.Pvp) Plugins::pvp::registery(&this->SettingsDB);
        if (this->config.Plugins.Wallet.ModuleEnabled) Plugins::wallet::registery(&this->SettingsDB);
        if (this->config.Plugins.Chat.ModuleEnabled) Plugins::chat::registery(&this->ChatDB, &this->SettingsDB);
        if (this->config.Plugins.Notice) Plugins::notice::registery(&this->NoticeDB, &this->SettingsDB);
        if (this->config.Plugins.Market.ModuleEnabled) Plugins::market::registery(&this->MarketDB, &this->SettingsDB);
        if (this->config.Plugins.BehaviorEvent.ModuleEnabled) Plugins::behaviorevent::registery(&this->BehaviorEventDB);

        if (this->config.ProtableTool.BasicHook.ModuleEnabled) ProtableTool::BasicHook::registery(this->config.ProtableTool.BasicHook.FakeSeed);
        if (this->config.ProtableTool.RedStone) ProtableTool::RedStone::registery(this->config.ProtableTool.RedStone);
        if (this->config.ProtableTool.OrderedUI) ProtableTool::OrderedUI::registery();
        return true;
    }

    bool A::disable() {
        Plugins::language::unregistery();
        if (this->config.Plugins.Blacklist) Plugins::blacklist::unregistery();
        if (this->config.Plugins.Mute) Plugins::mute::unregistery();
        if (this->config.Plugins.Cdk) Plugins::cdk::unregistery();
        if (this->config.Plugins.Tpa.ModuleEnabled) Plugins::tpa::unregistery();
        if (this->config.Plugins.Pvp) Plugins::pvp::unregistery();
        if (this->config.Plugins.Wallet.ModuleEnabled) Plugins::wallet::unregistery();
        if (this->config.Plugins.Notice) Plugins::notice::unregistery();
        if (this->config.Plugins.Menu.ModuleEnabled) Plugins::menu::unregistery();
        if (this->config.Plugins.Shop) Plugins::shop::unregistery();
        if (this->config.Plugins.Monitor.ModuleEnabled) Plugins::monitor::unregistery();
        if (this->config.Plugins.Chat.ModuleEnabled) Plugins::chat::unregistery();
        if (this->config.Plugins.Market.ModuleEnabled) Plugins::market::unregistery();
        if (this->config.Plugins.BehaviorEvent.ModuleEnabled) Plugins::behaviorevent::unregistery();

        if (this->config.ProtableTool.RedStone) ProtableTool::RedStone::unregistery();
        if (this->config.ProtableTool.OrderedUI) ProtableTool::OrderedUI::unregistery();
        return true;
    }
}

LL_REGISTER_MOD(LOICollection::A, LOICollection::A::getInstance());
