from pathlib import Path
import argparse, json, os, shutil, subprocess
if os.name != 'nt': raise SystemExit('Windows loader smoke test only')
repo = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--build-dir', type=Path, default=repo / 'build/Release')
args = p.parse_args()
out = repo / 'build-module-tests'
out.mkdir(exist_ok=True)
host = out / 'module_load.exe'
subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', str(repo / 'tests/native_module_load.cpp'), '-o', str(host)], check=True)
results = {}
for case, names in {'first-alone':['SMTVVFirstPerson.asi'], 'free-alone':['SMTVVGardenFreecam.asi'], 'first-free':['SMTVVFirstPerson.asi','SMTVVGardenFreecam.asi'], 'free-first':['SMTVVGardenFreecam.asi','SMTVVFirstPerson.asi']}.items():
    folder = out / case
    assert not folder.exists(), 'Keep previous smoke results; select a clean checkout for a rerun'
    folder.mkdir()
    for name in names + ['SMTVVCameraRuntime.dll']: shutil.copy2(args.build_dir / name, folder / name)
    subprocess.run([str(host), *[str(folder / n) for n in names]], check=True)
    log = (folder / 'SMTVVCameraRuntime.log').read_text(encoding='utf8')
    assert log.count('START v1.0.0-rc1') == 1 and log.count('STOP: unsupported host executable') == 1
    assert 'discovery ProcessEvent' not in log and 'READY runtime' not in log
    assert not (folder / 'SMTVVFirstPerson.ini').exists()
    results[case] = 'one runtime; host rejected before game discovery or writes'
(out / 'verification.json').write_text(json.dumps(results, indent=2), encoding='utf8')
print(json.dumps(results, indent=2))
