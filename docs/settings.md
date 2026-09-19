# Camera settings and compatibility

## First-person tuning

| Setting | Range | Default | Fine step | Ctrl step |
| --- | --- | --- | --- | --- |
| Eye offset | -20 to +100 cm | +40 cm | 1 cm | 5 cm |
| FOV | 60–140° | 110° | 1° | 5° |
| Head bob | 0–10 cm | 5 cm | 0.25 cm | 1 cm |

F4/F5/F6 increase these values. Shift decreases; Ctrl selects the larger step. Ctrl+F3 resets all three without toggling first person. Each press changes the value once, with no hold-to-repeat; values stop at the limits. Alt combinations are ignored.

Eye offset is relative to the calibrated camera baseline, not absolute character height. Head bob follows walking and does not add bob while airborne. Set it to 0 to disable it. Large offsets or FOV can reveal clipping.

Changes save automatically to `SMTVVFirstPerson.ini` after about half a second. The INI is loaded on launch; restart after editing it manually. `SMTVVFirstPerson.status.txt` reports values and configuration-write failures. There is no settings HUD. Tuning requires a keyboard; native gamepad movement and look remain available.

F3's on/off choice carries across maps within a session. Each new game launch starts first person enabled. The camera is intended for exploration; battle and scripted views are not first-person features.

## Freecam visibility

The separate visibility mod has no settings. It works with the game's native Demon Haunt free camera. Install it before launching the game; exit and re-enter the free camera if a demon has already faded. It addresses the identified nearby-demon fade trigger, not every possible visibility effect.

## Compatibility

Both mods can be used together with matching runtime versions. Other mods controlling the same camera or engine hook may conflict. These mods do not modify saves. Testing scope and target executable are in [validation](validation.md) and [runtime notes](../mods/smtvv/common/README.md).
