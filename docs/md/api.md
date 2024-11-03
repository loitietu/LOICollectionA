# LOICollectionAPI
> 以下为 `LOICollectionAPI` 的内容列表

!> 以下为 LOICollectionA 1.4.9 的 `LOICollectionAPI` 结构，对于后续版本的 `LOICollectionAPI` 结构可能会有所不同。

| LOICollectionAPI | 备注 |
| --- | --- |
{language} | 获取玩家使用的语言ID
{languageName} | 获取玩家使用的语言名称
{title} | 获取玩家当前佩戴称号
{title.time} | 获取玩家剩余拥有称号时间
{mute} | 玩家是否被禁言
{pvp} | 玩家是否开启PVP
{tps} | 获取当前服务器TPS
{mspt} | 获取当前服务器MSPT
{mcVersion} | 当前服务器 Minecraft 版本
{llVersion} | 当前服务器 LeviLamina 版本
{protocolVersion} | 当前服务器协议版本
{time} | 当前时间
{tr(languageId)} | 获取指定id文本
{maxPlayer} | 最大玩家数量
{onlinePlayer} | 在线玩家数量
{player} | 当前玩家名称
{pos} | 玩家所在坐标
{blockPos} | 玩家所在的方块坐标
{lastDeathPos} | 玩家上次死亡的坐标
{realName} | 玩家的真实名字
{xuid} | 玩家XUID字符串
{uuid} | 玩家Uuid字符串
{canFly} | 玩家是否可以飞行
{maxHealth} | 玩家最大生命值
{health} | 玩家当前生命值
{speed} | 玩家当前速度
{direction} | 玩家当前朝向
{dimension} | 玩家当前维度ID
{ip} | 玩家连接IP
{xp} | 玩家当前经验
{HandItem} | 玩家手持物品名称
{OffHand} | 玩家副手持物品名称
{os} | 玩家设备名称
{ms} | 玩家的网络延迟时间（ms）
{avgms} | 玩家的平均网络延迟时间（ms）
{Packet} | 玩家的网络丢包率（%）
{avgPacket} | 玩家的平均网络丢包率（%）
{score(ScoreboardName)} | 玩家的计分板分数