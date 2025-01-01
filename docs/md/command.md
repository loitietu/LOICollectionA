# 命令列表
> 在 LOIColletionA 中，对于不同的 `内置插件` 均提供了不同的命令列表。  
> 而插件则提供以下命令以进行简单的交互：

?> 在命令提示中的 `<>` 为必填参数，`[]` 为可选参数。

## Blacklist
```log
? blacklist
11:44:58.298 INFO [Server] blacklist:
11:44:58.298 INFO [Server] LOICollection -> 服务器黑名单
11:44:58.298 INFO [Server] Usage:
11:44:58.298 INFO [Server] - /blacklist add <ip|uuid> <target: target>
11:44:58.298 INFO [Server] - /blacklist add <ip|uuid> <target: target> <cause: string>
11:44:58.298 INFO [Server] - /blacklist add <ip|uuid> <target: target> <cause: string> <time: int>
11:44:58.298 INFO [Server] - /blacklist add <ip|uuid> <target: target> <time: int>
11:44:58.298 INFO [Server] - /blacklist gui
11:44:58.298 INFO [Server] - /blacklist list
11:44:58.298 INFO [Server] - /blacklist remove <targetName: string>
```
?> 其中 `blacklist` 为 Blacklist 的顶层命令（权限等级: GameDirectors）。

- `/blacklist add <ip|uuid> <target: target> <cause: string> <time: int>`
  - 向黑名单中添加一个目标。
  - 其中 `<ip|uuid>` 为目标的 IP 或 UUID。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<cause: string>` 为添加的原因。
  - 其中 `<time: int>` 为添加的时间（单位：小时）。

- `/blacklist gui`
  - 打开黑名单 GUI。

- `/blacklist list`
  - 列出黑名单中的所有目标。

- `/blacklist remove <targetName: string>`
  - 从黑名单中移除一个目标。
  - 其中 `<targetName: string>` 为目标的ID。

## Mute
```log
? mute
11:45:36.007 INFO [Server] mute:
11:45:36.007 INFO [Server] LOICollection -> 服务器禁言
11:45:36.007 INFO [Server] Usage:
11:45:36.007 INFO [Server] - /mute add <target: target>
11:45:36.007 INFO [Server] - /mute add <target: target> <cause: string>
11:45:36.007 INFO [Server] - /mute add <target: target> <cause: string> <time: int>
11:45:36.007 INFO [Server] - /mute add <target: target> <time: int>
11:45:36.007 INFO [Server] - /mute gui
11:45:36.007 INFO [Server] - /mute remove <target: target>
```

?> 其中 `mute` 为 Mute 的顶层命令（权限等级: GameDirectors）。

- `/mute add <target: target> <cause: string> <time: int>`
  - 向禁言列表中添加一个目标。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<cause: string>` 为添加的原因。
  - 其中 `<time: int>` 为添加的时间（单位：小时）。

- `/mute gui`
  - 打开禁言 GUI。

- `/mute remove <target: target>`
  - 从禁言列表中移除一个目标。
  - 其中 `<target: target>` 为目标选择器。

## Cdk
```log
? cdk
15:52:33.008 INFO [Server] cdk:
15:52:33.010 INFO [Server] LOICollection -> 总换码
15:52:33.010 INFO [Server] Usage:
15:52:33.010 INFO [Server] - /cdk convert <convertString: string>
15:52:33.010 INFO [Server] - /cdk edit
15:52:33.010 INFO [Server] - /cdk gui
```

?> 其中 `cdk` 为 Cdk 的顶层命令（权限等级: Any）。

- `/cdk convert <convertString: string>`
  - 总换CDK。
  - 其中 `<convertString: string>` 为要总换的CDK。

- `/cdk gui`
  - 打开总换CDK GUI。

- `/cdk setting`
  - 打开总换CDK设置（权限等级: GameDirectors）。

## Menu
```log
? menu
11:56:05.721 INFO [Server] menu:
11:56:05.721 INFO [Server] LOICollection -> 服务器菜单
11:56:05.723 INFO [Server] Usage:
11:56:05.724 INFO [Server] - /menu clock
11:56:05.725 INFO [Server] - /menu edit
11:56:05.725 INFO [Server] - /menu gui [uiName: string]
```

?> 其中 `menu` 为 Menu 的顶层命令（权限等级: Any）。

- `/menu clock`
  - 获取便携打开菜单物品。

- `/menu edit`
  - 打开菜单编辑界面（权限等级: GameDirectors）。

- `/menu gui [uiName: string]`
  - 打开菜单 GUI。
  - 其中 `<uiName: string>` 为菜单ID。

## Tpa
```log
? tpa
10:36:58.505 INFO [Server] tpa:
10:36:58.506 INFO [Server] LOICollection -> 玩家互传
10:36:58.508 INFO [Server] Usage:
10:36:58.509 INFO [Server] - /tpa gui
10:36:58.510 INFO [Server] - /tpa invite <tpa|tphere> <target: target>
```

?> 其中 `tpa` 为 Tpa 的顶层命令（权限等级: Any）。

- `tpa invite <tpa|tphere> <target: target>`
  - 向目标发送传送请求。
  - 其中 `<tpa|tphere>` 为传送类型。
  - 其中 `<target: target>` 为目标选择器。

