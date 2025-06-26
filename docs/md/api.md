# LOICollectionAPI
> [!NOTE]
> 以下内容取自 LOICollectionA 1.6.4 的 `LOICollectionAPI` 结构，对于后续版本的 `LOICollectionAPI` 结构可能会有所不同。

## 默认变量
| LOICollectionAPI | 备注 |
| --- | --- |
{version.mc} | 当前服务器 Minecraft 版本
{version.ll} | 当前服务器 LeviLamina 版本
{version.protocol} | 当前服务器协议版本
{player} | 当前玩家名称
{player.language} | 获取玩家使用的语言ID
{player.language.name} | 获取玩家使用的语言名称
{player.title} | 获取玩家当前佩戴称号
{player.title.time} | 获取玩家剩余拥有称号时间
{player.mute} | 玩家是否被禁言
{player.pvp} | 玩家是否开启PVP
{player.gamemode} | 玩家当前游戏模式
{player.pos} | 玩家所在坐标
{player.pos.x} | 玩家所在X坐标
{player.pos.y} | 玩家所在Y坐标
{player.pos.z} | 玩家所在Z坐标
{player.pos.respawn} | 玩家重生坐标
{player.pos.respawn.x} | 玩家重生X坐标
{player.pos.respawn.y} | 玩家重生Y坐标
{player.pos.respawn.z} | 玩家重生Z坐标
{player.pos.block} | 玩家所在的方块坐标
{player.pos.lastdeath} | 玩家上次死亡的坐标
{player.realname} | 玩家的真实名字
{player.xuid} | 玩家XUID字符串
{player.uuid} | 玩家Uuid字符串
{player.is.op} | 玩家是否为OP
{player.can.fly} | 玩家是否可以飞行
{player.health} | 玩家当前生命值
{player.max.health} | 玩家最大生命值
{player.hunger} | 玩家当前饥饿值
{player.max.hunger} | 玩家最大饥饿值
{player.saturation} | 玩家当前饱和度
{player.max.saturation} | 玩家最大饱和度
{player.speed} | 玩家当前速度
{player.direction} | 玩家当前朝向
{player.dimension} | 玩家当前维度ID
{player.ip} | 玩家连接IP
{player.exp.xp} | 玩家当前经验
{player.exp.level} | 玩家当前等级
{player.exp.level.next} | 玩家下一等级所需经验
{player.handitem} | 玩家手持物品名称
{player.offhand} | 玩家副手持物品名称
{player.os} | 玩家设备名称
{player.ms} | 玩家的网络延迟时间（ms）
{player.ms.avg} | 玩家的平均网络延迟时间（ms）
{player.packet} | 玩家的网络丢包率（%）
{player.packet.avg} | 玩家的平均网络丢包率（%）
{server.tps} | 获取当前服务器TPS
{server.mspt} | 获取当前服务器MSPT
{server.time} | 当前时间
{server.player.max} | 最大玩家数量
{server.player.online} | 在线玩家数量
{server.entity} | 当前服务器实体数量
{score(ScoreboardName)} | 玩家的计分板分数
{tr(languageId)} | 获取玩家当前语言指定ID的翻译文本
{entity(typeid)} | 获取指定ID的实体数量

## 判断语句 - if
> 判断语句用于判断条件是否成立，如果成立则将`result_yes` 的内容替换掉原语句，否则将以 `result_no` 的内容替换掉原语句  

> [!TIP]
> 在判断语句中是支持 `if` 嵌套的，但建议不要嵌套过多，否则会导致语句难以阅读

```text
@if (condition) ? result_yes : result_no@
```

| 参数 | 备注 |
| --- | --- |
| condition | 判断条件 |
| result_yes | 条件成立时替换的内容 |
| result_no | 条件不成立时替换的内容 |

> **具体使用实例**  

以下将以不同的颜色展示玩家的平均网络延迟时间

!> 需要注意的是语句的原 `result` 无法解析特殊字符，需要将其整合为字符串类型

```text
@if({player.ms.avg} <= 50) ? "§a" : if({player.ms.avg} > 50 && {player.ms.avg} <= 250) ? "§e" : "§c"@{player.ms.avg}§bms
```

- **当判断条件满足 `{player.ms.avg} <= 50` 时，其会返回 "<span class="green">{player.ms.avg}</span><span class="blue">ms</span>"**  
- **当判断条件满足 `{player.ms.avg} > 50 && {player.ms.avg} <= 250` 时，其会返回 "<span class="orange">{player.ms.avg}</span><span class="blue">ms</span>"**  
- **当判断条件不满足以上条件时，其会返回 "<span class="red">{player.ms.avg}</span><span class="blue">ms</span>"**
