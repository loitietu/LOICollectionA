# LOICollectionA

> **A Minecraft Server Plugin For LeviLamina**

![Release](https://img.shields.io/github/v/release/loitietu/LOICollectionA?style=flat-square)
![Stars](https://img.shields.io/github/stars/loitietu/LOICollectionA?style=social)
![Downloads](https://img.shields.io/github/downloads/loitietu/LOICollectionA/total?style=flat-square)
[![License](https://img.shields.io/github/license/loitietu/LOICollectionA)](LICENSE)

![English](https://img.shields.io/badge/English-inactive?style=for-the-badge)
[![简体中文](https://img.shields.io/badge/简体中文-informational?style=for-the-badge)](README.zh.md)

LOICollectionA is a plugin that originated from LOICollection and has evolved through a comprehensive refactoring. This process also served as an opportunity to adapt it for LeviLamina.

It inherits the functional diversity of LOICollection while introducing numerous optimizations. The plugin adopts a `data-centric` architecture for its functional modules, enhancing flexibility and extensibility.

Future developments will provide more API interfaces to empower plugin developers with richer functionality.

## Implemented Modules

> All modules below can be enabled/disabled in the configuration file.

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

## Installation

1. Execute the following command in your server directory:

    ```cmd
    lip install github.com/loitietu/LOICollectionA
    ```

2. Start the server.
3. Wait for the loading confirmation message.
4. Installation complete.

> [!TIP]
> For more information, visit [Github Pages](https://loitietu.github.io/LOICollectionA/)

## Local Compilation

Open Command Prompt (`cmd`) and execute:

```cmd
git clone https://github.com/loitietu/LOICollectionA.git
cd LOICollectionA
xmake repo -u
xmake
```

## Contributing

We welcome `PRs` and `Issues` to help improve this plugin.

## License

- Licensed under the [GPL-3.0](LICENSE) license.
