# LOICollectionA
## Minecraft Bedrock Server LeviLamina Plugin

### [English](README.md) | [简体中文](README.zh.md)

## Implemented Features
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

### How to Install the Plugin?
1. Execute the command in the server directory:
   ```cmd
   lip install github.com/loitietu/LOICollectionA
   ```
2. Start the server.
3. Wait for the output loading text.
4. Installation is complete.

### How to Configure the File?
1. Open the plugin directory `LOICollectionA`.
2. Open the file `config.json`.
3. After opening, you will see content similar to the following:
```json
{
    "version": 152,
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
            "MenuItemId": "minecraft:clock" 
        },
        "Tpa": false,
        "Shop": false,
        "Monitor": {
            "ModuleEnabled": false,
            "BelowName": {
                "ModuleEnabled": true,
                "RefreshInterval": 20,
                "FormatText": "[{title}] §r{player}"
            },
            "ServerToast": {
                "ModuleEnabled": true,
                "FormatText": {
                    "join": "{player} has joined the server",
                    "exit": "{player} has left the server"
                }
            },
            "ChangeScore": {
                "ModuleEnabled": true,
                "ScoreboardLists": [],
                "FormatText": "§e§lDetected Score §f${Object}§e Change §bOriginal Value: §f${OriMoney} §aChange: §f${SetMoney} §eCurrent Value: §f${GetScore}"
            },
            "DisableCommand": {
                "ModuleEnabled": true,
                "FormatText": "This command has been disabled",
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
            "FormatText": "<{player}> ${chat}",
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
- Please make changes according to the Json specification (https://www.json.org/).
4. You can freely make changes and save.

### How to Compile Locally?
Open the local command prompt (cmd) and execute the following commands:
```cmd
git clone https://github.com/loitietu/LOICollectionA.git
cd LOICollectionA
xmake repo -u
xmake lua scripts/project.lua
xmake
```

### LICENSE
- This plugin is licensed under the [GPL-3.0](LICENSE) license.