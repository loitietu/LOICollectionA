# LCUI 脚本语法

> [!NOTE]
> 以下内容取自 LOICollectionA 1.15.0 的 `.lcui` 脚本语言实现，对于后续版本可能会有所不同。

## 概述

`.lcui` 是 LOICollectionA 用于编写游戏内 GUI 的脚本语言。所有模块的内置界面（如 `blacklist.lcui`、`statistics.lcui`）以及 Menu/Shop 的 `menu.lcui`、`shop.lcui` 均使用该语言编写。

脚本的完整处理流程为：

```text
Lexer（词法分析） -> Parser（语法分析） -> SemanticAnalyzer（语义分析） -> Compiler（字节码编译） -> Optimizer（优化） -> VM（执行）
```

- 插件启动时或执行 `/xxx reload` 命令后，`GUIManager::load` 会读取 `.lcui` 文件，完成上述编译流程并将字节码缓存。
- 玩家打开 GUI 时，`GUIManager::open(id, formId, type[, ctx])` 会针对该玩家执行缓存的脚本；脚本中 `new CustomForm(...)`、`new PaginatedForm(...)` 等创建的表单会注册到 `GUIManager`，随后切换到 `formId` 对应的表单。

> 原生表单类的方法与生命周期详见 [原生 UI（Native UI）](./native-ui.md)，Menu/Shop 专用表单与默认变量详见 [LOICollectionAPI](./api.md)。

## 一个最小的脚本

```lcui
form = new CustomForm("example.main", {tr("example.gui.title")});
form.label({tr("example.gui.hello")}, new TextOptions());
form.button({tr("generic.gui.close")}, func () -> void {
    form.close();
}, new ButtonOptions());
form.closeButton();
form.show();
```

## 注释、字面量与语句分隔

### 注释

支持行注释 `//` 与块注释 `/* ... */`：

```lcui
// 这是行注释
/* 这是
   块注释 */
```

### 字面量

| 字面量 | 示例 | 说明 |
| --- | --- | --- |
| 整数 | `42`、`-1` | 32 位有符号整数 |
| 浮点数 | `3.14`、`.5`、`5.` | 支持省略小数点前的 `0` |
| 字符串 | `"hello"`、`'world'` | 单引号或双引号包裹 |
| 布尔 | `true`、`false` | — |
| 空值 | `None` | 仅可用于 `optional` 上下文 |
| 数组 | `[1, "a", true]` | 可混合元素类型 |

字符串没有转义符，引号内的内容原样保留。若字符串中需要包含某种引号，可以使用另一种引号包裹，例如 `'say "hi"'`；字符串也允许跨行。

### 语句分隔

语句之间可以使用 `;` 分隔，也可以使用换行分隔。同一行内存在多条语句时必须使用 `;`，推荐每条语句都以 `;` 结尾：

```lcui
a = 1; b = 2;   // 同一行需要 ;

c = 3           // 换行可以省略 ;
d = 4;
```

## 变量与类型

### 动态变量

直接赋值即可创建变量，未标注类型时为动态类型，可随时赋其他类型的值：

```lcui
a = 1;
a = "test";     // 合法，动态类型
b = [1, 2, 3];
```

### 类型标注

使用 `变量名: 类型 = 值` 声明带类型的变量，类型声明必须同时提供初始值：

```lcui
count: int = 1;
name: string = "LOICollection";
flag: bool = true;
score: float = 1.5;
```

类型标注会进行赋值检查，类型不匹配或声明后赋其他类型的值都会报错。

### 内置类型

| 类型 | 说明 |
| --- | --- |
| `int` | 整数 |
| `float` | 浮点数 |
| `string` | 字符串 |
| `bool` | 布尔值 |
| `void` | 函数无返回值时使用 |
| `ClassName` | 自定义类或原生类类型 |
| `optional<T>` | 可选值，可能为空 |
| `variant<T1, T2, ...>` | 联合值，只能保存声明中的某一种类型 |

未标注类型的函数参数、函数返回值与变量均为动态类型。`None` 是空值字面量而不是可写的类型名（`optional` 为空时，其 `.type` 返回 `none`）。

### optional（可选值）

`optional<T>` 可以保存 `T` 类型的一个值，也可以是空值 `None`：

```lcui
value: optional<int> = 1;
empty: optional<string> = None;

value.has_value;    // true
value.value;        // 1
empty.has_value;    // false
empty.value;        // 报错：optional 为空
```

`optional` 支持三个特殊成员：

| 成员 | 说明 |
| --- | --- |
| `has_value` | 是否有值（布尔） |
| `value` | 当前值，为空时访问报错 |
| `type` | 当前值的类型名；为空时为 `none` |

