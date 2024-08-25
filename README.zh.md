# LOICollectionA
## Minecraft Bedrock Server LeviLamina Plugin

### [English](README.md) | [简体中文](README.zh.md)

## 实现插件
实现功能 | 完成
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

### 如何安装插件？
1. 将文件 `LOICollection.dll` 放置在服务器目录的 `plugins` 文件夹中。
- `LOICollection.dll`（您可以在发布页面下载）。
2. 启动服务器。
3. 等待输出加载文本。
4. 完成安装。

### 如何配置文件？
1. 打开插件目录 `LOICollection`。
2. 打开文件 `config.json`。
3. 打开后，您会看到类似以下内容的内容：
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
- 请按照 Json 规范 (https://www.json.org/) 进行更改。
4. 您可以自由地进行更改并保存。

### 如何在本地编译？
打开本地命令提示符(cmd)并执行以下命令：
```cmd
git clone https://github.com/loitietu/LOICollectionA.git
cd LOICollectionA
xmake repo -u
xmake
```

### LICENSE
- 本插件根据 [GPL-3.0](LICENSE) 许可证进行许可。