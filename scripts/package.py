"""Create two allowlisted release ZIPs. Never copy a live game directory."""
from pathlib import Path
import argparse, hashlib, json, zipfile
repo = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, default=repo / 'build/Release')
parser.add_argument('--loader-archive', type=Path, required=True, help='Official SMTVFix_1.0.0.zip; hash is verified before extracting only dsound.dll')
args = parser.parse_args()
version = '1.0.0-rc1'
sha = lambda b: hashlib.sha256(b).hexdigest().upper()
assert sha(args.loader_archive.read_bytes()) == '7D2A3AFBD9054A71FA37EA16B01E886E43E75EC17697498DCD17120EAA758DAD', 'Unexpected loader source archive'
with zipfile.ZipFile(args.loader_archive) as upstream:
    loader = upstream.read('dsound.dll')
assert sha(loader) == '2DA9374DBE7089706EC86FE008E406BC9F562E105F7A07D5478542E738A45662'
common = (args.build_dir / 'SMTVVCameraRuntime.dll').read_bytes()
assert common[:2] == b'MZ', 'Build the runtime first'
out = repo / 'dist' / version
out.mkdir(parents=True, exist_ok=True)
# Keep complete license texts and attribution, combined into one file.
license_sources = [repo / 'licenses/LICENSE-MIT', *sorted((repo / 'licenses').glob('*.txt'))]
license_text = '\n\n'.join(
    p.name + '\n' + p.read_text(encoding='utf-8') for p in license_sources
)
license_text += (
    '\n\nCredits: Lyall / SMTVFix (SDK layouts and discovery reference); '
    'cursey / SafetyHook; Zydis and Zycore contributors; '
    'ThirteenAG / Ultimate ASI Loader.\n'
    'Third-party sources: https://github.com/0smium/game-mods/blob/main/THIRD_PARTY_NOTICES.md\n'
    'Unofficial mod; game content belongs to its respective rights holders.\n'
)
results = {}
for feature, filename, folder in [('FirstPerson', 'SMTVVFirstPerson.asi', 'first-person'), ('GardenFreecamVisibility', 'SMTVVGardenFreecam.asi', 'garden-freecam-visibility')]:
    payload = {
        'Project/Binaries/Win64/' + filename: (args.build_dir / filename).read_bytes(),
        'Project/Binaries/Win64/SMTVVCameraRuntime.dll': common,
        'Optional-ASI-Loader/dsound.dll': loader,
        'README.txt': (repo / 'mods/smtvv' / folder / 'INSTALL.txt').read_bytes(),
        'LICENSES.txt': license_text.encode('utf-8'),
    }
    for name in payload:
        assert not name.startswith('/') and '..' not in Path(name).parts
        assert not name.endswith(('.sav', '.pdb', '.log', '.ini')), name
    name = f'SMTVV-{feature}-{version}.zip'
    target = out / name
    assert not target.exists(), 'Do not overwrite a release artifact; use a new version/directory'
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for k, v in payload.items(): archive.writestr(k, v)
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == set(payload)
        for k, v in payload.items(): assert archive.read(k) == v
    results[name] = sha(target.read_bytes())
(out / 'SHA256SUMS.txt').write_text(''.join(f'{h}  {n}\n' for n, h in results.items()), encoding='ascii')
print(json.dumps(results, indent=2))
