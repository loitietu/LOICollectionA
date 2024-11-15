# 配置文件

> 配置文件属于 `.json` 格式的文件，一般情况下文件名为 `config.json`  
> 对于 `config.json` 中的配置项，在 `启动服务器` 时，只会进行一次读取，之后的修改不会生效  
> 而配置文件通常是在安装完成后 `第一次启动服务器` 时出现，位于 `plugins/LOICollectionA/config/` 目录下

!> 配置文件中的配置项，必须为 `英文` 或 `数字` 或 `下划线`，请不要使用中文作为配置项的名称，否则会导致配置文件无法正常读取。
请勿使用记事本等不支持 `.json` 格式的文本编辑器进行编辑，以免导致配置文件结构损坏。

## 配置文件结构
> 以下为 LOICollectionA 1.5.0 的配置文件结构，对于后续版本的配置文件结构可能会有所不同。
```json
{
    "version": 150, // 配置文件版本号，请勿修改
    "ConsoleLanguage": "system", // 控制台语言，其中 system 为跟随系统语言，zh_CN 为中文
    "Plugins": { // 内置插件配置
        "FakeSeed": "$random", // 假种子配置，其中 $random 为随机数，其余为数字时是固定值（为 0 时不启用）
        "language": { // 多语言配置
            "update": true // 是否启用语言更新（即每次启动服务器时向内置语言文件进行重写）
        },
        "Blacklist": false, // 是否启用黑名单
        "Mute": false,   // 是否启用禁言
        "Cdk": false, // 是否启用 CDK
        "Menu": { // 菜单配置
            "Enable": false, // 是否启用菜单
            "ItemId": "minecraft:clock"  // 打开菜单物品 ID
        },
        "Tpa": false, // 是否启用 TPA
        "Shop": false, // 是否启用商店
        "Monitor": { // 消息强化配置
            "Enable": false, // 是否启用消息强化
            "show": "[{title}] §r{player}", // 玩家名称显示格式，支持 LOICollectionA API 变量
            "join": "{player} 加入了服务器", // 玩家加入服务器提示，支持 LOICollectionA API 变量
            "exit": "{player} 退出了服务器", // 玩家退出服务器提示，支持 LOICollectionA API 变量
            "target": "money", // 检测 Score 变化的对象（为 $all 时会检测所有 Score 的变更）
            "changed": "§e§l检测到Score §f${Object}§e 发生变化 §b原值: §f${OriMoney} §a更改: §f${SetMoney} §e现值: §f${GetMoney}", // Score 变化提示
            "tips": "该指令已被禁用" , // 指令禁用提示
            "command": [], // 禁用指令列表
        },
        "Pvp": false, // 是否启用 PVP
        "Wallet": { // 钱包配置
            "Enable": false, // 是否启用钱包
            "score": "money", // 钱包指定使用 Score 对象
            "tax": 0.1  // 钱包汇率
        },
        "Chat": { // 聊天强化配置
            "Enable": false, // 是否启用聊天强化
            "chat": "<{player}> ${chat}" // 聊天消息格式，支持 LOICollectionA API 变量
        },
        "AnnounCement": false, // 是否启用公告栏
        "Market": { // 玩家市场配置
            "Enable": false, // 是否启用玩家市场
            "score": "money" // 玩家市场指定使用 Score 对象
        }
    },
    "ProtableTool": { // 便携工具配置
        "RedStone": 0, // 红石高频检测，其中为每秒频率（为 0 时不启用）
        "OrderedUI": false // 是否启用有序 UI
    }
}
```

?> 在更改时请按照 `Json规范`(https://www.json.org/) 进行更改