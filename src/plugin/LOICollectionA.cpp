#include <map>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <filesystem>

#include <ll/api/Config.h>
#include <ll/api/Logger.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/Mod/RegisterHelper.h>

#include "utils/I18nUtils.h"
#include "utils/SystemUtils.h"

#include "data/JsonStorage.h"
#include "data/SQLiteStorage.h"

#include "include/APIUtils.h"
#include "include/HookPlugin.h"
#include "include/languagePlugin.h"
#include "include/blacklistPlugin.h"
#include "include/mutePlugin.h"
#include "include/cdkPlugin.h"
#include "include/menuPlugin.h"
#include "include/tpaPlugin.h"
#include "include/shopPlugin.h"
#include "include/monitorPlugin.h"
#include "include/pvpPlugin.h"
#include "include/walletPlugin.h"
#include "include/chatPlugin.h"
#include "include/acPlugin.h"
#include "include/marketPlugin.h"

#include "include/ProtableTool/RedStone.h"
#include "include/ProtableTool/OrderedUI.h"

#include "LangPlugin.h"

#include "LOICollectionA.h"

namespace LOICollection {
    static std::unique_ptr<A> instance;

    A& A::getInstance() { return *instance; }

    bool A::load() {
        auto& logger = this->mSelf.getLogger();
        const auto& dataFilePath = this->mSelf.getDataDir();
        const auto& langFilePath = this->mSelf.getLangDir();
        const auto& configDataPath = this->mSelf.getConfigDir();
        const auto& configFilePath = configDataPath / "config.json";

        logger.setFile("./logs/LOICollectionA.log", false);

        Config::SynchronousPluginConfigVersion(this->config);
        logger.info("Loading LOICollection - A (Version {})", Config::getVersion());
        logger.info("Protocol - Mojang Eula (https://account.mojang.com/documents/minecraft_eula)");
        
        if (!std::filesystem::exists(configFilePath)) {
            logger.info("Configurations not found.");
            logger.info("Saving default configurations.");
            if (!ll::config::saveConfig(this->config, configFilePath)) {
                logger.error("Failed to save default configurations.");
                return false;
            }
        }
        Config::SynchronousPluginConfigType(this->config, configFilePath.string());
        if (!ll::config::loadConfig(this->config, configFilePath)) {
            logger.info("Update configurations.");
            if (!ll::config::saveConfig(this->config, configFilePath)) {
                logger.error("Failed to save default configurations.");
                return false;
            }
        }
        logger.info("Initialization of configurations completed.");

        std::filesystem::create_directory(dataFilePath);
        this->SettingsDB = std::make_unique<SQLiteStorage>(dataFilePath / "settings.db");
        this->BlacklistDB = std::make_unique<SQLiteStorage>(dataFilePath / "blacklist.db");
        this->MuteDB = std::make_unique<SQLiteStorage>(dataFilePath / "mute.db");
        this->ChatDB = std::make_unique<SQLiteStorage>(dataFilePath / "chat.db");
        this->MarketDB = std::make_unique<SQLiteStorage>(dataFilePath / "market.db");
        this->AnnounCementDB = std::make_unique<JsonStorage>(configDataPath / "announcement.json");
        this->CdkDB = std::make_unique<JsonStorage>(configDataPath / "cdk.json");
        this->MenuDB = std::make_unique<JsonStorage>(configDataPath / "menu.json");
        this->ShopDB = std::make_unique<JsonStorage>(configDataPath / "shop.json");
        logger.info("Initialization of database file completed.");
        
        std::filesystem::create_directory(langFilePath);
        if (this->config.Plugins.language.FileUpdate) {
            JsonStorage mObjectLanguage(langFilePath / "zh_CN.json");
            mObjectLanguage.write(CNLangData);
            mObjectLanguage.save();
        }
        I18nUtils::getInstance() = std::make_unique<I18nUtils>(langFilePath.string());
        I18nUtils::getInstance()->setDefaultLocal(this->config.ConsoleLanguage == "system" ?
            SystemUtils::getSystemLocaleCode() : this->config.ConsoleLanguage);
        logger.info("Initialization of language file completed.");

        HookPlugin::setFakeSeed(this->config.Plugins.FakeSeed);
        logger.info("Initialization of Hook completed.");
        return true;
    }