`None` 只能出现在 `optional` 上下文中（例如 optional 变量的初始值、optional 参数、optional 返回值），在其他位置使用会报错。空 `optional` 直接参与算术、比较等运算也会报错，建议先用 `if (value)` 或 `has_value` 判断。

### variant（联合值）

`variant<T1, T2, ...>` 只能保存声明列表中的某一种类型，赋值时会校验：

```lcui
data: variant<string, bool, int> = 1;
data.type;      // "int"
data.value;     // 1
data = "test";  // 合法
data = 1.5;     // 报错：float 不在声明列表中
```

`variant` 至少需要两个类型参数（如 `variant<int>` 不合法）。

### 类型别名 using

使用 `using` 可以为类型定义别名，仅允许出现在脚本顶层：

```lcui
using Value = variant<string, bool, int>;
using Count = int;

a: Value = 1;
b: Count = 3;
```

别名可以链式引用（`using B = A;`），但不能自引用，也不能重复定义。

### 数组

数组使用 `[元素1, 元素2, ...]` 创建，支持任意混合类型：

```lcui
items = [1, "a", true];
items[0];       // 1
items.length;   // 3

items[3] = 2;   // 索引等于当前长度时追加元素
items[1] = "b"; // 修改已有元素
```

数组索引从 `0` 开始，越界读取、负数索引会报错。数组的 `==`/`!=` 比较的是引用（是否为同一个数组），而非内容。类的数组字段在每个实例中相互隔离（创建实例时会拷贝），修改一个实例的数组不会影响其他实例。

## 运算符

### 算术运算符

| 运算符 | 说明 |
| --- | --- |
| `+` | 加法 / 字符串拼接 |
| `-` | 减法 |
| `*` | 乘法 |
| `/` | 除法（两个整数相除也会产生浮点数） |
| `%` | 取模（仅支持整数） |
| `^` | 幂运算（右结合） |
| `+`/`-`（一元） | 正号 / 负号 |

```lcui
1 + 2 * 3;   // 7
2 ^ 3 ^ 2;   // 512（右结合）
10 / 3;      // 3.33333...
10 % 3;      // 1
"Hello " + "World";  // "Hello World"
```

字符串与数值、布尔等类型使用 `+` 时都会转换为字符串拼接。`%` 只能用于整数，除数为零、整数溢出、取模非整数类型都会报错。

### 比较运算符

`==`、`!=`、`>`、`<`、`>=`、`<=` 可用于数值、字符串与布尔比较；数值的 `int`/`float` 可以直接比较。字符串比较规则大致参考 C++。类型不一致的 `==`/`!=` 会报错（例如 `1 == "1"`）。

对象、数组与函数支持 `==`/`!=`，但比较的是引用：

```lcui
a = [1];
b = [1];
a == b;   // false，内容相同但不是同一个数组
c = a;
a == c;   // true
```

`None == None` 为 `true`。

### 逻辑运算符

`&&`、`||` 支持短路求值：`false && 任意表达式` 不会执行右侧，`true || 任意表达式` 同样不会执行右侧。`!` 为逻辑非。

### 真值规则

条件判断使用以下真值规则：

| 值 | 真值 |
| --- | --- |
| `int` | 非 `0` 为真 |
| `float` | 绝对值大于 `0` 为真 |
| `string` | `""`、`"false"`、`"FALSE"`（不区分大小写）为假，其余非空字符串为真 |
| `bool` | 本身 |
| `array` | 非空为真 |
| 对象 / 函数 | 恒为真 |
| `None` | 恒为假 |

## 流程控制

### if / else

`if` 使用方括号 `[ ... ]` 作为分支块，`:` 分隔 else 分支，else 可以省略：

```lcui
if (count > 0) [
    mc::runCmd("say positive");
:
    mc::runCmd("say not positive");
]

if (flag) [
    mc::runCmd("say flag is true");
]
```

`if` 本身也是表达式，分支中的最后一个值会作为结果返回，可以参与运算或赋值：

```lcui
result = if (count > 0) [ 10 : 20 ];
total = if (flag) [ 1 : 2 ] + 5;
```

### while

```lcui
i = 0;
while (i < 5) [
    i = i + 1;
]
```

### for

```lcui
s = 0;
for (i = 0; i < 10; i = i + 1) [
    if (i % 2 == 0) [ continue; ];
    s = s + i;
]
```

`for (;;)` 可以省略三个子句，配合 `break` 使用。

### break / continue

`break` 跳出当前循环，`continue` 跳过本次循环剩余部分；两者只能出现在循环内部，嵌套循环只作用于最近的一层。

## 函数

### 命名函数

