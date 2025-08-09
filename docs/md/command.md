# 命令列表

在 LOIColletionA 中，对于不同的 `内置插件` 均提供了不同的命令列表。  
而插件则提供以下命令以进行简单的交互：

> [!TIP]
> 在命令提示中的 `<>` 为必填参数，`[]` 为可选参数。

## Blacklist

```log
[Server] blacklist:
[Server] LOICollection -> 服务器黑名单
[Server] Usage:
[Server] - /blacklist add &ltTarget: target&gt [Cause: string] [Time: int]
[Server] - /blacklist gui
[Server] - /blacklist info &ltId: string&gt
[Server] - /blacklist list
[Server] - /blacklist remove &ltId: string&gt
```

?> 其中 `blacklist` 为 Blacklist 的顶层命令（权限等级: GameDirectors）。

- `/blacklist add <Target: target> [Cause: string] [Time: int]`
  - 向黑名单中添加一个目标。
  - 其中 `<Target: target>` 为目标选择器。
  - 其中 `<Cause: string>` 为添加的原因。
  - 其中 `<Time: int>` 为添加的时间（单位：小时）。

- `/blacklist gui`
  - 打开黑名单 GUI。

- `/blacklist info <Id: string>`
  - 查看黑名单中的一个目标的信息。
  - 其中 `<Id: string>` 为目标的ID。

- `/blacklist list`
  - 列出黑名单中的所有目标。

- `/blacklist remove <Id: string>`
  - 从黑名单中移除一个目标。
  - 其中 `<Id: string>` 为目标的ID。

## Mute

```log
[Server] mute:
[Server] LOICollection -> 服务器禁言
[Server] Usage:
[Server] - /mute add &ltTarget: target&gt [Cause: string] [Time: int]
[Server] - /mute gui
[Server] - /mute remove &ltTarget: target&gt
```

?> 其中 `mute` 为 Mute 的顶层命令（权限等级: GameDirectors）。

- `/mute add <Target: target> [Cause: string] [Time: int]`
  - 向禁言列表中添加一个目标。
  - 其中 `<Target: target>` 为目标选择器。
  - 其中 `<Cause: string>` 为添加的原因。
  - 其中 `<Time: int>` 为添加的时间（单位：小时）。

- `/mute gui`
  - 打开禁言 GUI。

- `/mute remove <Target: target>`
  - 从禁言列表中移除一个目标。
  - 其中 `<Target: target>` 为目标选择器。

## Cdk

```log
[Server] cdk:
[Server] LOICollection -> 总换码
[Server] Usage:
[Server] - /cdk convert &ltId: string&gt
[Server] - /cdk edit
[Server] - /cdk gui
```

?> 其中 `cdk` 为 Cdk 的顶层命令（权限等级: Any）。

- `/cdk convert <Id: string>`
  - 总换CDK。
  - 其中 `<Id: string>` 为要总换的CDK。

- `/cdk edit`
  - 打开总换CDK设置（权限等级: GameDirectors）。

- `/cdk gui`
  - 打开总换CDK GUI。

## Menu

```log
? menu
[Server] menu:
[Server] LOICollection -> 服务器菜单
[Server] Usage:
[Server] - /menu clock
[Server] - /menu edit
[Server] - /menu gui [Id: string]
```

?> 其中 `menu` 为 Menu 的顶层命令（权限等级: Any）。

- `/menu clock`
  - 获取便携打开菜单物品。

- `/menu edit`
  - 打开菜单编辑界面（权限等级: GameDirectors）。

- `/menu gui [Id: string]`
  - 打开菜单 GUI。
  - 其中 `<Id: string>` 为菜单ID。

## Tpa

```log
[Server] tpa:
[Server] LOICollection -> 玩家互传
[Server] Usage:
[Server] - /tpa gui
[Server] - /tpa invite &lttpa|tphere&gt &ltTarget: target&gt
[Server] - /tpa setting
```

?> 其中 `tpa` 为 Tpa 的顶层命令（权限等级: Any）。

- `tpa invite <tpa|tphere> <Target: target>`
  - 向目标发送传送请求。
  - 其中 `<tpa|tphere>` 为传送类型。
  - 其中 `<Target: target>` 为目标选择器。

- `/tpa gui`
  - 打开 Tpa GUI。

- `/tpa setting`
  - 打开 Tpa 个人设置。

## Shop

