# LOICollectionA
## Minecraft Bedrock Server LeviLamina Plugin

### [English](README.md) | [简体中文](README.zh.md)

## Implemented Features
Feature | Completed
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
Announcement | $\color{#00FF00}{√}$
Market | $\color{#00FF00}{√}$

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
           "join": "{player} has joined the server",
           "target": "money",
           "changed": "§e§lDetected Score Change §eCurrent Value: §f${GetScore}",
           "command": [],
           "tips": "This command has been disabled" 
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
       "Announcement": false,
       "Market": {
           "Enable": false,
           "score": "money"
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