使用 `func` 定义函数，参数与返回值可以标注类型，也可以省略（省略时为动态类型）：

```lcui
func add(a: int, b: int) -> int {
    return a + b;
}

func pick(x) {
    return x;
}

result = add(1, 2);
```

函数支持递归与相互递归调用，也支持按参数类型重载：

```lcui
func id(x: int) -> int {
    return x;
}

func id(x: string) -> string {
    return x;
}

id(7);      // 调用 int 版本
id("s");    // 调用 string 版本
```

函数和类定义只能出现在脚本顶层，不能嵌套在函数或其他块中。`return` 只能出现在函数体内；没有显式 `return` 的函数返回空值。

### 匿名函数（Lambda）

使用 `func (参数) -> 返回类型 { ... }` 定义匿名函数，可以赋值给变量、作为参数传递，并捕获外部局部变量、参数与 `this`：

```lcui
double = func (x: int) -> int {
    return x * 2;
};

form.button("Button", func () -> void {
    GUIManager::callback("example.submit", [ double(21) ]);
}, new ButtonOptions());
```

## 类与继承

### 定义类

```lcui
class Animal {
public:
    name = "unknown";
    count: int = 0;

    Animal(name) {
        this.name = name;
    }

    func speak() -> string {
        return "...";
    }
}

dog = new Animal("dog");
dog.name;   // "dog"
```

### 字段与方法

- 字段可以带默认值、类型标注，也可以两者都省略（动态类型）。
- 没有默认值的字段必须在构造函数中赋值。
- 构造函数的名称必须与类名一致，每个类最多一个构造函数；未定义构造函数时使用默认构造。
- 方法使用 `func 方法名(参数) -> 返回类型 { ... }` 定义，支持重载。

### 访问控制

使用 `public:` 与 `private:` 声明访问段，默认（首个访问段之前）为公开：

```lcui
class SecretBox {
private:
    secret = 42;
public:
    func getSecret() -> int {
        return secret;
    }
}

box = new SecretBox();
box.getSecret();    // 42
box.secret;         // 报错：私有字段
```

私有成员不会被继承，子类中无法通过 `this` 或裸名称访问基类的私有成员。

### 静态成员

使用 `static` 声明静态字段或静态方法，通过 `类名.成员` 访问：

```lcui
class Counter {
public:
    static total = 0;

    static func inc() -> int {
        total = total + 1;
        return total;
    }
}

Counter.inc();  // 1
Counter.total;  // 1
```

静态字段在所有实例之间共享；静态方法中不能使用 `this`，实例对象也不能直接调用静态方法。静态成员支持继承与重写。

### 继承

使用 `extends` 继承基类：

```lcui
class Dog extends Animal {
    func speak() -> string {
        return "Woof";
    }
}

dog = new Dog("dog");
dog instanceof Animal;  // true
```

相关关键字：

| 关键字 | 说明 |
| --- | --- |
| `extends` | 声明基类 |
| `super(...)` | 调用基类构造函数 |
| `super.方法(...)` | 调用基类方法 |
| `instanceof` | 判断对象是否为指定类（含派生类）的实例 |
| `this` | 当前对象 |

构造规则：

- 子类构造函数若不显式调用 `super(...)`，会隐式调用基类的无参构造（或默认构造）。
- 基类只有带参构造函数时，子类构造函数必须显式 `super(...)`，否则报错。
- 类名先声明后声明均可，支持多层继承与多态；循环继承、重复类名、未知基类会报错。

对象的 `==`/`!=` 同样为引用比较。

## 内置命名空间与宏

### 内置命名空间

| 命名空间 | 函数 | 说明 |
| --- | --- | --- |
| `math` | `abs`、`min`、`max`、`sqrt`、`pow`、`log`、`sin`、`cos`、`random` | 数学函数 |
| `string` | `length`、`upper`、`lower`、`substr`、`trim`、`replace` | 字符串函数 |
| `std` | `format(formatStr, array)` | 使用数组中的值格式化字符串（fmt 风格） |
| `mc` | `runCmd(command)` | 以当前玩家身份执行命令 |
| `GUIManager` | `value(id)`、`request(id, args)`、`callback(id, args)`、`open(id, formId, type[, ctx])`、`switchTo(id, type)` | 与 GUI 数据交互 |

调用格式为 `命名空间::函数(参数)`，例如：

```lcui
mc::runCmd("say hello");
GUIManager::callback("example.submit", [ "data" ]);
text = std::format({tr("example.gui.info")}, [ 1, "two" ]);
```

> `GUIManager::open` 与 `GUIManager::switchTo` 中的 `type` 为整数：`1` 为 CustomForm，`2` 为 MessageBox，`3` 为 PaginatedForm，`4` 为 ScriptForm。

