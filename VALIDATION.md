# Validation status

Public packaging version: **1.0.0-rc1**.

Previously confirmed by the author in the development build: exploration first-person and map-change intent, nearby Demon Haunt visibility and dialogue, purple-form sprint obstruction removal, and the native garden free-camera fade fix. Those policy modules were retained for the public split.

Local verification of the split: MSVC x64 Release compilation;17 production camera lifecycle scenarios,15 scoped garden scenarios,16 native free-camera scenarios,14 dash scenarios; fine/coarse tuning, saturation, malformed/nonfinite values and exhaustive401841 tuning roundtrips. Windows tests exercise the production keyboard polling, focus behavior, unused F9 and real INI persistence using simulated input and an isolated temporary directory.

Not yet accepted for this public candidate: live split-module installation alone/together, saved fine adjustments across a restart, every new tuning value, other game versions, Reloaded-II/Vortex, Linux/Steam Deck. Build and adapter checks do not establish live visual acceptance. Public candidate reports should state the game version, installed feature(s) and steps to reproduce; avoid sharing private saves or account information.
