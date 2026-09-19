"""Package local candidates only. Never installs or uploads files."""
from pathlib import Path
import argparse, hashlib, io, json, zipfile
p=argparse.ArgumentParser()
p.add_argument('--dependencies',type=Path,required=True)
p.add_argument('--build',default='build-p5r')
a=p.parse_args()
root=Path(__file__).resolve().parents[1];mod=root/'mods/p5r/first-person'
binary=root/a.build/'mods/p5r/first-person/Release/P5RFirstPersonCamera.dll'
dist=root/'dist';dist.mkdir(exist_ok=True)
source=io.BytesIO()
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for f in sorted(mod.rglob('*')):
        if f.is_file():z.write(f,'module/'+f.relative_to(mod).as_posix())
    for name in ('safetyhook','zydis'):
        for f in sorted((a.dependencies/name).rglob('*')):
            relative=f.relative_to(a.dependencies)
            if not f.is_file() or any(x in ('.git','build','__pycache__') for x in relative.parts):continue
            if f.suffix.lower() in ('.dll','.exe','.pdb','.obj','.lib'):continue
            z.write(f,'dependencies/'+relative.as_posix())
    z.writestr('CMakeLists.txt','''cmake_minimum_required(VERSION 3.24)
project(P5RCameraSource LANGUAGES C CXX)
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "MSVC x64 required")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_VS_GLOBALS "ImportDirectoryBuildProps=false;ImportDirectoryBuildTargets=false;AutoDeploy=false")
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
add_subdirectory(dependencies/zydis)
set(SAFETYHOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/safetyhook")
enable_testing()
add_subdirectory(module)
''')
    z.writestr('BUILD.md','''# Build corresponding source
Windows x64, Visual Studio 2022 C++ tools, CMake 3.24+.
From a VS x64 developer shell:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

Output: build/module/Release/P5RFirstPersonCamera.dll.
For Ultimate ASI Loader copy it with extension .asi.
Bundled dependencies need no download. Licenses are in module/.
''')
license_text=(mod/'THIRD_PARTY_NOTICES.md').read_text(encoding='utf-8')+'\n\n'+(mod/'LICENSE').read_text(encoding='utf-8')
for f in sorted((mod/'licenses').iterdir()):
    if f.is_file():license_text+='\n\n===== '+f.name+' =====\n'+f.read_text(encoding='utf-8-sig')
common={'INSTALL.txt':(mod/'INSTALL.txt').read_bytes(),'LICENSES.txt':license_text.encode('utf-8')}
source_path=dist/'P5R-FirstPersonCamera-Source.zip';source_path.write_bytes(source.getvalue())
results=[]
for kind in ('Reloaded-II',):
    path=dist/f'P5R-FirstPersonCamera-1.0.0-rc1-{kind}.zip';files=dict(common)
    files['P5RFirstPersonCamera.dll']=binary.read_bytes();files['ModConfig.json']=(mod/'ModConfig.json').read_bytes()
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in files.items():z.writestr(('osmium.p5r.firstperson/' if kind=='Reloaded-II' else '')+name,data)
    with zipfile.ZipFile(path) as z:assert z.testzip() is None
    results.append({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size})
manifest={'version':'1.0.0-rc1','status':'local candidate; controller and mouse acceptance passed; not published','binarySha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'packages':results,'source':str(source_path),'sourceSha256':hashlib.sha256(source.getvalue()).hexdigest()}
(dist/'P5R-FirstPersonCamera-1.0.0-rc1-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps(manifest,indent=2))