### 宏 `{...}`

使用 `{名称}` 或 `{名称(参数)}` 调用宏，例如 `{tr("language.gui.title")}`。`tr` 返回当前玩家语言的翻译文本；`{player}`、`{server_tps}`、`{score(名称)}` 等默认变量也以宏形式使用，完整列表见 [LOICollectionAPI](./api.md)。

```lcui
title = {tr("example.gui.title")};
label = {player} + " -> " + {tr("example.gui.hello")};
```

### 透传与阻断 `$`

`$` 用于透传原始文本：`$` 之后到 `}`（或 `;`）之间的内容保持原样，不会参与解析：

```lcui
raw = $custom content};
```

`${变量}` 形式可以阻断变量/宏替换，使内容保留字面形式，例如 `{player_realname} + '->' + ${team}` 中的 `${team}` 会原样输出为 `{team}`。

## 原生类

脚本中可直接 `new` 以下原生类（概览）：

| 类 | 用途 | 详细文档 |
| --- | --- | --- |
| `GlobalValue` | 全局值容器，字段 `value` | [api.md](./api.md) |
| `CtxValue` | 读取 `GUIManager::open` 传入的 `ctx` 数组，字段 `value` | 见下文 |
| `ObservableString` / `ObservableNumber` / `ObservableBoolean` / `ObservableUIRawMessage` | 可观察数据，用于控件双向绑定 | [native-ui.md](./native-ui.md) |
| `UIRawMessage` | 构造富文本 / 翻译文本 | [native-ui.md](./native-ui.md) |
| `CustomForm` / `MessageBox` / `PaginatedForm` | 原生表单 | [native-ui.md](./native-ui.md) |
| `TextOptions` / `ButtonOptions` / `TextFieldOptions` / `DropdownOptions` / `DropdownItem` / `SliderOptions` / `ToggleOptions` / `DividerOptions` / `SpacingOptions` | 控件选项 | [native-ui.md](./native-ui.md) |
| `MenuData` / `MenuItemData` / `MenuControlData` / `MenuForm` / `MenuMessageBox` | 菜单数据与表单 | [api.md](./api.md) |
| `ShopData` / `ShopItemData` / `ShopForm` | 商店数据与表单 | [api.md](./api.md) |
| `ScoreRequirement` | Score 需求，字段 `objective`、`value` | [api.md](./api.md) |

### CtxValue

当脚本通过 `GUIManager::open(id, formId, type, ctx)` 打开并传入 `ctx` 数组时，可以使用 `new CtxValue(索引)` 读取对应元素：

```lcui
detail = new CtxValue(0);
form.label(detail.value, new TextOptions());
```

索引越界或未传入 `ctx` 时使用 `CtxValue` 会报错。

## 完整示例

综合使用变量、类型、函数、类、流程控制与原生表单：

```lcui
class Item {
public:
    name = "";
    price: int = 0;

    Item(name, price) {
        this.name = name;
        this.price = price;
    }

    func format() -> string {
        return name + " (" + price + ")";
    }
}

func makeItems() {
    return [ new Item("Apple", 3), new Item("Banana", 5) ];
}

items = makeItems();
selected = new GlobalValue();
selected.value = "";

form = new CustomForm("example.shop", {tr("example.shop.title")});
form.label({tr("example.shop.list")}, new TextOptions());
form.button(items[0].format(), func () -> void {
    selected.value = items[0].name;
    form.close();
}, new ButtonOptions());
form.button(items[1].format(), func () -> void {
    selected.value = items[1].name;
    form.close();
}, new ButtonOptions());
form.closeButton();
form.show();
```

## 常见约束与陷阱

- 类、命名函数与 `using` 只能出现在脚本顶层，不能嵌套定义。
- `return` 只能在函数内使用；`break`/`continue` 只能在循环内使用。
- 类型声明（`x: int`）必须同时提供初始值；没有默认值的类字段必须在构造函数中赋值。
- `None` 只能用于 `optional` 上下文；空 `optional` 直接读取、算术或比较会报错。
- `%` 仅支持整数；除零、整数溢出会报错。
- 数组索引必须为 `int`，越界读取或负数索引会报错；索引等于长度时表示追加。
- 字符串没有转义符，需要包含引号时请使用另一种引号包裹。
- 函数调用的参数最多支持 100 个，超出会解析失败。
- 不同模块注册的 `GUIManager::value` / `request` / `callback` ID 各有不同，具体以对应模块的 `.lcui` 文件为准。

> 脚本语言的常见错误及解决方案，请参见 [常见错误含义](./errors.md)。
