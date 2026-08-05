"""server.json is publish-time metadata nothing else reads, so it drifts silently.

Both fields guarded here have already broken a release: a lowercased namespace
made the MCP registry reject the listing with a 403, and the version appears in
three files that must agree or the registry advertises a PyPI version that was
never uploaded.
"""

import json
import tomllib
from pathlib import Path

OIDMCP = Path(__file__).resolve().parents[1]

# The registry derives publish permission from the GitHub OIDC claim, which
# carries the organisation's canonical casing. A namespace that differs by case
# is refused.
GITHUB_NAMESPACE = 'io.github.OpenImageDebugger'


def _manifest():
    return json.loads((OIDMCP / 'server.json').read_text())


def _pyproject_version():
    with open(OIDMCP / 'pyproject.toml', 'rb') as handle:
        return tomllib.load(handle)['project']['version']


def test_namespace_matches_the_github_organisation():
    assert _manifest()['name'] == f'{GITHUB_NAMESPACE}/oid-mcp'


def test_manifest_version_matches_pyproject():
    assert _manifest()['version'] == _pyproject_version()


def test_pypi_package_version_matches_pyproject():
    packages = _manifest()['packages']
    assert len(packages) == 1
    assert packages[0]['registryType'] == 'pypi'
    assert packages[0]['identifier'] == 'oid-mcp'
    assert packages[0]['version'] == _pyproject_version()
