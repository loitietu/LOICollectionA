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
    "version": 150,
    "ConsoleLanguage": "system",
    "Plugins": {
        "FakeSeed": "$random",
        "language": {
            "update": true
        },
        "Blacklist": false,
        "Mute": false, 
        "Cdk": false,
        "Menu": {
            "Enable": false,
            "ItemId": "minecraft:clock" 
        },
        "Tpa": false,
        "Shop": false,
        "Monitor": {
            "Enable": false,
            "show": "[{title}] §r{player}",
            "join": "{player} has joined the server",
            "exit": "{player} has left the server",
            "target": "money",
            "changed": "§e§lDetected Score §f${Object}§e Change §bOriginal Value: §f${OriMoney} §aChange: §f${SetMoney} §eCurrent Value: §f${GetScore}",
            "tips": "This command has been disabled",
            "command": [],
        },
        "Pvp": false,
        "Wallet": {
            "Enable": false,
            "score": "money",
            "tax": 0.1 
        },
        "Chat": {
            "Enable": false,
            "chat": "<{player}> ${chat}"
        },
        "AnnounCement": false,
        "Market": {
            "Enable": false,
            "score": "money"
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