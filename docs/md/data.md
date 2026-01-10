# 数据文件

## 配置文件

配置文件属于 `.json` 格式的文件，一般情况下文件名为 `config.json`  
对于 `config.json` 中的配置项，在 `启动服务器` 时，只会进行一次读取，之后的修改不会生效  
而配置文件通常是在安装完成后 `第一次启动服务器` 时出现，位于 `plugins/LOICollectionA/config/` 目录下

> [!WARNING]
> 配置文件中的配置项，必须为 `英文` 或 `数字` 或 `下划线`，请不要使用中文作为配置项的名称，否则会导致配置文件无法正常读取。  
> 请勿使用记事本等不支持 `.json` 格式的文本编辑器进行编辑，以免导致配置文件结构损坏。

```json
{
    "version": 44456286, // 配置文件版本号，通常为一个由程序生成的八位数字，不建议修改
    "ConsoleLanguage": "system", // 控制台语言，其中 system 为跟随系统语言，zh_CN 为中文
    "Plugins": { // 内置插件配置
        "Blacklist": false, // 是否启用黑名单
        "Mute": false,   // 是否启用禁言
        "Cdk": false, // 是否启用 CDK
        "Menu": { // 菜单配置
            "ModuleEnabled": false, // 是否启用菜单
            "MenuItemId": "minecraft:clock",  // 打开菜单物品 ID
            "EntranceKey": "main" // 菜单入口
        },
        "Tpa": { // TPA 配置
            "ModuleEnabled": false, // 是否启用 TPA
            "TargetScoreboard": "money", // TPA 请求目标使用 Score 对象
            "BlacklistUpload": 10, // 玩家黑名单目标最大上传数量
            "RequestRequired": 100 // TPA 请求所需 Score 数量
        },
        "Shop": false, // 是否启用商店
        "Monitor": { // 消息强化配置
            "ModuleEnabled": false, // 是否启用消息强化
            "BelowName": {
                "ModuleEnabled": true, // 是否启用玩家名称显示
                "RefreshInterval": 20, // 刷新间隔，单位为 tick（20 tick = 1 秒）
                "RefreshDisplayInterval": 100, // 刷新显示间隔，单位为 tick
                "Pages": [
                    [
                        "{player}" // 每行显示内容
                    ]
                ] // 玩家名称显示格式，支持 LOICollectionA API 变量
            },
            "ServerToast": {
                "ModuleEnabled": true, // 是否启用服务器提示
                "FormatText": {
                    "join": "{player}' 加入了服务器'", // 玩家加入服务器提示，支持 LOICollectionA API 变量
                    "exit": "{player}' 退出了服务器'" // 玩家退出服务器提示，支持 LOICollectionA API 变量
                }
            },
            "ChangeScore": {
                "ModuleEnabled": true, // 是否启用 Score 变化检测
                "ScoreboardLists": [], // 检测 Score 变化的对象（为空时会检测所有 Score 的变更）
                "FormatText": "§e§l检测到Score §f${Object}§e 发生变化 §b原值: §f${OriMoney} §a更改: §f${SetMoney} §e现值: §f${GetMoney}" // Score 变化提示
            },
            "DisableCommand": {
                "ModuleEnabled": true, // 是否启用指令禁用检测
                "FormatText": "该指令已被禁用", // 指令禁用提示
                "CommandLists": [] // 被禁用的指令列表
            },
            "DynamicMotd": {
                "ModuleEnabled": true, // 是否启用动态服务器 MOTD
                "RefreshInterval": 200, // 刷新间隔，单位为 tick（20 tick = 1 秒）
                "Pages": [
                    "'在线玩家: '{server.player.online}/{server.player.max}" // 每行显示内容，支持 LOICollectionA API 变量
                ]
            }
        },
        "Pvp": {
            "ModuleEnabled": true, // 是否启用 PVP
            "ExtraListener": {
                "onActorHurt": true, // 是否启用玩家伤害侦听器
                "onSplashPotion": true, // 是否启用药水效果侦听器
                "onProjectileHit": true // 是否启用射击侦听器
            }
        },
        "Wallet": { // 钱包配置
            "ModuleEnabled": false, // 是否启用钱包
            "TargetScoreboard": "money", // 钱包指定使用 Score 对象
            "ExchangeRate": 0.1  // 钱包汇率
        },
        "Chat": { // 聊天强化配置
            "ModuleEnabled": false, // 是否启用聊天强化
            "FormatText": "<{player}> ${chat}", // 聊天消息格式，支持 LOICollectionA API 变量
            "BlacklistUpload": 10 // 黑名单目标最大上传数量
        },
        "Notice": false, // 是否启用公告栏
        "Market": { // 玩家市场配置
            "ModuleEnabled": false, // 是否启用玩家市场
            "TargetScoreboard": "money", // 玩家市场指定使用 Score 对象
            "MaximumUpload": 20, // 玩家市场最大上传数量
            "BlacklistUpload": 10, // 玩家黑名单目标最大上传数量
            "ProhibitedItems": [] // 玩家市场禁止上传的物品
        },
        "BehaviorEvent": { // 行为事件配置
            "ModuleEnabled": false, // 是否启用行为事件
            "OrganizeDatabaseInterval": 144, // 行为事件数据库清理阈值（单位为小时）
            "CleanThresholdEvent": 10000, // 行为事件清理阈值
            "CleanDatabaseInterval": 1, // 数据库自动清理间隔（单位为小时）
            "RefreshIntervalInMinutes": 5, // 行为事件记录间隔（单位为分钟）
            "Events": { // 行为事件配置
                "onPlayerConnect": { // 玩家连接事件
                    "ModuleEnabled": true, // 是否启用该事件
                    "RecordDatabase": true, // 是否记录到数据库
                    "OutputConsole": true // 是否输出到控制台
                },
                "onPlayerDisconnect": { // 玩家断开连接事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerChat": { // 玩家聊天事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerAddExperience": { // 玩家获得经验事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerAttack": { // 玩家攻击事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerChangePerm": { // 玩家权限改变事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerDestroyBlock": { // 玩家破坏方块事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerPlaceBlock": { // 玩家放置方块事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerDie": { // 玩家死亡事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerPickUpItem": { // 玩家捡起物品事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerRespawn": { // 玩家重生事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerUseItem": { // 玩家使用物品事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onPlayerContainerInteract": { // 玩家容器交互事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                },
                "onBlockExplode": { // 方块爆炸事件
                    "ModuleEnabled": true,
                    "RecordDatabase": true,
                    "OutputConsole": true
                }
            }
        },
        "Statistics": {
            "ModuleEnabled": true, // 是否启用统计
            "RefreshIntervalInMinutes": 1, // 统计刷新间隔（单位为分钟）
            "RankingPlayerCount": 100, // 排行榜显示数量
            "DatabaseInfo": { // 统计数据库信息（每次退出后记录）
                "OnlineTime": true, // 是否启用在线时间统计
                "Kill": true, // 是否启用击杀生物统计
                "Death": true, // 是否启用死亡统计
                "Place": true, // 是否启用放置方块统计
                "Destroy": true, // 是否启用破坏方块统计
                "Respawn": true, // 是否启用重生统计
                "Join": true // 是否启用加入服务器统计
            }
        }
    },
    "ProtableTool": { // 便携工具配置
        "BasicHook": { // 基础功能配置
            "ModuleEnabled": false, // 是否启用基础功能
            "FakeSeed": "$random" // 假种子配置
        },
        "RedStone": 0, // 红石高频检测，其中为每秒频率（为 0 时不启用）
        "OrderedUI": false // 是否启用有序 UI
    }
}
```

