# Development

## Build

Requires Git, CMake 3.24+, PowerShell 7 and Visual Studio 2022 C++ x64 build tools.

```powershell
./build.ps1
```

Output: `build/Release`. Dependencies come from the official SMTVFix repository at a pinned 1.0.0 revision and its pinned submodules. Use `-SmtvfixSource <path>` for an existing complete pinned checkout. Building does not install anything into a game.

## Tests

With Python 3 and a C++20 `g++` compiler:

```console
python scripts/test.py
```

Windows runs additionally exercise keyboard polling and INI persistence using simulated input. The tests do not interact with a game. `scripts/test_modules.py` checks actual ASI loading in isolated test processes after building.

## Layout

- `mods/<game>/<mod>/`: feature entry points and player instructions.
- `mods/smtvv/common/`: shared camera runtime and policies.
- `tests/`, `scripts/`: regression checks and packaging tools.
- `licenses/`: third-party license texts, also included in release packages.

[Runtime notes](../mods/smtvv/common/README.md) · [Validation](validation.md) · [Credits](../THIRD_PARTY_NOTICES.md) · [MIT license](../LICENSE)

Code and documentation were developed with substantial generative-AI assistance, guided by the author's design decisions and in-game testing.
