# P5RFirstPerson source and dependency notices

This native ASI module uses camera/engine research from the projects below. It is
an experimental adaptation, not an official release by the upstream authors.
Keep this notice, the applicable licenses, the adapted source, and the build
instructions together when distributing a build. The adapted module is licensed
under GNU GPL v3; see `LICENSE`. No proprietary P5R executable or game asset is
included in this source project.

## Camera and engine structure research

- **p5r-freecam**, rirurin and contributors:
  <https://github.com/rirurin/p5r-freecam>
  - Reviewed commit: `17a6aeca80fa94b04624b5acfcf41c5c84da641a`.
  - License: GNU GPL v3 (see the upstream repository).
  - Upstream reference sources are linked above.
  - Camera structures, signatures and hook behavior are adapted with attribution;
    the full freecam GUI and camera-path application are not linked into this ASI.
- **OpenGFD**, rirurin and contributors:
  <https://github.com/rirurin/opengfd>
  - Reviewed commit: `167739405d2e1f0520980186b7ad6fbcf3f9e559`.
  - License: GNU **LGPL v3**, as verified from `upstream-opengfd/LICENSE`.
  - GFD/xrd-related structure research is
    referenced/adapted without linking the entire OpenGFD library.
  - License copy: `licenses/OpenGFD-LGPL-3.0.txt`.

## Native build dependencies reused locally

The build uses pinned SafetyHook, Zydis and Zycore source dependencies.
It does not link SMTVFix gameplay code and does not deploy anything to SMT3/SMTVV.

- **SafetyHook**, cursey and contributors: <https://github.com/cursey/safetyhook>.
  Boost Software License 1.0; `licenses/SafetyHook-BSL-1.0.txt`.
  `compat/safetyhook-os.windows.cpp` is a local copy of its Windows implementation.
  Its virtual-memory query returns `MEMORY_BASIC_INFORMATION.BaseAddress` instead
  of `AllocationBase`: the near allocator walks regions, and free regions have
  a null allocation base. This fixes skipped free space with Reloaded-II loaded.
  The shared SMTVV source remains unchanged.
  `compat/safetyhook-allocator.cpp` additionally tries Windows `VirtualAlloc2`
  with the intersection of all near-address constraints before the existing
  manual search. This avoids depending on the placement of CLR/Reloaded buffers.
  It allocates trampoline storage in page-sized blocks; `compat/safetyhook-inline.cpp`
  retains the near-allocation error before fallback so startup diagnostics can
  distinguish memory exhaustion from relocation errors. The ASI pins one shared
  near pool and waits boundedly before installing any hooks.
- **Zydis**, Florian Bernd, Joel Höner and contributors:
  <https://github.com/zyantific/zydis>. MIT; `licenses/Zydis-MIT.txt`.
- **Zycore**, Zyantific contributors: <https://github.com/zyantific/zycore-c>.
  MIT; `licenses/Zycore-MIT.txt`.

## Loader staged separately

- **Ultimate ASI Loader**, ThirteenAG and contributors:
  <https://github.com/ThirteenAG/Ultimate-ASI-Loader>.
  MIT; `licenses/Ultimate-ASI-Loader-MIT.txt`.
- Official binary release: **v9.7.4**, x64 NoPDB.
  <https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB_x64.zip>
- Downloaded ZIP SHA256:
  `E5860E7D9A1805267535B65749575B5E406CC6EA3325C7392189C578815045D1`.
- Extracted `dinput8.dll` SHA256:
  `031A3E5576D91DCE1E438D36B9A3D462C7334AB4791990A8FF1E3DDC0E132DAF`.
- Verified file/product version: `9.7.4`. These are locally recorded hashes of
  the official HTTPS download, not a separately published publisher checksum.
- `dinput8.dll` is chosen because the reviewed P5R executable imports it. This
  preserves `dxgi.dll` for a possible later ReShade setup. No ReShade/NR component
  is installed by this module's deployment script.
- The loader is optional for manual development use and is not included in the Reloaded-II player package.

## Public camera build

Modified by Osmium: view-relative movement, scoped restoration, bob, settings and loader support. Public module source is in this folder. Build using the repository build.ps1; corresponding source with pinned dependencies and BUILD.md is supplied separately in P5R-FirstPersonCamera-Source.zip. The camera-only module contains no debug menu, weather or NR code. Load it either through Reloaded-II ModConfig.json or Ultimate ASI Loader, never both. The source license remains GPL-3.0-or-later; compatible dependency licenses remain in licenses/.