> [!NOTE]
> 以上内容取自 LOICollectionA 1.10.0 的配置文件结构，对于后续版本的配置文件结构可能会有所不同。

## 模块数据文件

数据文件是指存储在数据库中的数据文件。数据文件是数据库的核心，它存储了数据库中的所有数据。数据文件可以是文本文件、二进制文件或者其他类型的文件。数据文件的格式和内容取决于数据库的类型和应用场景。  
目前 `LOICollectionA` 支持 `Json` 和 `SQLite` 两种数据文件格式。其中只有 `Json` 格式的数据文件可以被直接修改。而对于 `SQLite` 格式的数据文件，我们是不建议您直接修改的。

?> 通常情况下，您不需要手动修改数据文件，因为在使用 `LOICollectionA` 的过程中，对于指定数据文件是存在内部编辑器的，您可以通过它们进行更加快捷的编辑。

### menu.json

> [!NOTE]
> 以下内容取自 LOICollectionA 1.9.0 的 `menu.json` 结构，对于后续版本的 `menu.json` 结构可能会有所不同。  

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `menu.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#menu) 进行编辑

```json
{
    "main": { // 表单 ID（同时 main 也为表单入口，不可不存在）
        "title": "Menu Example", // 表单标题
        "content": "This is a menu example", // 表单内容
        "info": { // 部分功能提供（可选）
            "exit": "execute as ${player} run say Exit Menu", // 玩家退出表单时执行命令（其中 ${player} 代表玩家名称）
            "permission": "execute as ${player} run say You do not have permission to use this button", // 使用部分按钮时，玩家没有权限时所执行命令
            "score": "execute as ${player} run say You do not have enough score to use this button" // 使用部分按钮时，玩家没有足够 Score 时所执行命令
        },
        "type": "Simple", // 表单类型 （Simple 类似于按钮列表）
        "customize": [ // 按钮列表
            {
                "title": "Header", // 控件标题
                "id": "Header", // 控件 ID（不可重复）
                "type": "header" // 控件类型（header 为标题）
            },
            {
                "title": "Label", // 控件标题
                "id": "Label", // 控件 ID（不可重复）
                "type": "label" // 控件类型（label 为标签）
            },
            {
                "id": "Divider", // 控件 ID（不可重复）
                "type": "divider" // 控件类型（divider 为分割线）
            },
            {
                "title": "Button 1", // 按钮标题
                "image": "",  // 按钮图标（可选，只支持 path 类型）
                "id": "Button1", // 按钮 ID（不可重复）
                "scores": { // 按钮所需的 Score（可选）
                    "money": 100 // 按钮所需的 Score 分数
                },
                "run": [ // 使用该按钮时，当所有条件都满足时，所执行的命令（其中 ${player} 代表玩家名称）
                    "execute as ${player} run say Button1",
                    "execute as ${player} run say Button1 - 1"
                ],
                "type": "button", // 按钮类型（button 为按钮）
                "permission": 0 // 按钮所需的权限等级（0 为无需权限）
            },
            {
                "title": "From 1",
                "image": "",
                "id": "Form1",
                "scores": {},
                "run": "Menu1", // 使用该按钮时，当所有条件都满足时，所打开的表单 ID
                "type": "from", // 按钮类型（from 为表单）
                "permission": 0
            },
            {
                "title": "From 2",
                "image": "",
                "id": "Form2",
                "scores": {},
                "run": "Menu2",
                "type": "from",
                "permission": 0
            },
            {
                "title": "OP Button 1",
                "image": "",
                "id": "Button2",
                "run": [ 
                    "execute as ${player} run say OPButton1"
                ],
                "type": "button",
                "permission": 2 // 按钮所需的权限等级（2 为 OP 权限）
            },
            {
                "title": "OP From 1",
                "image": "",
                "id": "Button3",
                "run": "Menu1",
                "type": "from",
                "permission": 2
            }
        ],
        "permission": 0 // 表单所需的权限等级
    },
    "Menu1": {
        "title": "Menu 1",
        "content": "This is a menu 1",
        "info": {
            "permission": "execute as ${player} run say You do not have permission to use this button",
            "score": "execute as ${player} run say You do not have enough score to use this button"
        },
        "type": "Modal", // 表单类型 （Modal 类似于对话框）
        "confirmButton": { // 确认按钮
            "title": "Confirm",
            "scores": {},
            "run": [ 
                "execute as ${player} run say Confirm"
            ],
            "type": "button",
            "permission": 0
        },
        "cancelButton": { // 取消按钮
            "title": "Cancel",
            "scores": {},
            "run": [ 
                "execute as ${player} run say Cancel" 
            ],
            "type": "button",
            "permission": 0
        },
        "permission": 0
    },
    "Menu2": {
        "title": "Menu2",
        "info": {
            "exit": "execute as ${player} run say Exit Menu"
        },
        "type": "Custom", // 表单类型 （Custom 类似于自定义表单）
        "customize": [ // 自定义表单组件列表
            {
                "title": "Test is a Header", // 组件标题
                "id": "Header", // 组件 ID（不可重复）
                "type": "header" // 组件类型（header 为标题）
            },
            {
                "title": "This is a Menu 2", // 组件标题
                "id": "Label", // 组件 ID（不可重复）
                "type": "Label" // 组件类型（Label 为标签）
            },
            {
                "id": "Divider", // 组件 ID（不可重复）
                "type": "divider" // 组件类型（divider 为分割线）
            },
            {
                "title": "This is a Input",
                "id": "Input",
                "placeholder": "This is a content", // 输入框占位符
                "defaultValue": "default", // 输入框默认值
                "tooltip": "This is a tooltip", // 文本提示
                "type": "Input" // 组件类型（Input 为输入框）
            },
            {
                "title": "Dropdown",
                "id": "Dropdown",
                "options": [ "default" ], // 下拉框选项（必须存在 1 个元素，否则不予解析）
                "defaultValue": 0, // 下拉框默认值索引（从 0 开始）
                "type": "Dropdown" // 组件类型（Dropdown 为下拉框）
            },
            {
                "title": "This is a Toggle",
                "id": "Toggle",
                "defaultValue": false, // 切换框默认值（true 为开，false 为关）
                "type": "Toggle" // 组件类型（Toggle 为切换框）
            },
            {
                "title": "this is a Slider",
                "id": "Slider",
                "min": 0, // 滑块最小值
                "max": 100, // 滑块最大值
                "step": 1, // 滑块步长
                "defaultValue": 50, // 滑块默认值
                "type": "Slider" // 组件类型（Slider 为滑块）
            },
            {
                "title": "This is a StepSlider",
                "id": "StepSlider",
                "options": [ "default1", "default2" ], // 步骤滑块选项（必须存在 2 个元素，否则不予解析）
                "defaultValue": 0, // 步骤滑块默认值索引（从 0 开始）
                "type": "StepSlider" // 组件类型（StepSlider 为步骤滑块）
            }
        ],
        "submit": "Submit", // 提交按钮标题
        "run": [ // 使用该表单时，所执行的命令（对于获取组件内容可通过 {组件ID} 进行获取）
            "execute as ${player} run say Input: {Input}",
            "execute as ${player} run say Dropdown: {Dropdown}",
            "execute as ${player} run say Toggle: {Toggle}",
            "execute as ${player} run say Slider: {Slider}",
            "execute as ${player} run say StepSlider: {StepSlider}"
        ],
        "permission": 0
    }
}
```

### shop.json

> [!NOTE]
> 以下内容取自 LOICollectionA 1.7.2 的 `shop.json` 结构，对于后续版本的 `shop.json` 结构可能会有所不同。

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `shop.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#shop) 进行编辑

