# First Person Camera — 女神异闻录5 皇家版

以第一人称探索，可调眼高、FOV 和步态晃动。

[下载](https://github.com/0smium/game-mods/releases/tag/p5r-v1.0.0-rc1) · [Reloaded-II](https://github.com/Reloaded-Project/Reloaded-II/releases)

## 安装

把 `osmium.p5r.firstperson` 解压到 Reloaded-II 的 `Mods` 文件夹，
为 P5R 勾选 **First Person Camera**，从启动器进入游戏。
先停用其他第一人称模组。不需要 Persona Essentials 或调试菜单。
适用于 Windows x64、Steam 构建 15515071；启停模组需要重启游戏。

## 操作

默认第一人称，**F3** 切换第一／第三人称，正常切图保留本次选择。

| 按键 | 增量 | 默认值 |
| --- | --- | --- |
| F4 | 眼高 +1 cm | Joker 根节点上方 165 cm |
| F5 | 水平 FOV +1° | 110° |
| F6 | 晃动 +0.25 cm | 5 cm |

**Shift** 反向，**Ctrl** 大步（5 cm／5°／1 cm），同时按住则大步减少。
**Ctrl+F3** 恢复三项默认值；晃动为 0 关闭。参数自动保存到模组旁的
`P5RFirstPersonCamera.ini`。P5R 的眼高基准与 SMTVV 的偏移不同。

用于探索；正式战斗和无法接管的演出保留原版。隐藏 Joker，不提供可见身体或手臂。
支持鼠标与右摇杆控制视角；菜单和游戏失焦时暂停鼠标转向。

## 致谢与许可

感谢 **rirurin/p5r-freecam、OpenGFD、SafetyHook、Zydis、Zycore**。
采用 GPL-3.0-or-later，详见本模块 LICENSE 与第三方声明。
对应源码另附 `P5R-FirstPersonCamera-Source.zip`。
