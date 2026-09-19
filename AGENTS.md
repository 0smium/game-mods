# Mod publication guidelines

These are the maintainer's publication preferences. Read before changing packaging, repository presentation or release pages.

- Keep About generic: `Open-source game mods by Osmium, organized by game.` List games and mods in the README directory, not in About.
- Keep player pages focused on purpose, installation and controls. Put build details, architecture, checksums and validation evidence in developer documentation.
- Release text should describe the mod and link to installation instructions. Include actual user-visible changes when relevant; omit agent work logs and packaging/editorial process narratives.
- Keep download ZIPs small in structure: runtime files, a required optional loader where applicable, one short installation guide and consolidated license notices. Do not include development logs, validation reports, manifests or duplicate guides.
- Preserve every required copyright/license notice and any source-distribution obligations. Each game/module's license applies independently; do not assume the root MIT license covers every module.
- Prepare one finished package before publishing. Avoid multiple public downloads for editorial or packaging-only variants of the same mod. Keep genuinely different functional release history.
- Before replacing an erroneous published package, keep a recoverable local copy, verify the replacement and update checksums and download links. Do not remove unrelated releases or Git history.
- For packaging-only changes, verify archive contents, unchanged runtime binaries and complete licenses; do not redeploy the game just for documentation changes.
- Preserve other sessions' uncommitted work. Stage only the authorized module and intended edits. A release request for one game is not authorization to publish another game's pending work.
- Respect the latest publication authorization and visibility preference. Do not repeatedly ask for permission already given. Describe testing accurately and update stale pending-test claims when feedback arrives.
- Nexus pages should list real player dependencies, accurate license permissions and current required disclosure tags. Use real gameplay images; keep submission drafts outside player packages.

- Player-facing mod folders use readable product names (for example `P5RFirstPersonCamera`), not internal reverse-domain ModIds. Preserve game-required installation paths; keep stable internal IDs inside ModConfig.json.