```json
{
    "MainBuy": { // 商店 ID（不可重复）
        "title": "Buy Shop Example", // 商店标题
        "content": "This is a shop example", // 商店内容
        "info": { // 部分功能提供（可选）
            "exit": "execute as ${player} run say Exit Shop", // 玩家退出商店时执行命令（其中 ${player} 代表玩家名称）
            "score": "execute as ${player} run say You do not have enough score to buy this item" // 购买部分商品时，玩家没有足够 Score 时所执行命令
        },
        "classiflcation": [ // 分类列表
            {
                "title": "Apple", // 组件标题
                "image": "textures/items/apple", // 组件图标（可选，只支持 path 类型）
                "introduce": "A red apple\nscores: 100", // 组件介绍
                "number": "Buy number", // 购买组件时输入框标题
                "id": "minecraft:apple", // 物品 ID
                "scores": { // 组件所需的 Score
                    "money": 100 // 组件所需的 Score 分数
                },
                "type": "commodity" // 组件类型（commodity 为物品组件）
            },
            {
                "title": "Nbt Apple",
                "image": "textures/items/apple",
                "introduce": "A red apple\nscores: 100",
                "number": "Buy number",
                "nbt": "{Count:2b,Damage:0s,Name:'minecraft:apple',WasPickedUp:0b}", // 对于使用自定义物品，可用 nbt 代替 id 配置
                "scores": {
                    "money": 100
                },
                "type": "commodity"
            },
            {
                "title": "Buy Title Shop",
                "image": "",
                "id": "titleBuy", // 使用该组件时，所打开的表单 ID
                "type": "from" // 组件类型（from 为表单）
            }
        ],
        "type": "buy" // 商店类型（buy 为购买商店）
    },
    "titleBuy": {
        "title": "Buy Title Shop",
        "content": "This is a title shop",
        "info": {
            "exit": "execute as ${player} run say Exit Title Shop",
            "score": "execute as ${player} run say You do not have enough score to buy this item"
        },
        "classiflcation": [
            {
                "title": "Test Title",
                "image": "",
                "introduce": "This is a test title\nscores: 100",
                "confirmButton": "Confirm", // 确认按钮标题
                "cancelButton": "Cancel", // 取消按钮标题
                "id": "Test Title", // 称号 ID
                "time": 24, // 称号持续时间（单位：小时）
                "scores": {
                    "money": 100
                },
                "type": "title" // 组件类型（title 为称号组件）
            }
        ],
        "type": "buy"
    },
    "MainSell": {
        "title": "Sell Shop Example",
        "content": "This is a shop example",
        "info": {
            "exit": "execute as ${player} run say Exit Shop",
            "title": "execute as ${player} run say You do not have enough title to sell", // 使用部分组件时，玩家没有指定称号时所执行命令
            "item": "execute as ${player} run say You do not have enough item to sell" // 使用部分组件时，玩家没有指定物品时所执行命令
        },
        "classiflcation": [
            {
                "title": "Apple",
                "image": "textures/items/apple",
                "introduce": "A red apple\nscores: 100",
                "number": "Sell number",
                "id": "minecraft:apple",
                "scores": {
                    "money": 100
                },
                "type": "commodity"
            },
            {
                "title": "Sell Title Shop",
                "image": "",
                "id": "titleSell",
                "type": "from"
            }
        ],
        "type": "sell" // 商店类型（sell 为出售商店）
    },
    "titleSell": {
        "title": "Sell Title Shop",
        "content": "This is a title shop",
        "info": {
            "exit": "execute as ${player} run say Exit Title Shop",
            "title": "execute as ${player} run say You do not have enough title to sell",
            "item": "execute as ${player} run say You do not have enough item to sell"
        },
        "classiflcation": [
            {
                "title": "Test Title",
                "image": "",
                "introduce": "This is a test title\nscores: 100",
                "confirmButton": "Confirm",
                "cancelButton": "Cancel",
                "id": "Test Title",
                "scores": {
                    "money": 100
                },
                "type": "title"
            }
        ],
        "type": "sell"
    }
}
```