- `/tpa gui`
  - 打开 Tpa GUI。

## Shop
```log
? shop
11:58:16.902 INFO [Server] shop:
11:58:16.902 INFO [Server] LOICollection -> 服务器商店
11:58:16.902 INFO [Server] Usage:
11:58:16.905 INFO [Server] - /shop edit
11:58:16.905 INFO [Server] - /shop gui <uiName: string>
```

?> 其中 `shop` 为 Shop 的顶层命令（权限等级: Any）。

- `/shop edit`
  - 打开商店编辑界面（权限等级: GameDirectors）。

- `/shop gui <uiName: string>`
  - 打开商店 GUI。
  - 其中 `<uiName: string>` 为商店ID。

## Wallet
```log
? wallet
15:33:39.440 INFO [Server] wallet:
15:33:39.440 INFO [Server] LOICollection -> 个人钱包
15:33:39.440 INFO [Server] Usage:
15:33:39.440 INFO [Server] - /wallet gui
15:33:39.440 INFO [Server] - /wallet transfer <target: target> <score: int>
15:33:39.440 INFO [Server] - /wallet wealth
```

?> 其中 `wallet` 为 Wallet 的顶层命令（权限等级: Any）。

- `/wallet gui`
  - 打开个人钱包 GUI。

- `/wallet transfer <target: target> <score: int>`
  - 向目标转账。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<score: int>` 为转账金额。

- `/wallet wealth`
  - 查看个人钱包余额。

## Chat
```log
? chat
15:55:41.904 INFO [Server] chat:
15:55:41.904 INFO [Server] LOICollection -> 个人称号
15:55:41.904 INFO [Server] Usage:
15:55:41.904 INFO [Server] - /chat add <target: target> <titleName: string> [time: int]
15:55:41.904 INFO [Server] - /chat gui
15:55:41.904 INFO [Server] - /chat list <target: target>
15:55:41.904 INFO [Server] - /chat remove <target: target> <titleName: string>
15:55:41.904 INFO [Server] - /chat set <target: target> <titleName: string>
```

?> 其中 `chat` 为 Chat 的顶层命令（权限等级: GameDirectors）。

- `/chat add <target: target> <titleName: string> [time: int]`
  - 为目标添加指定称号。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<titleName: string>` 为称号名称。
  - 其中 `[time: int]` 为称号时间（单位：小时）。

- `/chat remove <target: target> <titleName: string>`
  - 为目标移除指定称号。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<titleName: string>` 为称号名称。

- `/chat set <target: target> <titleName: string>`
  - 为目标设置指定称号。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<titleName: string>` 为称号名称。
  - 注: 无法设置未拥有的称号。

- `/chat list <target: target>`
  - 查看目标的所有称号。
  - 其中 `<target: target>` 为目标选择器。

- `/chat gui`
  - 打开管理称号 GUI。

## AnnounCement
```log
? announcement
12:02:20.000 INFO [Server] announcement:
12:02:20.000 INFO [Server] LOICollection -> 公告系统
12:02:20.001 INFO [Server] Usage:
12:02:20.001 INFO [Server] - /announcement edit
12:02:20.001 INFO [Server] - /announcement gui
```

?> 其中 `announcement` 为 AnnounCement 的顶层命令（权限等级: Any）。

- `/announcement edit`
  - 打开公告编辑界面（权限等级: GameDirectors）。

- `/announcement gui`
  - 打开公告 GUI。

## Market
```log
? market
12:02:26.910 INFO [Server] market:
12:02:26.910 INFO [Server] LOICollection -> 玩家市场
12:02:26.911 INFO [Server] Usage:
12:02:26.911 INFO [Server] - /market gui
12:02:26.911 INFO [Server] - /market sell
```

?> 其中 `market` 为 Market 的顶层命令（权限等级: Any）。

- `/market gui`
  - 打开玩家市场 GUI。

- `/market sell`
  - 打开玩家市场出售界面。

## Setting
```log
? setting
15:57:16.377 INFO [Server] setting:
15:57:16.377 INFO [Server] LOICollection -> 个人设置
15:57:16.377 INFO [Server] Usage:
15:57:16.377 INFO [Server] - /setting announcement
15:57:16.377 INFO [Server] - /setting chat
15:57:16.392 INFO [Server] - /setting language
15:57:16.399 INFO [Server] - /setting pvp gui
15:57:16.399 INFO [Server] - /setting pvp off
15:57:16.399 INFO [Server] - /setting pvp on
15:57:16.399 INFO [Server] - /setting tpa
```

?> 其中 `setting` 为 Setting 的顶层命令（权限等级: Any）。

- `/setting announcement`
  - 打开个人公告设置界面。

- `/setting chat`
  - 打开个人聊天设置界面。

- `/setting language`
  - 打开个人语言设置界面。

- `/setting pvp gui`
  - 打开个人 PVP 设置界面。

- `/setting pvp on`
  - 开启个人 PVP。

- `/setting pvp off`
  - 关闭个人 PVP。

- `/setting tpa`
  - 打开个人 TPA 设置界面。

!> 以上属于 LOICollectionA 1.5.2 版本的命令列表，对于后续版本的命令列表可能会有所不同。 