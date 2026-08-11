# LOICollectionA

> **A Minecraft Server Plugin For LeviLamina**

![Release](https://img.shields.io/github/v/release/loitietu/LOICollectionA?style=flat-square)
![Stars](https://img.shields.io/github/stars/loitietu/LOICollectionA?style=social)
![Downloads](https://img.shields.io/github/downloads/loitietu/LOICollectionA/total?style=flat-square)
[![License](https://img.shields.io/github/license/loitietu/LOICollectionA)](LICENSE)  

[![656669024](https://img.shields.io/badge/1018233878-red?style=for-the-badge&logo=qq)](https://qm.qq.com/cgi-bin/qm/qr?k=l7XBaItHiNLnFKX7YiI7uqsEIZHaxjq3&jump_from=webapi&authKey=G3/2El1RPyAVYP4NYTJ2ytKRL6hSYfDNQXbrOlKBy/P0FEUjQSnXF8c7TWNkGbCC)

[![English](https://img.shields.io/badge/English-inactive?style=for-the-badge)](README.md)
[![中文](https://img.shields.io/badge/简体中文-informational?style=for-the-badge)](README.zh.md)

LOICollectionA is a plugin that originated from LOICollection and has evolved through a comprehensive refactoring. This process also served as an opportunity to adapt it for LeviLamina.

It inherits the functional diversity of LOICollection while introducing numerous optimizations. The plugin adopts a `Micro Kernel` architecture for its functional modules, enhancing flexibility and extensibility.

Future developments will provide more API interfaces to empower plugin developers with richer functionality.

## Built-in .lcui Examples

`.lcui` is a collection of classes that directly wraps LeviLamina's native form and UI capabilities, including built-in classes such as `CustomForm`, `MessageBox`, and `PaginatedForm`. It serves as the plugin's native UI.

### CustomForm Example

```lcui
input = new ObservableString("", true);
count = new ObservableNumber(1, false);
enabled = new ObservableBoolean(true, true);

form = new CustomForm("example", "Native UI Example");
form.header("Hello", new TextOptions());
form.label("This is a native custom form", new TextOptions());
form.divider(new DividerOptions());
form.textField("Input", input, new TextFieldOptions());
form.slider("Count", count, 1, 10, new SliderOptions());
form.toggle("Enabled", enabled, new ToggleOptions());
form.button("Submit", func () -> void {
    mc::runCmd("say " + input.getData());
}, new ButtonOptions());
form.closeButton();

form.show(func (closeReason) -> void {
    if (closeReason == 2) [
        mc::runCmd("say closed");
    ]
});
```

### MessageBox Example

```lcui
box = new MessageBox("confirm", "Confirm");
box.body("Are you sure?");
box.button1("Yes");
box.button2("No");
box.show(func (result) -> void {
    if (result.selection) [
        mc::runCmd("say Button 2 selected");
    : 
        mc::runCmd("say Button 1 selected or no selection");
    ]
});
```

### PaginatedForm Example

```lcui
pages = [ "Apple", "Banana", "Cherry", "Dragon Fruit", "Elderberry" ];

form = new PaginatedForm("fruits", "Fruit List", pages, 2);
form.previousButton("Previous");
form.nextButton("Next");
form.chooseButton("Go", "Page number");
form.closeButton();

form.show(func (result) -> void {
    if (result.closeReason == 2) [
        if (result.selection != "") [
            mc::runCmd("say Selected: " + result.selection);
        ]
    ]
});
```

## Implemented Modules

> All modules below can be enabled/disabled in the configuration file.

### Basic Modules

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
- [x] Statistics

### Additional Modules

- [x] BasicHook
  - [x] FakeSeed
- [x] RedStone
- [x] OrderedUI

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
