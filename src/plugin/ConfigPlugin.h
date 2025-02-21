#pragma once

#include <string>
#include <vector>

struct C_Config {
    int version = 0;
    std::string ConsoleLanguage = "system";
    struct C_Plugins {
        struct C_Language{
            bool FileUpdate = true;
        } language;
        bool Blacklist = false;
        bool Mute = false;
        bool Cdk = false;
        struct C_Menu {
            bool ModuleEnabled = false;
            std::string MenuItemId = "minecraft:clock";
            std::string EntranceKey = "main";
        } Menu;
        struct C_Tpa {
            bool ModuleEnabled = false;
            std::string TargetScoreboard = "money";
            int BlacklistUpload = 10;
            int RequestRequired = 100;
        } Tpa;
        bool Shop = false;
        struct C_Monitor {
            bool ModuleEnabled = false;
            struct C_BelowName {
                bool ModuleEnabled = true;
                int RefreshInterval = 20;
                std::string FormatText = "{player}";
            } BelowName;
            struct C_ServerToast {
                bool ModuleEnabled = true;
                struct C_FormatText {
                    std::string join = "{player} 加入了服务器";
                    std::string exit = "{player} 离开了服务器";
                } FormatText;
            } ServerToast;
            struct C_ChangeScore {
                bool ModuleEnabled = true;
                std::vector<std::string> ScoreboardLists{};
                std::string FormatText = "§e§l检测到Score §f${Object}§e 发生变化 §b原值: §f${OriMoney} §a更改: §f${SetMoney} §e现值: §f${GetMoney}";
            } ChangeScore;
            struct C_DisableCommand {
                bool ModuleEnabled = true;
                std::string FormatText = "该指令已被禁用";
                std::vector<std::string> CommandLists{};
            } DisableCommand;
        } Monitor;
        bool Pvp = false;
        struct C_Wallet {
            bool ModuleEnabled = false;
            std::string TargetScoreboard = "money";
            double ExchangeRate = 0.1;
        } Wallet;
        struct C_Chat {
            bool ModuleEnabled = false;
            std::string FormatText = "<{player}> ${chat}";
            int BlacklistUpload = 10;
        } Chat;
        bool Notice = false;
        struct C_Market {
            bool ModuleEnabled = false;
            std::string TargetScoreboard = "money";
            int MaximumUpload = 20;
            int BlacklistUpload = 10;
            std::vector<std::string> ProhibitedItems{};
        } Market;
    } Plugins;
    struct C_ProtableTool {
        struct C_BasicHook {
            bool ModuleEnabled = false;
            std::string FakeSeed = "$random";
        } BasicHook;
        int RedStone = 0;
        bool OrderedUI = false;
    } ProtableTool;
};

namespace Config {
    std::string getVersion();

    void SynchronousPluginConfigVersion(C_Config& config);
    void SynchronousPluginConfigType(C_Config& config, const std::string& path);
}