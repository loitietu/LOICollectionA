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

#include "Include/ProtableTool/RedStone.h"
#include "Include/ProtableTool/OrderedUI.h"

#include "LangPlugin.h"

#include "LOICollectionA.h"

namespace LOICollection {
    static std::unique_ptr<A> instance;

    A& A::getInstance() { return *instance; }

    bool A::load() {
        auto& logger = this->mSelf.getLogger();
        const auto& dataFilePath = this->mSelf.getDataDir();
        const auto& configDataPath = this->mSelf.getConfigDir();
        const auto& configFilePath = configDataPath / "config.json";

        logger.setFile("./logs/LOICollectionA.log", false);

        toolUtils::Config::SynchronousPluginConfigVersion(&this->config);
        logger.info("Loading LOICollection - A (Version {})", toolUtils::Config::getVersion());
        logger.info("Protocol - Mojang Eula (https://account.mojang.com/documents/minecraft_eula)");
        
        if (!std::filesystem::exists(configFilePath)) {
            logger.info("Configurations not found.");
            logger.info("Saving default configurations.");
            if (!ll::config::saveConfig(this->config, configFilePath)) {
                logger.error("Failed to save default configurations.");
                return false;
            }
        }
        toolUtils::Config::SynchronousPluginConfigType(&this->config, configFilePath);
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
        this->AnnounCementDB = std::make_unique<JsonUtils>(configDataPath / "announcement.json");
        this->CdkDB = std::make_unique<JsonUtils>(configDataPath / "cdk.json");
        this->MenuDB = std::make_unique<JsonUtils>(configDataPath / "menu.json");
        this->ShopDB = std::make_unique<JsonUtils>(configDataPath / "shop.json");
        logger.info("Initialization of database file completed.");
        
        if (this->config.Plugins.language.update) {
            JsonUtils mObjectLanguage(configDataPath / "language.json");
            mObjectLanguage.set("zh_CN", CNLangData);
            mObjectLanguage.save();
        }
        I18nUtils::load(configDataPath / "language.json");
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
        if (this->config.Plugins.Menu.Enable) Plugins::menu::registery(&this->MenuDB, this->config.Plugins.Menu.ItemId);
        if (this->config.Plugins.Tpa) Plugins::tpa::registery(&this->SettingsDB);
        if (this->config.Plugins.Shop) Plugins::shop::registery(&this->ShopDB);
        if (this->config.Plugins.Monitor.Enable) {
            std::map<std::string, std::variant<std::string, std::vector<std::string>>> options;
            options["show"] = this->config.Plugins.Monitor.show;
            options["join"] = this->config.Plugins.Monitor.join;
            options["exit"] = this->config.Plugins.Monitor.exit;
            options["target"] = this->config.Plugins.Monitor.target;
            options["changed"] = this->config.Plugins.Monitor.changed;
            options["tips"] = this->config.Plugins.Monitor.tips;
            options["command"] = this->config.Plugins.Monitor.command;
            Plugins::monitor::registery(options);
        }
        if (this->config.Plugins.Pvp) Plugins::pvp::registery(&this->SettingsDB);
        if (this->config.Plugins.Wallet.Enable) {
            std::map<std::string, std::variant<std::string, double>> options;
            options["score"] = this->config.Plugins.Wallet.score;
            options["tax"] = this->config.Plugins.Wallet.tax;
            Plugins::wallet::registery(options);
        }
        if (this->config.Plugins.Chat.Enable) Plugins::chat::registery(&this->ChatDB, this->config.Plugins.Chat.chat);
        if (this->config.Plugins.AnnounCement) Plugins::announcement::registery(&this->AnnounCementDB, &this->SettingsDB);
        if (this->config.Plugins.Market.Enable) {
            std::map<std::string, std::string> options;
            options["score"] = this->config.Plugins.Market.score;
            Plugins::market::registery(&this->MarketDB, options);
        }

        if (this->config.ProtableTool.RedStone) ProtableTool::RedStone::registery(this->config.ProtableTool.RedStone);
        if (this->config.ProtableTool.OrderedUI) ProtableTool::OrderedUI::registery();
        return true;
    }

    bool A::disable() {
        HookPlugin::unregistery();
        Plugins::language::unregistery();
        if (this->config.Plugins.Menu.Enable) Plugins::menu::unregistery();
        if (this->config.Plugins.Tpa) Plugins::tpa::unregistery();
        if (this->config.Plugins.Monitor.Enable) Plugins::monitor::unregistery();
        if (this->config.Plugins.Pvp) Plugins::pvp::unregistery();
        if (this->config.Plugins.Chat.Enable) Plugins::chat::unregistery();
        if (this->config.Plugins.AnnounCement) Plugins::announcement::unregistery();
        if (this->config.Plugins.Market.Enable) Plugins::market::unregistery();

        if (this->config.ProtableTool.RedStone) ProtableTool::RedStone::unregistery();
        if (this->config.ProtableTool.OrderedUI) ProtableTool::OrderedUI::unregistery();
        return true;
    }
}

LL_REGISTER_MOD(LOICollection::A, LOICollection::instance);
