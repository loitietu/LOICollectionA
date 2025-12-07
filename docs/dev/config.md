# 环境配置

该部分将介绍如何配置开发环境。  
在开始之前，请确保您已经安装了 `xmake` 和 `C++` 编译环境。  

## 配置 xmake.lua

> [!TIP]
> 以下内容将默认您已经安装了 `xmake` 和 `C++` 编译环境，并创建了一个空的 `mod` 项目

1. 打开项目根目录下的 `xmake.lua` 文件，并添加以下内容：

    ```lua
    package("LOICollectionA")
        add_urls("https://github.com/loitietu/LOICollectionA.git")
        add_versions("1.9.0", "e1af7bf87732dd6e6cf39ce85b88a66ac9093c31")

        on_install(function (package)
            import("package.tools.xmake").install(package)
        end)
    package_end()
    ```

2. 保存并关闭 `xmake.lua` 文件。

- 此时项目已经配置完毕，您可以通过 `add_requires` 和 `add_packages` 去安装依赖库。

> [!ATTENTION]
> 以上内容仅为示例，其中部分可能会有所区别，请根据您的实际情况进行修改。
