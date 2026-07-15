"""The vendored framing must stay byte-identical to the endpoint's copy."""

from pathlib import Path


def test_vendored_wireframe_matches_oidscripts():
    root = Path(__file__).resolve().parents[3]  # repo root
    endpoint = (root / 'resources' / 'oidscripts'
                / 'wireframe.py').read_bytes()
    vendored = (root / 'resources' / 'oidmcp' / 'oidmcp'
                / '_wireframe.py').read_bytes()
    assert vendored == endpoint, (
        'oidmcp/_wireframe.py drifted from oidscripts/wireframe.py; '
        'they must stay byte-identical')
