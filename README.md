# LOICollectionA
## Minecraft Bedrock Server LeviLamina Plugin

### [English](README.md) | [简体中文](README.zh.md)

## Implementation plug-in
Realize the function | accomplish
--- | :---:
Blacklist | $\color{#00FF00}{√}$
Mute | $\color{#00FF00}{√}$
Cdk | $\color{#00FF00}{√}$
Menu | $\color{#00FF00}{√}$
Tpa | $\color{#00FF00}{√}$
Shop | $\color{#00FF00}{√}$
Monitor | $\color{#00FF00}{√}$
Pvp | $\color{#00FF00}{√}$
Wallet | $\color{#00FF00}{√}$
Chat | $\color{#00FF00}{√}$
AnnounCement | $\color{#00FF00}{√}$
Market | $\color{#00FF00}{√}$

### How to install the plugin?
1. Execute the command in the server directory
```cmd
lip install github.com/loitietu/LOICollectionA
```
2. Start the server
3. Wait for the output to load the text
4. Complete the installation

### How to configure the file?
1. Open the plugin directory `LOICollectionA`
2. Open file `config.json`
3. When you open it, you'll see something like this:
```json
{
    "version": 141,
    "FakeSeed": 114514,
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
        "join": "{player}加入了服务器",
        "target": "money",
        "changed": "§e§l检测到Score发生变化 §e现值: §f${GetScore}",
        "command": [],
        "tips": "该指令已被禁用" 
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
}
```
- Please follow the Json specification (https://www.json.org/) for changes.
4. You can change it freely and save it.

### How to compile locally?
Open the local cmd and execute the command:
```cmd
git clone https://github.com/loitietu/LOICollectionA.git
cd LOICollectionA
xmake repo -u
xmake lua scripts/project.lua
xmake
```

### LICENSE
- This plugin is licensed under the [GPL-3.0](LICENSE) license.