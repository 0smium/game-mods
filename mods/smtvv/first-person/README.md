# SMTVV First Person

Exploration first-person camera for the Windows Steam version of **Shin Megami Tensei V: Vengeance**.

- Starts enabled; F3 toggles and retains your choice across map changes within the session.
- Restores the game's camera during guarded menu/story/camera transitions, then resumes eligible exploration.
- Prevents the verified Demon Haunt near-camera hide path while first-person is active.
- Hides local-player body geometry from the owner camera and handles the verified purple-form dash obstruction. Other characters are not globally hidden.
- Adjustable eye offset, FOV and movement-driven head bob. No changes to movement, jumping, collision sizes or game saves.

| Control | Fine step | Ctrl coarse step | Range | Default |
| --- | --- | --- | --- | --- |
| F4 eye offset | +1 cm | +5 cm | -20 to +100 cm | +40 cm |
| F5 FOV | +1 degree | +5 degrees | 60 to 140 | 110 |
| F6 head bob | +0.25 cm | +1 cm | 0 to 10 cm | 5 cm |

Hold Shift with those keys to decrease; Ctrl+Shift decreases by the coarse step. Ctrl+F3 resets these three values without toggling first-person. Bounds clamp instead of wrapping. A fresh press makes one adjustment; no keyboard auto-repeat. Alt combinations are ignored. Set bob to 0 to disable it.

Tuning saves to `SMTVVFirstPerson.ini` after input settles, and loads at the next launch. The example INI is documentation only; updates do not overwrite your actual INI. The status text file reports the current values and whether config writing failed. There is no in-game settings menu/HUD in this focused release. Keyboard input is needed for tuning; native gamepad movement/look is retained.

Eye offset is relative to the calibrated camera baseline, not an absolute character height. Wide FOV/high eye offsets can show clipping; not every newly allowed value has been visually tested. This is an exploration camera, not a promise of first-person battle/cutscene support. No near-clip override, body-rendering option or arbitrary free flight is added.

For the native Demon Haunt free camera, install the [separate visibility fix](../garden-freecam-visibility/README.md). F3 alone does not enable that fix. See [installation](../../../../README.md#installation) and [validation](../../../../VALIDATION.md).
