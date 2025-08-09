# LOICollectionA

> **A Minecraft Server Plugin For LeviLamina**

![Release](https://img.shields.io/github/v/release/loitietu/LOICollectionA?style=flat-square)
![Stars](https://img.shields.io/github/stars/loitietu/LOICollectionA?style=social)
![Downloads](https://img.shields.io/github/downloads/loitietu/LOICollectionA/total?style=flat-square)
[![License](https://img.shields.io/github/license/loitietu/LOICollectionA)](LICENSE)

[![English](https://img.shields.io/badge/English-inactive?style=for-the-badge)](README.md)
![中文](https://img.shields.io/badge/简体中文-informational?style=for-the-badge)

LOICollectionA 是源于插件 LOICollection 所演化而来的内容，其在原有的基础上对于整体进行了一次完整的重构，并由此为契机进行了对 LeviLamina 的适配。

而其在整体上及继承了 LOICollection 的功能多样性，同时也对其进行了许多优化。并且在功能模块方面实现了以 `数据` 为核心的插件架构，使得插件的功能更加灵活，更加容易扩展。

同时未来也将会提供更多 API 接口，以便于插件开发者进行添加更加丰富的功能。

## 实现功能模块

> 以下功能模块均可在配置文件中进行配置开启或关闭。

- [x] Blacklist
- [x] Mute
- [x] Cdk
- [x] Menu
- [x] Tpa
- [x] Shop
- [x] Monitor
- [x] Pvp
- [x] Wallet
- [x] Chat
- [x] Notice
- [x] Market
- [x] BehaviorEvent

## 安装插件

1. 在服务器目录中执行命令

    ```cmd
    lip install github.com/loitietu/LOICollectionA
    ```

2. 启动服务器。
3. 等待输出加载文本。
4. 完成安装。

> [!TIP]
> 更多信息请访问 [Github Pages](https://loitietu.github.io/LOICollectionA/)

## 本地编译

打开本地命令提示符(`cmd`)并执行以下命令：

```cmd
git clone https://github.com/loitietu/LOICollectionA.git
cd LOICollectionA
xmake repo -u
xmake lua scripts/project.lua
xmake
```

## 贡献

欢迎提交 `PR` 或者 `Issue` 帮助一起完善这个插件。

## LICENSE

- 该插件根据 [GPL-3.0](LICENSE) 许可证进行许可。
