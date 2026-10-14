"""Verify native installer staging, personal-state exclusion and source ZIPs."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import windows_release as release
import source_archive as source

with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary); repo=root/'repo';build=root/'build'
    repo.mkdir();build.mkdir()
    (repo/'docs').mkdir();(repo/'docs/WINDOWS-RELEASE.md').write_text('fixture')
    for name in ['README.md','LICENSE-NOTICE.md']:(repo/name).write_text('fixture')
    (build/'CMakeCache.txt').write_text('GRID_USE_VEL_ENGINE:BOOL=ON')
    product=build/'GridARVxApp_artefacts/Release-1.2.3'
    product.mkdir(parents=True);(product/'GRID-ARVx.exe').write_bytes(b'app')
    bundle=product/'Engine/Vel Engine.vst3'
    for relative in release.ENGINE_FILES:
        path=bundle/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'engine')
    (bundle/'activation.settings').write_bytes(b'PRIVATE')
    (product/'state.bin').write_bytes(b'PRIVATE')
    digest=hashlib.sha256(b'engine').hexdigest()
    def notices(build,stage):
        folder=stage/'ThirdPartyNotices';folder.mkdir();(folder/'LICENSE.txt').write_text('fixture')
    with patch.object(release,'ROOT',repo),patch.object(release,'version',lambda:'1.2.3'),patch.object(release,'executable_version',lambda path:'1.2.3'),patch.object(release,'ENGINE_HASH',digest),patch.object(release,'collect',notices):
        stage=release.stage(build,root/'stage')
        manifest=json.loads((stage/'release-manifest.json').read_text())
        assert manifest['version']=='1.2.3' and not (stage/'Tools').exists()
        assert (stage/'Standalone/Engine/Vel Engine.vst3'/release.ENGINE_FILES[0]).read_bytes()==b'engine'
        assert not any('activation.settings' in p or 'state.bin' in p for p in manifest['files'])
        for name,item in manifest['files'].items():assert hashlib.sha256((stage/name).read_bytes()).hexdigest()==item['sha256']
        try:release.stage(build,stage)
        except RuntimeError:pass
        else:raise AssertionError('Existing stage accepted')
        with patch.object(release,'executable_version',lambda path:'0.0.1'):
            try:release.stage(build,root/'stale')
            except RuntimeError:pass
            else:raise AssertionError('Stale executable accepted')
        (bundle/release.ENGINE_FILES[0]).write_bytes(b'wrong')
        try:release.stage(build,root/'bad-engine')
        except RuntimeError:pass
        else:raise AssertionError('Changed engine accepted')
        assert not (root/'bad-engine').exists()
        calls=[]
        def compiler(args,check):
            calls.append(args);(root/'output.exe').write_bytes(b'installer')
        with patch.object(release.subprocess,'run',compiler):release.compile_installer(stage,root/'output.exe',Path('iscc.exe'))
        assert '/DStandaloneOnly=1' in calls[0] and '/DBundleEncoder=0' in calls[0]
    prefix=repo/source.ENGINE_PREFIX
    for relative in release.ENGINE_FILES:
        path=prefix/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'engine')
    (prefix/'license.settings').write_bytes(b'PRIVATE')
    (repo/'local.token').write_text('PRIVATE')
    (repo/'PACKAGE-MANIFEST.sha256').write_text(hashlib.sha256((repo/'README.md').read_bytes()).hexdigest()+'  README.md\n')
    with patch.object(source,'ROOT',repo),patch.object(source,'ENGINE_HASH',digest),patch.object(source,'version',lambda:'1.2.3'):
        archive=source.create(root/'source.zip')
        with zipfile.ZipFile(archive) as zipped:
            assert not any('license.settings' in p or 'local.token' in p for p in zipped.namelist())
            assert zipped.testzip() is None
        (repo/'README.md').write_text('changed')
        try:source.create(root/'stale.zip')
        except RuntimeError:pass
        else:raise AssertionError('Stale source manifest accepted')
print('PASS: native installer staging, engine/version/hash checks, Inno arguments, source archive verification and personal-state exclusion')
