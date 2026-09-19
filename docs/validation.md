# Validation

Version: **1.0.0-rc1**.

On 2026-09-19, the author reported no problems after testing the deployed candidate. The requested check covered camera adjustment/reset keys, nearby Demon Haunt visibility in first person, and visibility in the native free camera with first person disabled. This records the overall feedback; it does not establish every parameter value, restart-persistence case or separate installation combination.

Earlier development testing also confirmed exploration first person, map-change intent, close-demon visibility and dialogue, purple-form sprint obstruction removal, and the native free-camera fade fix. Those policy modules were retained in the split release.

## Automated checks

- MSVC x64 Release builds using the pinned local dependency checkout and a fresh dependency download.
- 17 camera lifecycle, 15 garden visibility, 16 native free-camera and 14 dash scenarios.
- Fine/coarse settings, bounds, invalid values and 401,841 settings roundtrips.
- Windows production keyboard polling, focus behavior and real INI persistence under simulated input.
- Four isolated actual ASI-loading cases: each feature alone and both loading orders.
- GitHub Linux regression and Windows build jobs passed for the initial release commit.

Other game revisions, all parameter extremes, Reloaded-II/Vortex packages and Linux/Steam Deck remain unverified. See the [runtime notes](../mods/smtvv/common/README.md) for the tested executable. Reports should include game version, installed mod(s) and reproduction steps.
