# SMTVV 相机模组安装说明

[English](install.md)

适用于 **Windows x64 / Steam 版 SMT V: Vengeance**。两份模组可单独安装，也可一起使用。

1. 关闭游戏。在 Steam 打开游戏的 **属性 → 已安装文件 → 浏览**。
2. 把下载包内的 `Project` 文件夹解压到游戏目录。模组文件应位于 `Project/Binaries/Win64`，与 `SMT5V-Win64-Shipping.exe` 放在一起。
3. 如果已有 ASI 加载器（例如安装 SMTVFix 时带的），沿用即可。没有的话，把包内 `Optional-ASI-Loader/dsound.dll` 复制到同一个 `Win64` 文件夹。已有同名文件时不要覆盖。
4. 从 Steam 正常启动，无需额外启动器。

两份安装包带有相同的 `SMTVVCameraRuntime.dll`，一起安装时使用同一版本即可。

## 更新与卸载

先关闭游戏，再替换文件。更新时替换旧 `SMTVVFirstPerson.asi`，不要在其他加载器目录保留重复副本。更新包不包含实际的 `SMTVVFirstPerson.ini`，所以不会覆盖你的配置。

卸载时删除对应文件：

- 第一人称：`SMTVVFirstPerson.asi`
- 自由视角防透明：`SMTVVGardenFreecam.asi`

两份都卸载后，可以删除 `SMTVVCameraRuntime.dll` 及其日志，以及第一人称的 INI、status 文件。其他模组仍在使用加载器时，保留加载器。

## 没有生效？

检查文件是否放在游戏 EXE 旁边，而不是 `Content/Paks`，以及是否安装了 x64 ASI 加载器。移除旧开发版或重复副本。其他相机模组可能冲突；尚未验证 Reloaded-II、Vortex 专用包及 Linux / Steam Deck。
