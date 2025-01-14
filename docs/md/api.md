# LOICollectionAPI
> 以下为 `LOICollectionAPI` 的内容列表

!> 以下为 LOICollectionA 1.6.0 的 `LOICollectionAPI` 结构，对于后续版本的 `LOICollectionAPI` 结构可能会有所不同。

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
{player.pos} | 玩家所在坐标
{player.pos.block} | 玩家所在的方块坐标
{player.pos.lastDeath} | 玩家上次死亡的坐标
{player.realName} | 玩家的真实名字
{player.xuid} | 玩家XUID字符串
{player.uuid} | 玩家Uuid字符串
{player.canFly} | 玩家是否可以飞行
{player.maxHealth} | 玩家最大生命值
{player.health} | 玩家当前生命值
{player.speed} | 玩家当前速度
{player.direction} | 玩家当前朝向
{player.dimension} | 玩家当前维度ID
{player.ip} | 玩家连接IP
{player.xp} | 玩家当前经验
{player.HandItem} | 玩家手持物品名称
{player.OffHand} | 玩家副手持物品名称
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
{score(ScoreboardName)} | 玩家的计分板分数
{tr(languageId)} | 获取玩家当前语言指定ID的翻译文本
{entity(typeid)} | 获取指定ID的实体数量