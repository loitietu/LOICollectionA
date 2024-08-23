#include <string>
#include <vector>

struct C_Config {
    int version = 0;
    int FakeSeed = 114514;
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
        std::string join = "{player}加入了服务器";
        std::string left = "{player}退出了服务器";
        std::string llmoney = "§e§l检测到LLMoney发生变化 §b原值: §f${OriMoney} §a更改: §f${SetMoney} §e现值: §f${GetMoney}";
        std::vector<std::string> command = {};
        std::string tips = "该指令已被禁用";
    } Monitor;
    bool Pvp = false;
    struct C_Wallet {
        bool Enable = false;
        bool llmoney = true;
        std::string score = "money";
        double tax = 0.1;
    } Wallet;
    struct C_Chat {
        bool Enable = false;
        std::string chat = "<{player}> ${chat}";
    } Chat;
    bool AnnounCement = false;
    bool Market = false;
};