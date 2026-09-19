# First Person Camera — Persona 5 Royal

Explore in first person with adjustable eye height, FOV and head bob.

[中文](README.zh-CN.md)

[Download](https://github.com/0smium/game-mods/releases/tag/p5r-v1.0.0-rc1) · [Reloaded-II](https://github.com/Reloaded-Project/Reloaded-II/releases)

## Install

Extract the `osmium.p5r.firstperson` folder into Reloaded-II's `Mods` directory,
enable **First Person Camera** for P5R, and launch through Reloaded-II.
Disable other first-person camera mods first. No Persona Essentials or debug menu
is required. Windows x64, Steam build 15515071. Restart to enable/disable the mod.

## Controls

Starts in first person. **F3** switches first/third person and retains your choice
across normal map changes.

| Key | Increase | Default |
| --- | --- | --- |
| F4 | Eye height +1 cm | 165 cm above Joker's root |
| F5 | Horizontal FOV +1° | 110° |
| F6 | Head bob +0.25 cm | 5 cm |

**Shift** decreases; **Ctrl** uses larger steps (5 cm / 5° / 1 cm).
**Ctrl+F3** resets all three. Bob 0 disables it. Changes save automatically to
`P5RFirstPersonCamera.ini` beside the mod. Height uses a different reference from SMTVV.

For exploration. Native battles and unsupported cinematic cameras are retained.
Joker is hidden to avoid obstruction; visible arms/body are not provided.
Mouse look and controller right-stick look are both supported. Mouse input pauses
in menus and while the game is unfocused.

## Credits and license

Camera/engine research: **rirurin/p5r-freecam** and **OpenGFD**.
Hooking: **SafetyHook**, **Zydis**, **Zycore**.
GPL-3.0-or-later; see `LICENSE` and `THIRD_PARTY_NOTICES.md`.
Corresponding source is supplied separately as `P5R-FirstPersonCamera-Source.zip`.
