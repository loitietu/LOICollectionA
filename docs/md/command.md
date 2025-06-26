# 命令列表
> 在 LOIColletionA 中，对于不同的 `内置插件` 均提供了不同的命令列表。  
> 而插件则提供以下命令以进行简单的交互：

> [!TIP]
> 在命令提示中的 `<>` 为必填参数，`[]` 为可选参数。

## Blacklist
```log
? blacklist
11:44:58.298 INFO [Server] blacklist:
11:44:58.298 INFO [Server] LOICollection -> 服务器黑名单
11:44:58.298 INFO [Server] Usage:
11:44:58.298 INFO [Server] - /blacklist add <ip|uuid|clientid> <target: target> [cause: string] [time: int]
11:44:58.298 INFO [Server] - /blacklist gui
11:44:58.298 INFO [Server] - /blacklist list
11:44:58.298 INFO [Server] - /blacklist remove <targetName: string>
```
?> 其中 `blacklist` 为 Blacklist 的顶层命令（权限等级: GameDirectors）。

- `/blacklist add <ip|uuid> <target: target> [cause: string] [time: int]`
  - 向黑名单中添加一个目标。
  - 其中 `<ip|uuid|clientid>` 为目标的类型。
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
11:45:36.007 INFO [Server] - /mute add <target: target> [cause: string] [time: int]
11:45:36.007 INFO [Server] - /mute gui
11:45:36.007 INFO [Server] - /mute remove <target: target>
```

?> 其中 `mute` 为 Mute 的顶层命令（权限等级: GameDirectors）。

- `/mute add <target: target> [cause: string] [time: int]`
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

- `/cdk edit`
  - 打开总换CDK设置（权限等级: GameDirectors）。

- `/cdk gui`
  - 打开总换CDK GUI。

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
13:08:57.336 INFO [Server] tpa:
13:08:57.336 INFO [Server] LOICollection -> 玩家互传
13:08:57.336 INFO [Server] Usage:
13:08:57.336 INFO [Server] - /tpa gui
13:08:57.336 INFO [Server] - /tpa invite <tpa|tphere> <target: target>
13:08:57.336 INFO [Server] - /tpa setting
```

?> 其中 `tpa` 为 Tpa 的顶层命令（权限等级: Any）。

- `tpa invite <tpa|tphere> <target: target>`
  - 向目标发送传送请求。
  - 其中 `<tpa|tphere>` 为传送类型。
  - 其中 `<target: target>` 为目标选择器。

- `/tpa gui`
  - 打开 Tpa GUI。

- `/tpa setting`
  - 打开 Tpa 个人设置。

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

## Pvp
```log
? pvp
13:10:14.946 INFO [Server] pvp:
13:10:14.946 INFO [Server] LOICollection -> 服务器PVP
13:10:14.946 INFO [Server] Usage:
13:10:14.946 INFO [Server] - /pvp gui
13:10:14.946 INFO [Server] - /pvp off
13:10:14.946 INFO [Server] - /pvp on
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
13:11:17.040 INFO [Server] chat:
13:11:17.040 INFO [Server] LOICollection -> 个人称号
13:11:17.040 INFO [Server] Usage:
13:11:17.040 INFO [Server] - /chat add <target: target> <titleName: string> [time: int]
13:11:17.040 INFO [Server] - /chat gui
13:11:17.040 INFO [Server] - /chat list <target: target>
13:11:17.040 INFO [Server] - /chat remove <target: target> <titleName: string>
13:11:17.040 INFO [Server] - /chat set <target: target> <titleName: string>
13:11:17.040 INFO [Server] - /chat setting
```

?> 其中 `chat` 为 Chat 的顶层命令（权限等级: Any）。

- `/chat add <target: target> <titleName: string> [time: int]`
  - 为目标添加指定称号（权限等级: GameDirectors）。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<titleName: string>` 为称号名称。
  - 其中 `[time: int]` 为称号时间（单位：小时）。

- `/chat remove <target: target> <titleName: string>`
  - 为目标移除指定称号（权限等级: GameDirectors）。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<titleName: string>` 为称号名称。

- `/chat set <target: target> <titleName: string>`
  - 为目标设置指定称号（权限等级: GameDirectors）。
  - 其中 `<target: target>` 为目标选择器。
  - 其中 `<titleName: string>` 为称号名称。
  - 注: 无法设置未拥有的称号。

- `/chat list <target: target>`
  - 查看目标的所有称号（权限等级: GameDirectors）。
  - 其中 `<target: target>` 为目标选择器。

- `/chat gui`
  - 打开管理称号 GUI（权限等级: GameDirectors）。

- `/chat setting`
  - 打开个人称号设置。

## Notice
```log
? notice
13:13:03.468 INFO [Server] notice:
13:13:03.468 INFO [Server] LOICollection -> 公告系统
13:13:03.468 INFO [Server] Usage:
13:13:03.468 INFO [Server] - /notice edit
13:13:03.468 INFO [Server] - /notice gui
13:13:03.468 INFO [Server] - /notice setting
```

?> 其中 `notice` 为 notice 的顶层命令（权限等级: Any）。

- `/notice edit`
  - 打开公告编辑界面（权限等级: GameDirectors）。

- `/notice gui [uiName: string]`
  - 打开公告 GUI。
  - 其中 `[uiName: string]` 为公告ID。

- `/notice setting`
  - 打开公告个人设置。

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

> [!NOTE]
> 以上内容均属于 LOICollectionA 1.6.1 版本的命令列表，对于后续版本的命令列表可能会有所不同。 