```log
[Server] shop:
[Server] LOICollection -> 服务器商店
[Server] Usage:
[Server] - /shop edit
[Server] - /shop gui &ltId: string&gt
```

?> 其中 `shop` 为 Shop 的顶层命令（权限等级: Any）。

- `/shop edit`
  - 打开商店编辑界面（权限等级: GameDirectors）。

- `/shop gui <Id: string>`
  - 打开商店 GUI。
  - 其中 `<Id: string>` 为商店ID。

## Pvp

```log
[Server] pvp:
[Server] LOICollection -> 服务器PVP
[Server] Usage:
[Server] - /pvp gui
[Server] - /pvp off
[Server] - /pvp on
```

?> 其中 `pvp` 为 Pvp 的顶层命令（权限等级: Any）。

- `/pvp gui`
  - 打开 Pvp GUI。

- `/pvp off`
  - 关闭 Pvp。

- `/pvp on`
  - 开启 Pvp。

## Wallet

```log
? wallet
[Server] wallet:
[Server] LOICollection -> 个人钱包
[Server] Usage:
[Server] - /wallet gui
[Server] - /wallet transfer &ltTarget: target&gt &ltScore: int&gt
[Server] - /wallet wealth
```

?> 其中 `wallet` 为 Wallet 的顶层命令（权限等级: Any）。

- `/wallet gui`
  - 打开个人钱包 GUI。

- `/wallet transfer <Target: target> <Score: int>`
  - 向目标转账。
  - 其中 `<Target: target>` 为目标选择器。
  - 其中 `<Score: int>` 为转账金额。

- `/wallet wealth`
  - 查看个人钱包余额。

## Chat

```log
[Server] chat:
[Server] LOICollection -> 个人称号
[Server] Usage:
[Server] - /chat add &ltTarget: target&gt &ltTitle: string&gt [Time: int]
[Server] - /chat gui
[Server] - /chat list &ltTarget: target&gt
[Server] - /chat remove &ltTarget: target&gt &ltTitle: string&gt
[Server] - /chat set &ltTarget: target&gt &ltTitle: string&gt
[Server] - /chat setting
```

?> 其中 `chat` 为 Chat 的顶层命令（权限等级: Any）。

- `/chat add <Target: target> <Title: string> [Time: int]`
  - 为目标添加指定称号（权限等级: GameDirectors）。
  - 其中 `<Target: target>` 为目标选择器。
  - 其中 `<Title: string>` 为称号名称。
  - 其中 `[Time: int]` 为称号时间（单位：小时）。

- `/chat remove <Target: target> <Title: string>`
  - 为目标移除指定称号（权限等级: GameDirectors）。
  - 其中 `<Target: target>` 为目标选择器。
  - 其中 `<Title: string>` 为称号名称。

- `/chat set <Target: target> <Title: string>`
  - 为目标设置指定称号（权限等级: GameDirectors）。
  - 其中 `<Target: target>` 为目标选择器。
  - 其中 `<Title: string>` 为称号名称。
  - 注: 无法设置未拥有的称号。

- `/chat list <Target: target>`
  - 查看目标的所有称号（权限等级: GameDirectors）。
  - 其中 `<Target: target>` 为目标选择器。

- `/chat gui`
  - 打开管理称号 GUI（权限等级: GameDirectors）。

- `/chat setting`
  - 打开个人称号设置。

## Notice

```log
[Server] notice:
[Server] LOICollection -> 公告系统
[Server] Usage:
[Server] - /notice edit
[Server] - /notice gui [Id: string]
[Server] - /notice setting
```

?> 其中 `notice` 为 notice 的顶层命令（权限等级: Any）。

- `/notice edit`
  - 打开公告编辑界面（权限等级: GameDirectors）。

- `/notice gui [Id: string]`
  - 打开公告 GUI。
  - 其中 `[Id: string]` 为公告ID。

- `/notice setting`
  - 打开公告个人设置。

## Market

```log
[Server] market:
[Server] LOICollection -> 玩家市场
[Server] Usage:
[Server] - /market gui
[Server] - /market sell
```

?> 其中 `market` 为 Market 的顶层命令（权限等级: Any）。

- `/market gui`
  - 打开玩家市场 GUI。

- `/market sell`
  - 打开玩家市场出售界面。

## BehaviourEvent

