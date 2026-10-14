#!/usr/bin/env python3
"""Package the native Windows standalone with the existing Inno Setup recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from private_preview import ENGINE_HASH, executable_version
from installers import version
from notices import collect

ROOT = Path(__file__).resolve().parents[1]
ENGINE_FILES = ('Contents/x86_64-win/Vel Engine.vst3',
                'Contents/Resources/moduleinfo.json', 'Plugin.ico', 'desktop.ini')


def stage(build, destination, release=None, encoder=None):
    build, destination = build.resolve(), destination.resolve()
    if destination.exists():
        raise RuntimeError('Choose a new stage directory; existing files are never deleted')
    if destination == ROOT or destination in ROOT.parents or destination == build:
        raise RuntimeError('Unsafe staging destination')
    if 'GRID_USE_VEL_ENGINE:BOOL=ON' not in (build/'CMakeCache.txt').read_text(encoding='utf-8'):
        raise RuntimeError('This installer requires a Vel Engine build')
    current = build/'GridARVxApp_artefacts'/('Release-'+version())
    release = release.resolve() if release else current if current.is_dir() else build/'GridARVxApp_artefacts/Release'
    app = release/'GRID-ARVx.exe'
    if not app.is_file() or executable_version(app) != version():
        raise RuntimeError('Missing or stale standalone executable; rebuild the current version')
    bundle = release/'Engine/Vel Engine.vst3'
    for relative in ENGINE_FILES:
        if not (bundle/relative).is_file():
            raise RuntimeError('Missing engine resource: '+relative)
    if hashlib.sha256((bundle/ENGINE_FILES[0]).read_bytes()).hexdigest() != ENGINE_HASH:
        raise RuntimeError('Engine fingerprint mismatch')
    if encoder:
        encoder = encoder.resolve()
        if not (encoder/'ffmpeg.exe').is_file() or not (encoder/'Source/BUILD.json').is_file():
            raise RuntimeError('Encoder requires its tested executable and corresponding source package')
        provenance = json.loads((encoder/'Source/BUILD.json').read_text(encoding='utf-8'))
        if hashlib.sha256((encoder/'ffmpeg.exe').read_bytes()).hexdigest() != provenance.get('binary_sha256'):
            raise RuntimeError('Encoder fingerprint mismatch')
    standalone = destination/'Standalone'
    standalone.mkdir(parents=True)
    shutil.copy2(app, standalone/app.name)
    for relative in ENGINE_FILES:
        target=standalone/'Engine/Vel Engine.vst3'/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bundle/relative, target)
    for name in ['README.md','LICENSE-NOTICE.md']:
        shutil.copy2(ROOT/name,destination/name)
    shutil.copytree(ROOT/'docs',destination/'docs')
    collect(build,destination)
    if encoder:shutil.copytree(encoder,destination/'Tools')
    manifest={'product':'GRID-ARVx','version':version(),'platform':'Windows x64',
              'backend':'Vel Engine','components':['Standalone'],
              'recordingEncoder':'bundled' if encoder else 'Select an existing FFmpeg executable in Settings',
              'distribution':'Public distribution authorized under creator clearance', 'files':{}}
    for path in sorted(destination.rglob('*')):
        if path.is_file():
            manifest['files'][path.relative_to(destination).as_posix()]={'bytes':path.stat().st_size,
                'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    (destination/'release-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    return destination


def compile_installer(stage_directory, output, compiler=None):
    stage_directory, output = stage_directory.resolve(), output.resolve()
    if output.exists():raise RuntimeError('Installer already exists; choose another output filename')
    if not (stage_directory/'Standalone/GRID-ARVx.exe').is_file():raise RuntimeError('Missing standalone stage')
    compiler = compiler or shutil.which('ISCC')
    if not compiler:
        for folder in ['C:/Program Files (x86)/Inno Setup 6', 'C:/Program Files/Inno Setup 6']:
            if (Path(folder)/'ISCC.exe').is_file():compiler=str(Path(folder)/'ISCC.exe');break
    if not compiler:raise RuntimeError('Inno Setup 6 is required; provide --iscc if it is not on PATH')
    output.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run([str(compiler),'/Qp',f'/DStageDir={stage_directory}',f'/DAppVersion={version()}',
        '/DStandaloneOnly=1',f'/DBundleEncoder={int((stage_directory/"Tools").is_dir())}',
        f'/O{output.parent}',f'/F{output.stem}',str(ROOT/'installer/windows/GRID-ARVx.iss')],check=True)
    if not output.is_file():raise RuntimeError('Inno Setup did not produce the requested installer')
    return output


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,default=ROOT/'build')
    parser.add_argument('--stage',type=Path,default=ROOT/'dist/windows-native-stage')
    parser.add_argument('--release-dir',type=Path)
    parser.add_argument('--encoder',type=Path,help='Optional scripts/encoder.py output with corresponding source')
    parser.add_argument('--output',type=Path,default=ROOT/f'dist/GRID-ARVx-{version()}-Windows-x64-Setup.exe')
    parser.add_argument('--iscc',type=Path)
    parser.add_argument('--stage-only',action='store_true')
    args=parser.parse_args()
    target=stage(args.build,args.stage,args.release_dir,args.encoder)
    print('Staged:',target)
    if not args.stage_only:print('Installer:',compile_installer(target,args.output,args.iscc))