### notice.json

> [!NOTE]
> 以下内容取自 LOICollectionA 1.6.2 的 `notice.json` 结构，对于后续版本的 `notice.json` 结构可能会有所不同。

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `notice.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#notice) 进行编辑

```json
{
    "main": { // 公告ID（不可重复）
        "title": "Test Notice 123", // 公告标题
        "content": [ // 公告内容（支持多行）
            "This is a test text 1", // 第 1 行内容
            "This is a test text 2", // 第 2 行内容
            "This is a test text 3" // 第 3 行内容
        ],
        "priority": 0, // 公告优先级
        "poiontout": true // 公告是否在玩家上线时弹出显示
    }
}
```

### cdk.json

> [!NOTE]
> 以下内容取自 LOICollectionA 1.7.0 的 `cdk.json` 结构，对于后续版本的 `cdk.json` 结构可能会有所不同。

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `cdk.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#cdk) 进行编辑

```json
{
    "cdk": { // CDK ID（不可重复）
        "personal": false, // 是否只能被总换一次
        "player": [], // 总换 CDK 的玩家列表
        "scores": { // CDK 给予的Score（可选）
            "money": 100 // CDK 给予的 Score 分数
        },
        "item": [ // CDK 给予的物品（可选）
            { 
                "id": "minecraft:apple", // 物品 ID
                "name": "apple", // 物品名称
                "quantity": 1, // 物品数量
                "specialvalue": 0, // 物品特殊值
                "type": "universal" // 物品解析类型
            },
            {
                "id": "{Count:2b,Damage:0s,Name:'minecraft:apple',WasPickedUp:0b}",
                "type": "nbt"
            }
        ],
        "title": { // CDK 给予的称号（可选）
            "None": 0 // 称号 ID（值为 0 表示永久称号）
        },
        "time": "0" // CDK 删除时间（具体格式为 %Y%m%d%H%M%S，为 0 时永久存在）
    }
}
```

---

> [!DANGER]
> 在服务器启动时，请不要直接修改数据文件，否则极有可能丢失数据内容。
