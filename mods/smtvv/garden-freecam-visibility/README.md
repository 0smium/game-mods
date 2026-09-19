# SMTVV Demon Haunt Free Camera Visibility

Prevents the game's **native Demon Haunt free camera** from fading nearby demons when you move in for a close look.

Install the mod, enter the Demon Haunt, enable the game's free camera, and approach a demon. No hotkey or configuration is required. The fix does not add a free camera and does not enable or alter the exploration first-person camera. Use it alone or together with [First Person](../first-person/README.md).

The verified dedicated free-camera begin-overlap event temporarily receives an empty `OtherActor` input. The original event runs once, and the input is restored in the same call even during unwinding. The mod does not change opacity materials, collision, actor visibility flags, dialogue, game saves or other camera overlaps.

Install before launching the game. This prevents the confirmed fade trigger; it is not a general recovery tool for demons already faded before the mod loaded, or for unrelated effects. Exit and re-enter the free camera if needed. Early-spawn/all-map coverage is not claimed.

Disable by removing `SMTVVGardenFreecam.asi` while the game is closed. Keep the shared runtime if First Person is still installed. See [installation](../../../../README.md#installation) and [validation](../../../../VALIDATION.md).
