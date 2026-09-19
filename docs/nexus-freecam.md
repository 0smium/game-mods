# Nexus submission: Demon Haunt Free Camera Visibility

Game: Shin Megami Tensei V: Vengeance

Name: Demon Haunt Free Camera - No Close-Up Fade

Summary: Keep demons visible when the game's native Demon Haunt free camera moves close to them. Install and use; no hotkey or configuration required.

Version: 1.0.0-rc1 (release candidate)

## Description

The Demon Haunt's native free camera normally triggers a fade when approaching a demon. This mod prevents that specific verified overlap-triggered fade, making close-up viewing easier.

It uses the game's existing free camera; it does not add a new camera, enable first person, or alter collision/material opacity globally. No hotkeys or configuration are needed. Install before launching, enter the Demon Haunt, enable its native free camera and approach a demon. Exit/re-enter the free camera if the demon had already faded; the mod prevents the trigger rather than repairing arbitrary pre-existing fades.

Install: extract the ZIP's Project folder into the Steam game folder. The ASI and shared runtime go in Project/Binaries/Win64 beside SMT5V-Win64-Shipping.exe. Retain an existing x64 ASI loader; if none exists, copy the optional included dsound.dll into that folder. Never blindly overwrite an existing proxy DLL. Launch normally through Steam.

This mod works independently of First Person. If both are installed, keep the shared runtime included with the matching release. Remove SMTVVGardenFreecam.asi to disable this feature, keeping the runtime if First Person still uses it. No saves, debug tools, creature sizes, Boss state or graphics settings are modified.

The original fade fix was tested during development. Separate packaging/combination testing for this release candidate is tracked in VALIDATION.md; coverage of every map, early-spawn timing and unrelated fade sources is not claimed.

Source and issue tracker: https://github.com/0smium/game-mods

## Credits and permissions

Osmium (0smium), MIT license. Redistribution/modification allowed with required notices. Credits: Lyall/SMTVFix, SafetyHook, Zydis/Zycore and ThirteenAG's Ultimate ASI Loader; all relevant license notices are included. Unofficial and not endorsed by ATLUS/SEGA.

Development used substantial generative-AI assistance for code/documentation, directed and tested by the author. Nexus tags: **AI-Generated Content**, **AI Media** (page text). No AI-generated in-game art/audio is included.

File: SMTVV-GardenFreecamVisibility-1.0.0-rc1.zip. Requirements: x64 ASI loader; optional tested loader included. No dependency on the First Person mod or on Debug Tools. Use actual in-game screenshots, not fabricated before/after claims.
