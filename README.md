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
1. Place the file `LOICollection.dll` in the server directory `plugins`.
- `LOICollection.dll` (You can download it in the Release page.).
2. Start the server
3. Wait for the output to load the text
4. Complete the installation

### How to configure the file?
1. Open the plugin directory `LOICollection`
2. Open file `config.json`
3. When you open it, you'll see something like this:
```json
{
    "version": 140,
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
        "left": "{player}退出了服务器",
        "llmoney": "§e§l检测到LLMoney发生变化 §b原值: §f${OriMoney} §a更改: §f${SetMoney} §e现值: §f${GetMoney}",
        "command": [],
        "tips": "该指令已被禁用" 
    },
    "Pvp": false,
    "Wallet": {
        "Enable": false,
        "llmoney": true,
        "score": "money",
        "tax": 0.1 
    },
    "Chat": {
        "Enable": false,
        "chat": "<{player}> ${chat}"
    },
    "AnnounCement": false,
    "Market": false
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
xmake
```

### LICENSE
- This plugin is licensed under the [GPL-3.0](LICENSE) license.