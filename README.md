---
title: Snipaste KWin 贴图捕获补丁
tags: [KDE, Wayland, Snipaste]
note_types: document
created: 2026-10-30
updated: 2026-10-30

---

# Snipaste KWin 贴图捕获补丁

让 Snipaste 自己的截图包含桌面上已有的贴图，继续使用原有的框选、标注与贴图界面。补丁加载到常驻 Snipaste 进程中，截图时无需启动其他应用或转发图像。

## 原理与适用范围

KWin 的桌面截图接口默认排除调用进程的窗口，Snipaste 的贴图也属于该进程。补丁在 Qt D-Bus 发送入口将指定请求的 `hide-caller-windows` 设为 `false`：

```text
Snipaste → 进程内补丁 → KWin 桌面截图 → Snipaste 框选、标注、贴图
                  保留调用进程的窗口
```

当前原型针对 **Linux x86_64、Snipaste 2.11.3 AppImage 内置的 Qt 6.2.4，以及 KWin ScreenShot2 接口版本 5**。仅匹配以下请求，其他方法与目标保持原样：

- 服务、接口：`org.kde.KWin.ScreenShot2`
- 路径：`/org/kde/KWin/ScreenShot2`
- 方法：`CaptureWorkspace`

补丁保留原请求的 options、截图管道文件描述符、服务启动与交互授权标志、超时和同步/异步调用方式。Qt 的 `QDBusMessage` 采用共享可变数据，因此改写时创建独立消息，保持 Snipaste 持有的原消息不变。

## 构建

需要 C++17 编译器、CMake、Ninja、curl、dpkg-deb，以及已有的 Snipaste AppImage。以下操作在仓库根目录执行，均无需管理员权限：

```bash
./scripts/prepare.sh /path/to/Snipaste-2.11.3-x86_64.AppImage
```

脚本完成三件事：

1. 将固定 SHA-256 的 Ubuntu Qt 6.2.4 开发包解压到用户缓存，用于获取匹配版本的头文件。
2. 将 AppImage 提取到 `.runtime/squashfs-root`，保留原文件；记录原 AppImage 的校验和。
3. 使用 AppImage 内置 Qt 库构建补丁并运行测试。

系统 Qt 保持不变。下载的开发包不安装到系统，其动态库也不参与运行。构建产物、提取目录和依赖缓存不纳入 Git。

## 启动与回退

先保存重要贴图，从托盘退出当前 Snipaste，再在自己的终端中运行：

```bash
./scripts/run-snipaste.sh
```

启动器设置局部 `LD_PRELOAD`，保留 AppImage 自带的 checkrt 预加载链路。当前 KDE/Fcitx5 环境中，AppImage 实际使用 xcb；启动器为该进程设置 `QT_IM_MODULE=fcitx`，选择内置的 Fcitx5 插件，使文本标注接入输入法。系统全局输入法环境保持不变。

已存在 Snipaste 进程时会明确退出，避免新启动的补丁进程仅发送单实例消息后结束。

通过这个入口启动常驻进程后，原有截图快捷键和托盘操作继续使用该进程。日常延迟以常驻后的热唤起为目标；提取 AppImage 和构建不发生在截图路径上。

初次验收可启用控制消息日志：

```bash
SNIPASTE_PATCH_TRACE=1 ./scripts/run-snipaste.sh
```

依次检查：

1. 贴出 A、B，调整位置、大小及重叠关系。
2. 使用 Snipaste 截图，将 A、B 与桌面内容一起框选为 C。
3. 编辑 C，再贴出来；再次截图应包含 C。
4. 检查截图内没有工具栏、菜单、框选界面的残影。

日志只打印命中的请求和参数变化，不保存截图。正常使用省略 `SNIPASTE_PATCH_TRACE`。

**回退：**从托盘退出补丁版，再正常打开原 AppImage。补丁不改写 Snipaste 二进制、KWin 或系统 Qt；Snipaste 自身仍按其原有逻辑维护配置与 D-Bus 激活文件。需要登录自启时，在 KDE“自动启动”中添加本仓库的 `scripts/run-snipaste.sh`，并避免同时自启原 AppImage。

## 检查与性能

```bash
ctest --test-dir build --output-on-failure

# 验证补丁能调用真实 KWin；仅读取接口版本，不截屏。
LD_LIBRARY_PATH="$PWD/.runtime/squashfs-root/usr/lib" \
LD_PRELOAD="$PWD/build/libsnipaste-kwin.so" \
./build/kwin-probe
```

测试通过真实 ELF 预加载链路检查同步、异步调用、精确匹配、消息独立性、标志、超时及文件描述符。测试传输层不执行截图。

模拟调用的微基准可用同一测试程序运行：

```bash
LD_LIBRARY_PATH="$PWD/build:$PWD/.runtime/squashfs-root/usr/lib" \
LD_PRELOAD= ./build/interposition-test baseline --benchmark

LD_LIBRARY_PATH="$PWD/build:$PWD/.runtime/squashfs-root/usr/lib" \
LD_PRELOAD="$PWD/build/libsnipaste-kwin.so" \
./build/interposition-test patched --benchmark
```

补丁只修改控制消息，新增开销处于微秒量级。完整的按键到框选界面延迟包含 Snipaste、KWin 与显示呈现，应通过桌面端到端测量评估。

更新 Snipaste、Qt 或 KWin 后，应重新检查接口与桌面循环。Qt 版本不匹配或命中请求的参数签名改变时，原型会明确终止并输出错误。

## 许可

Copyright © 2026 [yceachan](https://github.com/yceachan)。

本仓库原创代码和文档采用 [CC BY-NC 4.0（署名—非商业性使用）](LICENSE) 公开，面向非商业学习与研究。你可以在遵守署名、标明修改等许可条件下复制、分享和修改；**商业使用须另行取得作者授权**。完整条款以许可证正文为准。

Snipaste、Qt 等第三方组件由各自许可证授权。构建时使用你提供的 AppImage，并单独获取 Qt 开发头文件；发布内容为本项目补丁源码、脚本和文档。

## 参考

- [KWin 截图请求默认值](https://github.com/KDE/kwin/blob/Plasma/6.7/src/plugins/screenshot/screenshotdbusinterface2.cpp)
- [KWin 按 PID 过滤窗口](https://github.com/KDE/kwin/blob/Plasma/6.7/src/plugins/screenshot/screenshot.cpp)
- [Qt QDBusMessage](https://doc.qt.io/qt-6/qdbusmessage.html)

本工作区的验证记录见 [artifacts 索引](artifacts/process-patch-2026-10-30/index.md)。
