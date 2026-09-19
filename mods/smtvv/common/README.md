# Shared SMTVV camera runtime

Both feature ASIs import one `RegisterCameraFeature(feature, abi)` entry from `SMTVVCameraRuntime.dll`. Registration runs outside the loader lock and is serialized; the runtime installs one ProcessEvent hook for the union of enabled features. Both ZIPs carry the same runtime version. Mixing versions or loading duplicates from several folders is unsupported.

The first-person feature registers bit1; native free-camera visibility registers bit2. With bit1 absent, no first-person ControlBefore/ControlAfter, near-camera garden permission changes, keyboard polling, configuration creation or first-person status writing is performed. Camera callback observation still establishes the verified game thread needed by the free-camera guard. The free-camera overlap fix only runs with bit2 enabled.

Code discovery requires unique executable signatures. UObject access is restricted to guarded game callbacks with indexed identities and complete contextual checks. Temporary garden permission and free-camera parameter changes are restored in a finally scope; native callbacks are not skipped or repeated. Layout failures disable control rather than trying guessed offsets. Removal/hot unloading during a running game is unsupported; restart to change the installed features.

The camera, garden, owner visibility, dash and free-camera policies originate from the author's tested development version. Broad evidence collection, debug travel, boss revival, actor picking, scale controls and HUD diagnostics were removed for publication. The only periodic worker work is first-person key polling and local status/config writing; no Boss/GObjects list scans remain.

Targets the SDK/layouts used by SMTVFix1.0.0 and the Steam executable tested by the author (SHA256 `8E38E3C39CD2156FB782D4EC742DA55FE287D58703F4AE049C00B2DF1AC76035`). Other game revisions/platforms are unverified. Unique signatures do not guarantee all layouts remain compatible with a future update.
