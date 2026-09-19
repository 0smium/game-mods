# Install SMTVV camera mods

[中文](install.zh-CN.md)

For **Windows x64 / Steam SMT V: Vengeance**. Either mod can be installed alone, or both together.

1. Close the game. In Steam, open **Properties → Installed Files → Browse**.
2. Extract the downloaded ZIP's `Project` folder into that game folder. The mod files go in `Project/Binaries/Win64`, beside `SMT5V-Win64-Shipping.exe`.
3. If you already have an ASI loader, such as one installed with SMTVFix, keep it. Otherwise, copy the package's `Optional-ASI-Loader/dsound.dll` into the same `Win64` folder. Do not overwrite an existing `dsound.dll`.
4. Launch normally through Steam. No extra launcher is needed.

Both packages include the same `SMTVVCameraRuntime.dll`. Use files from the same release when installing both.

## Update or remove

Close the game before changing files. Replace an older `SMTVVFirstPerson.asi`; do not keep duplicate copies in other loader folders. Your actual `SMTVVFirstPerson.ini` is not included in updates, so it is preserved.

To remove a mod, delete its file:

- First Person: `SMTVVFirstPerson.asi`
- Freecam Visibility: `SMTVVGardenFreecam.asi`

After removing both, you may also remove `SMTVVCameraRuntime.dll` and its log, plus the first-person INI/status files. Keep the ASI loader if other mods use it.

## If it does not load

Check that the files are beside the game EXE, not in `Content/Paks`, and that an x64 ASI loader is installed. Remove duplicate or old development copies of these mods. Other camera mods may conflict. Reloaded-II/Vortex packages and Linux/Steam Deck are not verified.
