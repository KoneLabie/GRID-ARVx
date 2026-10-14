#!/usr/bin/env python3
"""Create the GitHub source ZIP from an explicit source/engine allowlist."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
from installers import version
from private_preview import ENGINE_HASH
from windows_release import ENGINE_FILES

ROOT=Path(__file__).resolve().parents[1]
ENGINE_PREFIX='vendor/VelEngine/Windows/x64/Vel Engine.vst3/'


def create(output):
    output=output.resolve()
    if output.exists():raise RuntimeError('Archive already exists; choose another output filename')
    paths=[]
    for line in (ROOT/'PACKAGE-MANIFEST.sha256').read_text(encoding='utf-8').splitlines():
        digest,name=line.split('  ',1)
        path=ROOT/name
        if Path(name).is_absolute() or '..' in Path(name).parts or path.is_symlink() or ROOT.resolve() not in path.resolve().parents:
            raise RuntimeError('Unsafe source path: '+name)
        if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise RuntimeError('Source manifest is stale: '+name)
        paths.append(name)
    paths+=['PACKAGE-MANIFEST.sha256']+[ENGINE_PREFIX+relative for relative in ENGINE_FILES]
    if hashlib.sha256((ROOT/ENGINE_PREFIX/ENGINE_FILES[0]).read_bytes()).hexdigest()!=ENGINE_HASH:
        raise RuntimeError('Engine fingerprint mismatch')
    manifest={'product':'GRID-ARVx','version':version(),'type':'GitHub source package',
              'platform':'Windows x64; Linux/macOS deferred',
              'distribution':'Public distribution authorized under creator clearance',
              'files':{name:{'sha256':hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),
                             'bytes':(ROOT/name).stat().st_size} for name in sorted(set(paths))}}
    prefix='GRID-ARVx-'+version()
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(set(paths)):archive.write(ROOT/name,prefix+'/'+name)
        archive.writestr(prefix+'/SOURCE-ARCHIVE.json',json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(output) as archive:
        if archive.testzip():raise RuntimeError('Archive CRC verification failed')
        for name,item in manifest['files'].items():
            if hashlib.sha256(archive.read(prefix+'/'+name)).hexdigest()!=item['sha256']:
                raise RuntimeError('Archived file hash mismatch: '+name)
    return output


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    print('Verified private source ZIP:',create(args.output))