    bool A::enable() {
        LOICollectionAPI::initialization();

        HookPlugin::registery();
        Plugins::language::registery(&this->SettingsDB);
        if (this->config.Plugins.Blacklist) Plugins::blacklist::registery(&this->BlacklistDB);
        if (this->config.Plugins.Mute) Plugins::mute::registery(&this->MuteDB);
        if (this->config.Plugins.Cdk) Plugins::cdk::registery(&this->CdkDB);
        if (this->config.Plugins.Menu.ModuleEnabled) Plugins::menu::registery(&this->MenuDB, this->config.Plugins.Menu.MenuItemId);
        if (this->config.Plugins.Tpa) Plugins::tpa::registery(&this->SettingsDB);
        if (this->config.Plugins.Shop) Plugins::shop::registery(&this->ShopDB);
        if (this->config.Plugins.Monitor.ModuleEnabled) {
            std::map<std::string, std::variant<std::string, std::vector<std::string>, int, bool>> options;
            options["BelowName_Enabled"] = this->config.Plugins.Monitor.BelowName.ModuleEnabled;
            options["BelowName_RefreshInterval"] = this->config.Plugins.Monitor.BelowName.RefreshInterval;
            options["BelowName_Text"] = this->config.Plugins.Monitor.BelowName.FormatText;
            options["ServerToast_Enabled"] = this->config.Plugins.Monitor.ServerToast.ModuleEnabled;
            options["ServerToast_JoinText"] = this->config.Plugins.Monitor.ServerToast.FormatText.join;
            options["ServerToast_ExitText"] = this->config.Plugins.Monitor.ServerToast.FormatText.exit;
            options["ChangeScore_Enabled"] = this->config.Plugins.Monitor.ChangeScore.ModuleEnabled;
            options["ChangeScore_Scores"] = this->config.Plugins.Monitor.ChangeScore.ScoreboardLists;
            options["ChangeScore_Text"] = this->config.Plugins.Monitor.ChangeScore.FormatText;
            options["DisableCommand_Enabled"] = this->config.Plugins.Monitor.DisableCommand.ModuleEnabled;
            options["DisableCommand_Text"] = this->config.Plugins.Monitor.DisableCommand.FormatText;
            options["DisableCommand_List"] = this->config.Plugins.Monitor.DisableCommand.CommandLists;
            Plugins::monitor::registery(options);
        }
        if (this->config.Plugins.Pvp) Plugins::pvp::registery(&this->SettingsDB);
        if (this->config.Plugins.Wallet.ModuleEnabled) {
            std::map<std::string, std::variant<std::string, double>> options;
            options["score"] = this->config.Plugins.Wallet.TargetScoreboard;
            options["tax"] = this->config.Plugins.Wallet.ExchangeRate;
            Plugins::wallet::registery(options);
        }
        if (this->config.Plugins.Chat.ModuleEnabled) Plugins::chat::registery(&this->ChatDB, this->config.Plugins.Chat.FormatText);
        if (this->config.Plugins.AnnounCement) Plugins::announcement::registery(&this->AnnounCementDB, &this->SettingsDB);
        if (this->config.Plugins.Market.ModuleEnabled) {
            std::map<std::string, std::variant<std::string, int>> options;
            options["score"] = this->config.Plugins.Market.TargetScoreboard;
            options["upload"] = this->config.Plugins.Market.MaximumUpload;
            Plugins::market::registery(&this->MarketDB, options);
        }

        if (this->config.ProtableTool.RedStone) ProtableTool::RedStone::registery(this->config.ProtableTool.RedStone);
        if (this->config.ProtableTool.OrderedUI) ProtableTool::OrderedUI::registery();
        return true;
    }

    bool A::disable() {
        HookPlugin::unregistery();
        Plugins::language::unregistery();
        if (this->config.Plugins.Tpa) Plugins::tpa::unregistery();
        if (this->config.Plugins.Pvp) Plugins::pvp::unregistery();
        if (this->config.Plugins.AnnounCement) Plugins::announcement::unregistery();
        if (this->config.Plugins.Menu.ModuleEnabled) Plugins::menu::unregistery();
        if (this->config.Plugins.Monitor.ModuleEnabled) Plugins::monitor::unregistery();
        if (this->config.Plugins.Chat.ModuleEnabled) Plugins::chat::unregistery();
        if (this->config.Plugins.Market.ModuleEnabled) Plugins::market::unregistery();

        if (this->config.ProtableTool.RedStone) ProtableTool::RedStone::unregistery();
        if (this->config.ProtableTool.OrderedUI) ProtableTool::OrderedUI::unregistery();
        return true;
    }
}

LL_REGISTER_MOD(LOICollection::A, LOICollection::instance);
