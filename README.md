# Game Mods

Small, focused game mods by [Osmium](https://github.com/0smium). MIT-licensed source, grouped by game. [中文说明](README.zh-CN.md).

## SMT V: Vengeance

| Mod | Features | Configuration |
| --- | --- | --- |
| [First Person](mods/smtvv/first-person/README.md) | Exploration camera, adjustable eye offset/FOV/head bob, nearby Demon Haunt visibility, first-person body/dash obstruction fixes | F3–F6; saved INI |
| [Demon Haunt Free Camera Visibility](mods/smtvv/garden-freecam-visibility/README.md) | Prevents the native free camera from fading nearby demons | Install and use; no hotkey |

Download the corresponding ZIP from [Releases](https://github.com/0smium/game-mods/releases). These are two separate downloads; either can be used alone, or both together. They share `SMTVVCameraRuntime.dll`, included identically in each ZIP. Keep all installed files from the same release.

Current public version: **1.0.0-rc1**. The original camera and visibility features were verified in-game during development. The split packaging and expanded controls are release candidates until their dedicated live checks are complete. See [validation](VALIDATION.md).

No creature scaling, boss revival, debug-menu access, save editing, graphics overhaul or replacement free-camera implementation is included. P5R and other games can be added as `mods/<game>/<mod>` when ready; this repository does not contain unfinished placeholders for them.

## Installation

Windows x64 / Steam SMT V: Vengeance. Close the game first.

1. Open Steam → game properties → Installed Files → Browse.
2. Extract the ZIP's `Project` folder into the game folder. Files go under `Project/Binaries/Win64`, beside `SMT5V-Win64-Shipping.exe`.
3. An **x64 ASI loader** is required. If one is already installed (for example with SMTVFix), retain it. For a clean installation, copy the included `Optional-ASI-Loader/dsound.dll` into that same `Win64` folder. Do not overwrite an existing proxy DLL.
4. Launch normally through Steam. No extra launcher, console, UE4SS or Debug Tools is required.

Do not install two copies of a feature in different ASI loader folders. Replace older development versions of `SMTVVFirstPerson.asi`; do not keep the old all-in-one version beside these mods. Other mods that control the same first-person camera or patch the same engine entry can conflict. These packages have **not** been validated as Reloaded-II or Vortex packages. `.pak` mods and ASI mods have different install locations; do not put these binaries in `Content/Paks`.

To uninstall one feature, remove its `.asi`. Keep the shared runtime while either feature remains installed. Once both are removed, remove the runtime and the first-person INI/status/log files if desired. Remove an ASI loader only if no remaining mods use it. No game saves are modified by these mods.

## Build

Requires Git, CMake 3.24+, PowerShell 7, and Visual Studio 2022 C++ x64 build tools.

```powershell
./build.ps1
```

Dependencies are fetched from official repositories at a pinned SMTVFix 1.0.0 revision and its pinned submodules. Output is `build/Release`. An existing complete pinned checkout may be supplied using `-SmtvfixSource <path>`. The SDK is fetched as a build dependency, not committed here. The build never installs files into a game.

Run the production-module regression harness with Python 3 and a C++20 `g++` compiler:

```console
python scripts/test.py
```

The Windows run additionally exercises real INI persistence and production keyboard polling with simulated input. Tests use engine adapters and do not launch or manipulate a game. See [technical notes](mods/smtvv/common/README.md), [third-party notices](THIRD_PARTY_NOTICES.md) and [MIT license](LICENSE).

AI-assisted development: code and documentation were developed with substantial generative-AI assistance, guided by the author's design decisions and in-game testing. This disclosure does not replace validation.
