# 数据文件

> 数据文件是指存储在数据库中的数据文件。数据文件是数据库的核心，它存储了数据库中的所有数据。数据文件可以是文本文件、二进制文件或者其他类型的文件。数据文件的格式和内容取决于数据库的类型和应用场景。  
> 目前 `LOICollectionA` 支持 `Json` 和 `SQLite` 两种数据文件格式。其中只有 `Json` 格式的数据文件可以被直接修改。而对于 `SQLite` 格式的数据文件，我们是不建议您直接修改的。

?> 通常情况下，您不需要手动修改数据文件，因为在使用 `LOICollectionA` 的过程中，对于指定数据文件是存在内部编辑器的，您可以通过它们进行更加快捷的编辑。

## menu.json 
> 以下是为 LOICollectionA 1.4.9 的 `menu.json` 结构，对于后续版本的 `menu.json` 结构可能会有所不同。 

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `menu.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#menu) 进行编辑


```json
{
    "main": { // 表单 ID（同时 main 也为表单入口，不可不存在）
        "title": "Menu Example", // 表单标题
        "content": "This is a menu example", // 表单内容
        "exit": "execute as ${player} run say Exit Menu", // 玩家退出表单时执行命令（其中 ${player} 代表玩家名称）
        "NoPermission": "execute as ${player} run say You do not have permission to use this button", // 使用部分按钮时，玩家没有权限时所执行命令
        "NoScore": "execute as ${player} run say You do not have enough score to use this button", // 使用部分按钮时，玩家没有足够 Score 时所执行命令
        "type": "Simple", // 表单类型 （Simple 类似于按钮列表）
        "button": [ // 按钮列表
            {
                "title": "Button 1", // 按钮标题
                "image": "",  // 按钮图标（可选，只支持 path 类型）
                "id": "Button1", // 按钮 ID（不可重复）
                "scores": { // 按钮所需的 Score（可选）
                    "money": 100 // 按钮所需的 Score 分数
                },
                "command": [ // 使用该按钮时，当所有条件都满足时，所执行的命令（其中 ${player} 代表玩家名称）
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
                "menu": "Menu1", // 使用该按钮时，当所有条件都满足时，所打开的表单 ID
                "type": "from", // 按钮类型（from 为表单）
                "permission": 0
            },
            {
                "title": "From 2",
                "image": "",
                "id": "Form2",
                "scores": {},
                "menu": "Menu2",
                "type": "from",
                "permission": 0
            },
            {
                "title": "OP Button 1",
                "image": "",
                "id": "Button2",
                "command": [ 
                    "execute as ${player} run say OPButton1"
                ],
                "type": "button",
                "permission": 2 // 按钮所需的权限等级（2 为 OP 权限）
            },
            {
                "title": "OP From 1",
                "image": "",
                "id": "Button3",
                "menu": "Menu1",
                "type": "from",
                "permission": 2
            }
        ],
        "permission": 0 // 表单所需的权限等级
    },
    "Menu1": {
        "title": "Menu 1",
        "content": "This is a menu 1",
        "NoPermission": "execute as ${player} run say You do not have permission to use this button",
        "NoScore": "execute as ${player} run say You do not have enough score to use this button",
        "type": "Modal", // 表单类型 （Modal 类似于对话框）
        "confirmButton": { // 确认按钮
            "title": "Confirm",
            "scores": {},
            "command": [ 
                "execute as ${player} run say Confirm"
            ],
            "type": "button",
            "permission": 0
        },
        "cancelButton": { // 取消按钮
            "title": "Cancel",
            "scores": {},
            "command": [ 
                "execute as ${player} run say Cancel" 
            ],
            "type": "button",
            "permission": 0
        },
        "permission": 0
    },
    "Menu2": {
        "title": "Menu2",
        "exit": "execute as ${player} run say Exit Menu",
        "type": "Custom", // 表单类型 （Custom 类似于自定义表单）
        "customize": [ // 自定义表单组件列表
            {
                "title": "This is a Menu 2", // 组件标题
                "id": "Label", // 组件 ID（不可重复）
                "type": "Label" // 组件类型（Label 为标签）
            },
            {
                "title": "This is a Input",
                "id": "Input",
                "placeholder": "This is a content", // 输入框占位符
                "defaultValue": "default", // 输入框默认值
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
        "command": [ // 使用该表单时，所执行的命令（对于获取组件内容可通过 {组件ID} 进行获取）
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

## shop.json
> 以下是为 LOICollectionA 1.5.0 的 `shop.json` 结构，对于后续版本的 `shop.json` 结构可能会有所不同。

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `shop.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#shop) 进行编辑

```json
{
    "MainBuy": { // 商店 ID（不可重复）
        "title": "Buy Shop Example", // 商店标题
        "content": "This is a shop example", // 商店内容
        "exit": "execute as ${player} run say Exit Shop", // 玩家退出商店时执行命令（其中 ${player} 代表玩家名称）
        "NoScore": "execute as ${player} run say You do not have enough score to buy this item", // 购买部分商品时，玩家没有足够 Score 时所执行命令
        "classiflcation": [ // 分类列表
            {
                "title": "Apple", // 组件标题
                "image": "textures/items/apple", // 组件图标（可选，只支持 path 类型）
                "introduce": "A red apple\nscores: 100", // 组件介绍
                "number": "Buy number", // 购买组件时输入框标题
                "id": "minecraft:apple", // 物品 ID
                "scores": { // 组件所需的 Score（可选）
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
                "menu": "titleBuy", // 使用该组件时，所打开的表单 ID
                "type": "from" // 组件类型（from 为表单）
            }
        ],
        "type": "buy" // 商店类型（buy 为购买商店）
    },
    "titleBuy": {
        "title": "Buy Title Shop",
        "content": "This is a title shop",
        "exit": "execute as ${player} run say Exit Title Shop",
        "NoScore": "execute as ${player} run say You do not have enough score to buy this item",
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
        "exit": "execute as ${player} run say Exit Shop",
        "NoTitle": "execute as ${player} run say You do not have enough title to sell", // 使用部分组件时，玩家没有指定称号时所执行命令
        "NoItem": "execute as ${player} run say You do not have enough item to sell", // 使用部分组件时，玩家没有指定物品时所执行命令
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
                "menu": "titleSell",
                "type": "from"
            }
        ],
        "type": "sell" // 商店类型（sell 为出售商店）
    },
    "titleSell": {
        "title": "Sell Title Shop",
        "content": "This is a title shop",
        "exit": "execute as ${player} run say Exit Title Shop",
        "NoTitle": "execute as ${player} run say You do not have enough title to sell",
        "NoItem": "execute as ${player} run say You do not have enough item to sell",
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

## announcement.json
> 以下是为 LOICollectionA 1.4.9 的 `announcement.json` 结构，对于后续版本的 `announcement.json` 结构可能会有所不同。

?> 您可以在 `plugins/LOICollectionA/config` 目录下找到 `announcement.json` 文件。  
对于内部编辑器，您可以通过以下 [命令](./md/command.md#announcement) 进行编辑

```json
{
    "title": "测试公告123", // 公告标题
    "content": [ // 公告内容（支持多行）
        "这是一条测试公告，欢迎使用本插件！", // 第 1 行内容
        "这是第 2 行内容", // 第 2 行内容
        "这是第 3 行内容" // 第 3 行内容
    ]
}
```

## cdk.json
> 以下是为 LOICollectionA 1.4.9 的 `cdk.json` 结构，对于后续版本的 `cdk.json` 结构可能会有所不同。

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
        "item": { // CDK 给予的物品（可选）
            "minecraft:apple": { // 物品 ID
                "name": "apple", // 物品名称
                "quantity": 1, // 物品数量
                "specialvalue": 0 // 物品特殊值
            }
        },
        "title": { // CDK 给予的称号（可选）
            "None": 0 // 称号 ID（值为 0 表示永久称号）
        },
        "time": "0" // CDK 删除时间（具体格式为 %Y%m%d%H%M%S，为 0 时永久存在）
    }
}
```

---

!> 在服务器启动时，请不要直接修改数据文件，否则极有可能丢失数据内容。

?> 以上内容均可通过 `命令` 的方式进行全 GUI 配置。