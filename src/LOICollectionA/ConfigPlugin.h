#pragma once

#include <string>
#include <vector>

namespace Config {
    struct C_Blacklist {
        bool ModuleEnabled = false;

        struct C_BroadcastMessage {
            bool ModuleEnabled = true;
            std::string FormatText = "tr(blacklist.broadcast)";
        } BroadcastMessage;
    };

    struct C_Menu {
        bool ModuleEnabled = false;
        std::string MenuItemId = "minecraft:clock";
        std::string EntranceKey = "main";
    };

    struct C_Tpa {
        bool ModuleEnabled = false;
        std::string TargetScoreboard = "money";
        int BlacklistUpload = 10;
        int RequestRequired = 100;
    };

    struct C_Monitor {
        bool ModuleEnabled = false;

        struct C_BelowName {
            bool ModuleEnabled = true;
            int RefreshInterval = 20;
            int RefreshDisplayInterval = 100;
            std::vector<std::vector<std::string>> Pages{{ "{player}" }};
        } BelowName;

        struct C_ServerToast {
            bool ModuleEnabled = true;
            struct C_FormatText {
                std::string join = "{player}' 加入了服务器'";
                std::string exit = "{player}' 离开了服务器'";
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
        
        struct C_DynamicMotd {
            bool ModuleEnabled = true;
            int RefreshInterval = 200;
            std::vector<std::string> Pages{ "'在线玩家: '{server.player.online}/{server.player.max}" };
        } DynamicMotd;

        struct C_Sidebar {
            bool ModuleEnabled = true;
            int RefreshInterval = 20;
        
            std::vector<std::string> Titles{ "Title" };
            std::vector<std::vector<std::string>> Pages{ { "Content" } };
        } Sidebar;
    };

    struct C_Pvp {
        bool ModuleEnabled = false;
        struct C_ExtraListener {
            bool onActorHurt = true;
            bool onSplashPotion = true;
            bool onProjectileHit = true;
        } ExtraListener;
    };

    struct C_Wallet {
        bool ModuleEnabled = false;
        std::string TargetScoreboard = "money";
        double ExchangeRate = 0.1;
        int RedEnvelopeTimeout = 60;
    };

    struct C_Chat {
        bool ModuleEnabled = false;
        std::string FormatText = "<{player}> ${0}";
        int BlacklistUpload = 10;
    };

    struct C_Market {
        bool ModuleEnabled = false;
        std::string TargetScoreboard = "money";
        int MaximumUpload = 20;
        int BlacklistUpload = 10;
        std::vector<std::string> ProhibitedItems{};
    };

    struct C_BehaviorEvent {
        bool ModuleEnabled = false;
        int OrganizeDatabaseInterval = 144;
        int CleanThresholdEvent = 10000;
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
    };

    struct C_Statistics {
        bool ModuleEnabled = false;
        int RefreshIntervalInMinutes = 5;
        int RankingPlayerCount = 100;

        struct C_DatabaseInfo {
            bool OnlineTime = true;
            bool Kill = true;
            bool Death = true;
            bool Place = true;
            bool Destroy = true;
            bool Respawn = true;
            bool Join = true;
        } DatabaseInfo;
    };

    struct C_Plugins {
        C_Blacklist Blacklist;

        bool Mute = false;
        bool Cdk = false;

        C_Menu Menu;
        C_Tpa Tpa;
        
        bool Shop = false;

        C_Monitor Monitor;
        C_Pvp Pvp;
        C_Wallet Wallet;
        C_Chat Chat;

        bool Notice = false;

        C_Market Market;
        C_BehaviorEvent BehaviorEvent;
        C_Statistics Statistics;
    };

    struct C_BasicHook {
        bool ModuleEnabled = false;
        std::string FakeSeed = "$random";
    };

    struct C_ProtableTool {
        C_BasicHook BasicHook;

        int RedStone = 0;
        bool OrderedUI = false;
    };

    struct C_Config {
        int version = 0;
        std::string ConsoleLanguage = "system";
        
        C_Plugins Plugins;
        C_ProtableTool ProtableTool;
    };
    
    std::string GetVersion();

    void SynchronousPluginConfigVersion(C_Config& config);
    void SynchronousPluginConfigType(C_Config& config, const std::string& path);
}
