"""server.json is publish-time metadata nothing else reads, so it drifts silently.

Both fields guarded here have already broken a release: a lowercased namespace
made the MCP registry reject the listing with a 403, and the version appears in
three files that must agree or the registry advertises a PyPI version that was
never uploaded.
"""

import json
from pathlib import Path

import pytest

OIDMCP = Path(__file__).resolve().parents[1]

# The registry derives publish permission from the GitHub OIDC claim, which
# carries the org's canonical casing; a namespace differing by case is refused.
GITHUB_NAMESPACE = 'io.github.OpenImageDebugger'


def _manifest():
    return json.loads((OIDMCP / 'server.json').read_text())


def _pyproject_version():
    # tomllib is 3.11+ and the package supports 3.10, so version comparisons
    # sit out there rather than failing collection for the whole suite.
    tomllib = pytest.importorskip('tomllib')
    with open(OIDMCP / 'pyproject.toml', 'rb') as handle:
        return tomllib.load(handle)['project']['version']


def test_namespace_matches_the_github_organisation():
    assert _manifest()['name'] == f'{GITHUB_NAMESPACE}/oid-mcp'


def test_readme_carries_the_ownership_marker():
    # The registry proves package ownership by this marker in the description;
    # it needs a trailing boundary, so it stays alone on its own line.
    readme = (OIDMCP / 'README.md').read_text()
    assert f'<!-- mcp-name: {_manifest()["name"]} -->' in readme


def test_readme_reaches_pypi_as_the_long_description():
    # A marker in a README that the wheel does not ship is invisible to the
    # registry, which is exactly how the 0.3.3 listing failed.
    pyproject = (OIDMCP / 'pyproject.toml').read_text()
    assert 'readme = "README.md"' in pyproject


def test_manifest_links_back_to_the_repository():
    # The numeric id is what tells this repository from one that reused its
    # name, so it must be the id `gh api repos/<owner>/<repo> --jq .id` gives.
    repository = _manifest()['repository']
    assert repository['url'] == (
        'https://github.com/OpenImageDebugger/OpenImageDebugger'
    )
    assert repository['source'] == 'github'
    assert repository['id'] == '205714671'
    assert repository['subfolder'] == 'resources/oidmcp'


def test_manifest_version_matches_pyproject():
    assert _manifest()['version'] == _pyproject_version()


def test_pypi_package_version_matches_pyproject():
    packages = _manifest()['packages']
    assert len(packages) == 1
    assert packages[0]['registryType'] == 'pypi'
    assert packages[0]['identifier'] == 'oid-mcp'
    assert packages[0]['version'] == _pyproject_version()
