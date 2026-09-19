"""Compile production modules against explicit engine adapters; never access a game."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

repo = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default=shutil.which('g++'))
args = parser.parse_args()
if not args.compiler:
    raise SystemExit('Install a C++20 g++ compiler for the portable regression harness.')
out = repo / 'build-tests'
out.mkdir(exist_ok=True)
source = (repo / 'mods/smtvv/common/camera_control.inl').read_text(encoding='utf8')
needle = '#include "visibility_control.inl"\n#include "camera_motion.inl"'
assert source.count(needle) == 1
(out / 'camera_control.test.inl').write_text(source.replace(needle, '#include "lifecycle_dependencies.inl"'), encoding='utf8')
cases = ['native_lifecycle', 'native_garden', 'native_free_camera', 'native_dash', 'native_settings_policy']
if os.name == 'nt': cases.append('native_settings_windows')
results = {}
for name in cases:
    binary = out / (name + ('.exe' if os.name == 'nt' else ''))
    command = [args.compiler, '-std=c++20', '-O0', '-g', '-Wall', '-Wextra', '-Werror', '-DTEST_STARTUP_REQUEST=true', '-I', str(out), '-I', str(repo / 'tests'), str(repo / 'tests' / (name + '.cpp')), '-o', str(binary)]
    subprocess.run(command, check=True)
    result = subprocess.run([str(binary)], text=True, capture_output=True)
    print(result.stdout, end='')
    (out / (name + '.txt')).write_text(result.stdout + result.stderr, encoding='utf8')
    results[name] = result.returncode
    if result.returncode: raise SystemExit(result.stderr or f'{name} failed')
hashes = {p.relative_to(repo).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in (repo / 'mods').rglob('*') if p.is_file()}
(out / 'verification.json').write_text(json.dumps({'checks': results, 'sourceHashes': hashes, 'scope': 'Adapters and settings IO, not live game acceptance'}, indent=2), encoding='utf8')
