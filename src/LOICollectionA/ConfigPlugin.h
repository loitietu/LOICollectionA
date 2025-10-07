#pragma once

#include <string>
#include <vector>

struct C_Config {
    int version = 0;
    std::string ConsoleLanguage = "system";
    struct C_Plugins {
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
                std::string FormatText = "§e§l检测到Score §f{0}§e 发生变化 §b原值: §f{1} §a更改: §f{2} §e现值: §f{3}";
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
            int RedEnvelopeTimeout = 60;
        } Wallet;
        struct C_Chat {
            bool ModuleEnabled = false;
            std::string FormatText = "<{player}> ${0}";
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
        struct C_BehaviorEvent {
            bool ModuleEnabled = false;
            int OrganizeDatabaseInterval = 144;
            int CleanDatabaseInterval = 1;
            int RefreshIntervalInMinutes = 5;
            struct C_Events {
                struct C_onPlayerConnect {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerConnect;
                struct C_onPlayerDisconnect {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerDisconnect;
                struct C_onPlayerChat {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerChat;
                struct C_onPlayerAddExperience {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerAddExperience;
                struct C_onPlayerAttack {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerAttack;
                struct C_onPlayerChangePerm {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerChangePerm;
                struct C_onPlayerDestroyBlock {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerDestroyBlock;
                struct C_onPlayerPlaceBlock {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerPlaceBlock;
                struct C_onPlayerDie {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerDie;
                struct C_onPlayerPickUpItem {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerPickUpItem;
                struct C_onPlayerRespawn {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerRespawn;
                struct C_onPlayerUseItem {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerUseItem;
                struct C_onPlayerContainerInteract {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onPlayerContainerInteract;
                struct C_onBlockExplode {
                    bool ModuleEnabled = true;
                    bool RecordDatabase = true;
                    bool OutputConsole = true;
                } onBlockExplode;
            } Events;
        } BehaviorEvent;
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
    std::string GetVersion();

    void SynchronousPluginConfigVersion(C_Config& config);
    void SynchronousPluginConfigType(C_Config& config, const std::string& path);
}