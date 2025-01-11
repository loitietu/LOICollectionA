# LOICollectionA
## Minecraft Bedrock Server LeviLamina Plugin

### [English](README.md) | [简体中文](README.zh.md)

## 实现插件
- [x] Blacklist
- [x] Mute
- [x] Cdk
- [x] Menu
- [x] Tpa
- [x] Shop
- [x] Monitor
- [x] Pvp
- [x] Wallet
- [x] Chat
- [x] Announcement
- [x] Market

### 如何安装插件？
1. 在服务器目录中执行命令
```cmd
lip install github.com/loitietu/LOICollectionA
```
2. 启动服务器。
3. 等待输出加载文本。
4. 完成安装。

### 如何配置文件？
1. 打开插件目录 `LOICollectionA`。
2. 打开文件 `config.json`。
3. 打开后，您会看到类似以下内容的内容：
```json
{
    "version": 160,
    "ConsoleLanguage": "system",
    "Plugins": {
        "FakeSeed": "$random",
        "language": {
            "FileUpdate": true
        },
        "Blacklist": false,
        "Mute": false, 
        "Cdk": false,
        "Menu": {
            "ModuleEnabled": false,
            "MenuItemId": "minecraft:clock",
            "EntranceKey": "main"
        },
        "Tpa": false,
        "Shop": false,
        "Monitor": {
            "ModuleEnabled": false,
            "BelowName": {
                "ModuleEnabled": true,
                "RefreshInterval": 20,
                "FormatText": "{player}"
            },
            "ServerToast": {
                "ModuleEnabled": true,
                "FormatText": {
                    "join": "{player} 加入了服务器",
                    "exit": "{player} 退出了服务器"
                }
            },
            "ChangeScore": {
                "ModuleEnabled": true,
                "ScoreboardLists": [],
                "FormatText": "§e§l检测到Score §f${Object}§e 发生变化 §b原值: §f${OriMoney} §a更改: §f${SetMoney} §e现值: §f${GetMoney}"
            },
            "DisableCommand": {
                "ModuleEnabled": true,
                "FormatText": "§c该指令已被禁用",
                "CommandLists": []
            }
        },
        "Pvp": false,
        "Wallet": {
            "ModuleEnabled": false,
            "TargetScoreboard": "money",
            "ExchangeRate": 0.1 
        },
        "Chat": {
            "ModuleEnabled": false,
            "FormatText": "<{player}> ${chat}" 
        },
        "AnnounCement": false,
        "Market": {
            "ModuleEnabled": false,
            "TargetScoreboard": "money",
            "MaximumUpload": 20
        }
    },
    "ProtableTool": {
        "RedStone": 0,
        "OrderedUI": false
    }
}
```
- 请按照 Json 规范 (https://www.json.org/) 进行更改。
4. 您可以自由地进行更改并保存。

### 如何在本地编译？
打开本地命令提示符(cmd)并执行以下命令：
```cmd
git clone https://github.com/loitietu/LOICollectionA.git
cd LOICollectionA
xmake repo -u
xmake lua scripts/project.lua
xmake
```

### LICENSE
- 本插件根据 [GPL-3.0](LICENSE) 许可证进行许可。