```log
[Server] behaviorevent:
[Server] LOICollection -> 行为事件
[Server] 使用：
[Server] - /behaviorevent back position &ltPositionOrigin: x y z&gt &ltPositionTarget: x y z&gt &ltTime: int&gt
[Server] - /behaviorevent back range &ltPositionOrigin: x y z&gt &ltRadius: int&gt &ltTime: int&gt
[Server] - /behaviorevent clean
[Server] - /behaviorevent query action position &ltPositionOrigin: x y z&gt &ltPositionTarget: x y z&gt
[Server] - /behaviorevent query action range &ltPositionOrigin: x y z&gt &ltRadius: int&gt
[Server] - /behaviorevent query event custom &ltTarget: string&gt &ltValue: string&gt
[Server] - /behaviorevent query event dimension &ltDimension: Dimension&gt
[Server] - /behaviorevent query event foundation &ltEventName: string&gt &ltTime: int&gt
[Server] - /behaviorevent query event info &ltEventId: string&gt
[Server] - /behaviorevent query event name &ltEventName: string&gtW
[Server] - /behaviorevent query event position &ltPositionOrigin: x y z&gt
[Server] - /behaviorevent query event site &ltPositionOrigin: x y z&gt &ltDimension: Dimension&gt
[Server] - /behaviorevent query event time &ltTime: int&gt
```

?> 其中 `behaviorevent` 为 BehaviourEvent 的顶层命令（权限等级: GameDirectors）。

- `/behaviorevent back position <PositionOrigin: x y z> <PositionTarget: x y z> <Time: int>`
  - 向指定区域内执行行为事件回溯。
  - 其中 `<PositionOrigin: x y z>` 为行为事件回溯的起始位置。
  - 其中 `<PositionTarget: x y z>` 为行为事件回溯的目标位置。
  - 其中 `<Time: int>` 为回溯时间（单位：小时）。

- `/behaviorevent back range <PositionOrigin: x y z> <Radius: int> <Time: int>`
  - 向指定区间内执行行为事件回溯。
  - 其中 `<PositionOrigin: x y z>` 为行为事件回溯的起始位置。
  - 其中 `<Radius: int>` 为行为事件回溯的半径。
  - 其中 `<Time: int>` 为回溯时间（单位：小时）。

- `/behaviorevent clean`
  - 清除所有满足条件的行为事件记录。

- `/behaviorevent query action position <PositionOrigin: x y z> <PositionTarget: x y z>`
  - 查询指定区域内的行为事件。
  - 其中 `<PositionOrigin: x y z>` 为行为事件查询的起始位置。
  - 其中 `<PositionTarget: x y z>` 为行为事件查询的目标位置。

- `/behaviorevent query action range <PositionOrigin: x y z> <Radius: int>`
  - 查询指定区间内的行为事件。
  - 其中 `<PositionOrigin: x y z>` 为行为事件查询的起始位置。
  - 其中 `<Radius: int>` 为行为事件查询的半径。

- `/behaviorevent query event custom <Target: string> <Value: string>`
  - 查询指定自定义事件。
  - 其中 `<Target: string>` 为事件目标。
  - 其中 `<Value: string>` 为事件值。

- `/behaviorevent query event dimension <Dimension: Dimension>`
  - 查询指定维度的行为事件。
  - 其中 `<Dimension: Dimension>` 为事件维度。

- `/behaviorevent query event foundation <EventName: string> <Time: int>`
  - 查询指定基础信息的行为事件。
  - 其中 `<EventName: string>` 为事件名称。
  - 其中 `<Time: int>` 为事件发生时间。

- `/behaviorevent query event info <EventId: string>`
  - 查询指定事件的详细信息。
  - 其中 `<EventId: string>` 为事件ID。

- `/behaviorevent query event name <EventName: string>`
  - 查询指定名称的行为事件。
  - 其中 `<EventName: string>` 为事件名称。

- `/behaviorevent query event position <PositionOrigin: x y z>`
  - 查询指定位置的行为事件。
  - 其中 `<PositionOrigin: x y z>` 为事件发生位置。

- `/behaviorevent query event site <PositionOrigin: x y z> <Dimension: Dimension>`
  - 查询指定区域的行为事件。
  - 其中 `<PositionOrigin: x y z>` 为事件发生位置。
  - 其中 `<Dimension: Dimension>` 为事件发生维度。

- `/behaviorevent query event time <Time: int>`
  - 查询指定时间范围内的行为事件。
  - 其中 `<Time: int>` 为事件发生时间。

> [!NOTE]
> 以上内容均属于 LOICollectionA 1.7.0 版本的命令列表，对于后续版本的命令列表可能会有所不同。
