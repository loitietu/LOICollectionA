#ifndef CONFIG_PLUGIN_H
#define CONFIG_PLUGIN_H

#include <string>
#include <vector>

struct C_Config {
    int version = 0;
    struct C_Plugins {
        std::string FakeSeed = "random";
        struct C_Language{
            bool update = true;
        } language;
        bool Blacklist = false;
        bool Mute = false;
        bool Cdk = false;
        struct C_Menu {
            bool Enable = false;
            std::string ItemId = "minecraft:clock";
        } Menu;
        bool Tpa = false;
        bool Shop = false;
        struct C_Monitor {
            bool Enable = false;
            std::string show = "{player}\n称号: {title}";
            std::string join = "{player} 加入了服务器";
            std::string exit = "{player} 退出了服务器";
            std::string target = "money";
            std::string changed = "§e§l检测到Score发生变化 §e现值: §f${GetScore}";
            std::string tips = "该指令已被禁用";
            std::vector<std::string> command = {};
        } Monitor;
        bool Pvp = false;
        struct C_Wallet {
            bool Enable = false;
            std::string score = "money";
            double tax = 0.1;
        } Wallet;
        struct C_Chat {
            bool Enable = false;
            std::string chat = "<{player}> ${chat}";
        } Chat;
        bool AnnounCement = false;
        struct C_Market {
            bool Enable = false;
            std::string score = "money";
        } Market;
    } Plugins;
    struct C_AntiCheat {
        struct C_RedStone {
            bool Enable = false;
            int tick = 5;
        } RedStone;
    } AntiCheat;
};

#endif