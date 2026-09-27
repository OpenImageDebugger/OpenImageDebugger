"""The wheel must be importable without the repo's resources/ tree.

Simulates an installed site-packages: copy oidmcp and oidscripts side by
side into a temp dir and import oidmcp._wireframe from there. Guards both
the _wireframe path acceptance and (indirectly) the wheel's need to ship
oidscripts as a top-level package.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

RESOURCES = Path(__file__).resolve().parents[2]


def test_wireframe_imports_from_installed_layout(tmp_path):
    site = tmp_path / 'site-packages'
    shutil.copytree(RESOURCES / 'oidmcp' / 'oidmcp', site / 'oidmcp')
    shutil.copytree(RESOURCES / 'oidscripts', site / 'oidscripts')

    # A fresh interpreter whose sys.path starts at the fake site-packages;
    # cwd is neutral so the repo layout cannot mask a failure.
    code = ('import oidmcp._wireframe as w; '
            'print(w.MAX_FRAME_BYTES)')
    result = subprocess.run(
        [sys.executable, '-c', code],
        cwd=tmp_path,
        env={'PYTHONPATH': str(site),
             'PATH': os.environ.get('PATH', ''),
             'HOME': os.environ.get('HOME', str(tmp_path))},
        capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == str(1 << 20)


def test_wireframe_still_imports_from_repo_layout():
    import oidmcp._wireframe as w
    assert w.MAX_FRAME_BYTES == 1 << 20


def test_wireframe_imports_from_purelib_layout(tmp_path):
    # oidmcp lives at pkgs/oidmcp, so neither the repo candidate
    # (parents[2]=tmp_path) nor the site one (parents[1]=pkgs) holds
    # oidscripts; only _PURELIB_DIR, patched before the import, can serve it.
    pkgs = tmp_path / 'pkgs'
    purelib = tmp_path / 'purelib'
    shutil.copytree(RESOURCES / 'oidmcp' / 'oidmcp', pkgs / 'oidmcp')
    shutil.copytree(RESOURCES / 'oidscripts', purelib / 'oidscripts')

    code = (
        'import sysconfig; '
        'sysconfig.get_path = lambda *a, **k: {purelib!r}; '
        'import oidmcp._wireframe as w; '
        'print(w.MAX_FRAME_BYTES)'
    ).format(purelib=str(purelib))
    result = subprocess.run(
        [sys.executable, '-c', code],
        cwd=tmp_path,
        env={'PYTHONPATH': os.pathsep.join([str(pkgs), str(purelib)]),
             'PATH': os.environ.get('PATH', ''),
             'HOME': os.environ.get('HOME', str(tmp_path))},
        capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == str(1 << 20)
