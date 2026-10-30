# 进程补丁验证记录 · 2026-10-30

## 环境

- KDE / Wayland，KWin 6.7.5，ScreenShot2 接口 Version 5。
- 原 AppImage：`/home/pi/Portable/snipaste/Snipaste-2.11.3-x86_64.AppImage`。
- 内置 Qt 6.2.4；编译使用同版本头文件与内置动态库。
- GCC 16.2.1，Release 构建；补丁约 40 KiB。
- 未收集、存储屏幕图像。

## 证据

| 文件 | 内容 |
| --- | --- |
| [build.log](build.log) | 初版补丁编译结果 |
| [tests.log](tests.log) | 初版同步、异步预加载测试 |
| [prepare.log](prepare.log) | 稳定提取路径下的完整构建与 2/2 测试 |
| [kwin-probe.log](kwin-probe.log) | 通过补丁调用真实 Qt D-Bus 同步、异步读取 KWin 接口版本 |
| [benchmark.log](benchmark.log) | 模拟传输路径微基准，9 组 × 10,000 次 |
| [capture-trace.log](capture-trace.log) | 用户触发实际截图时的请求改写日志 |
| [launcher-guard.log](launcher-guard.log) | 已运行 Snipaste 时拒绝再次启动 |
| [input-default.log](input-default.log) | 同套 Qt/xcb 缺省加载 compose，未加载 Fcitx5 |
| [input-fcitx.log](input-fcitx.log) | 设置 QT_IM_MODULE=fcitx 后成功加载内置 Fcitx5 插件 |

## 结论

- 同步、异步发送入口均被 ELF 符号预加载正确拦截。
- 精确匹配目标；其余目标或方法不变；原消息、标志、超时、文件描述符保持完整。
- 真实 KWin 属性调用正常，补丁可转发至真实 Qt 实现。
- 实际 Snipaste 截图请求中 `hide-caller-windows` 缺省，已被改成 `false`；截图调用后进程仍在运行。
- 模拟传输基线中位数 0.779 µs/次，补丁版 1.169 µs/次，差值约 0.390 µs/次。此指标衡量控制消息改写开销；桌面按键到框选界面的端到端延迟待测。
- 用户报告一次 overlay 交叠，随后相同操作未能复现；原因未确定，暂不修改窗口层级逻辑。
- 输入法故障定位为 xcb 进程缺省加载 compose。设置 QT_IM_MODULE=fcitx 后，独立探测和重启后的 Snipaste 均成功加载内置 Fcitx5 插件；用户回复 done，输入法排查完成。
- 启动器在已有 Snipaste 实例时拒绝启动，避免单实例转发掩盖补丁未加载的情况。
