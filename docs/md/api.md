# LOICollectionAPI

> [!NOTE]
> 以下内容取自 LOICollectionA 1.9.2 的 `LOICollectionAPI` 结构，对于后续版本的 `LOICollectionAPI` 结构可能会有所不同。

## 默认变量

| LOICollectionAPI | 备注 |
| --- | --- |
| {version.mc} | 当前服务器 Minecraft 版本 |
| {version.ll} | 当前服务器 LeviLamina 版本 |
| {version.protocol} | 当前服务器协议版本 |
| {player} | 当前玩家名称 |
| {player.language} | 获取玩家使用的语言ID |
| {player.language.name} | 获取玩家使用的语言名称 |
| {player.title} | 获取玩家当前佩戴称号 |
| {player.title.time} | 获取玩家剩余拥有称号时间 |
| {player.mute} | 玩家是否被禁言 |
| {player.pvp} | 玩家是否开启PVP |
| {player.statistcs.onlinetime} | 玩家在线时长 |
| {player.statistcs.kills"} | 玩家击杀生物数量 |
| {player.statistcs.deaths} | 玩家死亡次数 |
| {player.statistcs.place} | 玩家放置方块数量 |
| {player.statistcs.destroy} | 玩家破坏方块数量 |
| {player.statistcs.respawn} | 玩家重生次数 |
| {player.statistcs.join} | 玩家加入服务器次数 |
| {player.gamemode} | 玩家当前游戏模式 |
| {player.pos} | 玩家所在坐标 |
| {player.pos.x} | 玩家所在X坐标 |
| {player.pos.y} | 玩家所在Y坐标 |
| {player.pos.z} | 玩家所在Z坐标 |
| {player.pos.respawn} | 玩家重生坐标 |
| {player.pos.respawn.x} | 玩家重生X坐标 |
| {player.pos.respawn.y} | 玩家重生Y坐标 |
| {player.pos.respawn.z} | 玩家重生Z坐标 |
| {player.pos.block} | 玩家所在的方块坐标 |
| {player.pos.lastdeath} | 玩家上次死亡的坐标 |
| {player.realname} | 玩家的真实名字 |
| {player.xuid} | 玩家XUID字符串 |
| {player.uuid} | 玩家Uuid字符串 |
| {player.is.op} | 玩家是否为OP |
| {player.can.fly} | 玩家是否可以飞行 |
| {player.health} | 玩家当前生命值 |
| {player.max.health} | 玩家最大生命值 |
| {player.hunger} | 玩家当前饥饿值 |
| {player.max.hunger} | 玩家最大饥饿值 |
| {player.saturation} | 玩家当前饱和度 |
| {player.max.saturation} | 玩家最大饱和度 |
| {player.speed} | 玩家当前速度 |
| {player.direction} | 玩家当前朝向 |
| {player.dimension} | 玩家当前维度ID |
| {player.ip} | 玩家连接IP |
| {player.exp.xp} | 玩家当前经验 |
| {player.exp.level} | 玩家当前等级 |
| {player.exp.level.next} | 玩家下一等级所需经验 |
| {player.handitem} | 玩家手持物品名称 |
| {player.offhand} | 玩家副手持物品名称 |
| {player.os} | 玩家设备名称 |
| {player.ms} | 玩家的网络延迟时间（ms） |
| {player.ms.avg} | 玩家的平均网络延迟时间（ms） |
| {player.packet} | 玩家的网络丢包率（%） |
| {player.packet.avg} | 玩家的平均网络丢包率（%） |
| {server.tps} | 获取当前服务器TPS |
| {server.mspt} | 获取当前服务器MSPT |
| {server.time} | 当前时间 |
| {server.player.max} | 最大玩家数量 |
| {server.player.online} | 在线玩家数量 |
| {server.entity} | 当前服务器实体数量 |
| {score(ScoreboardName)} | 玩家的计分板分数 |
| {tr(languageId)} | 获取玩家当前语言指定ID的翻译文本 |
| {entity(typeid)} | 获取指定ID的实体数量 |

## 阻断标识符 - $

> 阻断标识符用于阻止变量的替换，即原变量内容将保留原样，不会被解析为其他内容。

```text
${variable}
```

| 参数 | 备注 |
| --- | --- |
| variable | 变量名 |

> [!TIP]
> 阻断标识符通常用于变量前，且在完成替换后标识符会被移除。

### **阻断标识符具体使用实例**  

以下将展示玩家的真实名字，且不会解析其他内容

```text
{player.realname} -> ${team}
```

- 返回结果：`player -> {team}`

## 判断语句 - if

> 判断语句用于判断条件是否成立，如果成立则将`result_yes` 的内容替换掉原语句，否则将以 `result_no` 的内容替换掉原语句  

```text
@if (condition)[result_yes : result_no]@
```

| 参数 | 备注 |
| --- | --- |
| condition | 判断条件 |
| result_yes | 条件成立时替换的内容 |
| result_no | 条件不成立时替换的内容 |

> [!TIP]  
> 在判断语句中是支持 `if` 嵌套的，但建议不要嵌套过多，否则会导致语句难以阅读  

### **判断语句具体使用实例**  

以下将以不同的颜色展示玩家的平均网络延迟时间

!> 需要注意的是语句的原 `result` 无法解析特殊字符，需要将其整合为字符串类型

```text
@if({player.ms.avg} <= 50)["§a" : if( {player.ms.avg} > 50 && {player.ms.avg} <= 250)["§e" : "§c"]]@ {player.ms.avg}§bms
```

- **当判断条件满足 `{player.ms.avg} <= 50` 时，其会返回 "§a {player.ms.avg}§bms"**  
- **当判断条件满足 `{player.ms.avg} > 50 && {player.ms.avg} <= 250` 时，其会返回 "§e {player.ms.avg}§bms"**  
- **当判断条件不满足以上条件时，其会返回 "§c {player.ms.avg}§bms"**

## 运算符 - math

> 运算符通常是用于原始语句中进行更加方便快捷的处理工具，包括但不限于 `+`、`-`、`*`、`/` 等。

| 运算符1 | 说明 | 运算符2 | 说明 |
| --- | --- | --- | --- |
| + | 加法 | - | 减法 |
| * | 乘法 | / | 除法 |
| % | 取模 | ^ | 幂运算 |
| ! | 逻辑非 | && | 逻辑与 |
| \|\| | 逻辑或 | == | 等于 |
| != | 不等于 | > | 大于 |
| < | 小于 | >= | 大于等于 |
| <= | 小于等于 | ... | ... |

### **运算符具体使用实例**  

以上内容中的大部分均为表面含义，可直接使用，故不再赘述。  
接下来将展示一些`特殊用法`。

1. 字符串拼接

    ```text
    "Hello" + "World"
    ```

    - 返回结果："HelloWorld"

2. 字符串比较

    ```text
    "Hello" <= "World"
    ```

    - 返回结果：false

    > [!NOTE]
    > 这里的比较大致可以参考 `C++` 的比较规则

## 函数 - function

> 函数是一些常用的功能的集合，可以帮助我们更加方便的进行一些操作。  
> 以下将提供一些内置的函数，更多函数可自行进行扩展。

```text
@namespaces::function_name(parameter1, parameter2,...)@
```

| 参数 | 备注 |
| --- | --- |
| namespaces | 命名空间，用于区分不同功能 |
| function_name | 函数名 |
| parameter1 | 参数1 |
| parameter2 | 参数2 |
| ... | ... |

> [!WARNING]
> 在函数调用中，参数最大数量只支持到 `100` 个，超出数量将会解析失败

### 数学函数 - math

> [!TIP]
> 以下内容中 `number` 代表的是 `int` 或 `float` 类型的数据

| 函数名 | 说明 | 参数类型 | 函数名 | 说明 | 参数类型 |
| --- | --- | --- | --- | --- | --- |
| abs | 取绝对值 | number | min | 取最小值 | number, number |
| max | 取最大值 | number, number | sqrt | 开平方 | number |
| pow | 求次方 | number, number | log | 取对数 | number |
| sin | 正弦 | number | cos | 余弦 | number |
| random | 随机数 | number, number | ... | ... | ... |

### 字符串函数 - string

| 函数名 | 说明 | 参数类型 | 函数名 | 说明 | 参数类型 |
| --- | --- | --- | --- | --- | --- |
| length | 字符串长度 | string | upper | 转大写 | string |
| lower | 转小写 | string | substr | 取子串 | string, start, int |
| trim | 去除首尾空格 | string | replace | 替换字符串 | string, string, string |

### **函数具体使用实例**

以下将展示计算 `cos(sqrt(100))` 和 `sin(sqrt(100))` 的最大值，并取其绝对值。

?> 需要注意的是，函数的调用需要使用 `::` 进行分隔，且函数的参数需要使用 `,` 进行分隔。

```text
@math::abs(math::max(math::cos(math::sqrt(100)), math::sin(math::sqrt(100))))@
```

- 返回结果：0.544021
