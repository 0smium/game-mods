# Nexus submission: SMTVV First Person

Game: Shin Megami Tensei V: Vengeance

Name: First Person Camera with Demon Haunt Visibility Fix

Summary: Explore Da'at in first person. Adjust eye height, field of view and head bob with keyboard shortcuts, and keep nearby Demon Haunt companions visible.

Version: 1.0.0-rc1 (release candidate)

## Description

This mod adds an adjustable first-person exploration camera. It starts enabled, remembers your F3 choice across map changes within a session, and pauses/restores the original camera in guarded menus and scripted views. It also prevents the verified near-camera hide behavior in the Demon Haunt while first-person is active, and fixes the verified player-body/purple-dash obstruction paths.

F3 toggles first person. F4 adjusts the eye offset, F5 adjusts FOV, and F6 adjusts head bob. Shift decreases; Ctrl uses a larger step; Ctrl+F3 restores the tuned defaults: +40 cm eye offset, 110 FOV, and 5 cm bob. Set bob to 0 to disable it. Fine steps are 1 cm,1 degree and0.25 cm. Settings save to an INI automatically. There is no in-game settings menu/HUD.

Install: close the game, browse its installed files in Steam, and extract the ZIP's Project folder into the game folder. Files belong beside SMT5V-Win64-Shipping.exe in Project/Binaries/Win64. An x64 ASI loader is required. Keep an existing loader; if none is installed, copy the optional included dsound.dll into Win64. Do not overwrite an existing proxy DLL. Then launch through Steam normally.

The native Demon Haunt free-camera fade fix is a separate download/mod. Either works alone; both can be installed together using the identical shared runtime file supplied in both packages. Use matching release versions. This mod does not include Debug Tools, creature scaling, Boss revival, graphics modifications or save editing. It does not promise first-person battles/cutscenes. Other camera mods or duplicate ASI installs may conflict.

The camera/visibility policies were tested in the author's earlier development builds. This focused packaging and finer controls are a release candidate; consult VALIDATION.md for the current test scope. Large eye offsets/FOV can reveal clipping.

Source and issue tracker: https://github.com/0smium/game-mods

## Credits and permissions

Osmium (0smium). MIT license; modification and redistribution allowed with the required copyright/license notices. Credits: Lyall/SMTVFix for SDK layouts/discovery reference, SafetyHook, Zydis/Zycore and ThirteenAG's Ultimate ASI Loader. Full license notices are included. Unofficial; not affiliated with ATLUS/SEGA.

Development used substantial generative-AI assistance for code/documentation with author-directed design and in-game testing. Apply the Nexus **AI-Generated Content** tag and **AI Media** tag (page text); do not disguise the extent of assistance as a solely human-authored implementation. No AI-generated game art/audio is included.

File: SMTVV-FirstPerson-1.0.0-rc1.zip. Requirements: x64 ASI loader (optional tested loader included); no requirement on SMTVFix itself. Use matching runtime versions with the separate visibility mod. Select an available Camera/Gameplay category appropriate to the actual Nexus game page. Screenshot/media should show real gameplay and be labelled if additional graphical mods are visible.
