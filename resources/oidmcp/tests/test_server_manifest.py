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
# carries the organisation's canonical casing. A namespace that differs by case
# is refused.
GITHUB_NAMESPACE = 'io.github.OpenImageDebugger'


def _manifest():
    return json.loads((OIDMCP / 'server.json').read_text())


def _pyproject_version():
    # tomllib is 3.11+ and the package supports 3.10, so the version
    # comparisons sit out on the older interpreter rather than failing
    # collection for the whole suite. The namespace check needs no parser and
    # runs everywhere, which is the half that has actually broken a release.
    tomllib = pytest.importorskip('tomllib')
    with open(OIDMCP / 'pyproject.toml', 'rb') as handle:
        return tomllib.load(handle)['project']['version']


def test_namespace_matches_the_github_organisation():
    assert _manifest()['name'] == f'{GITHUB_NAMESPACE}/oid-mcp'


def test_readme_carries_the_ownership_marker():
    # The registry proves we own the PyPI package by finding this marker in the
    # package description, and the name after it must match server.json exactly.
    # The marker has to be followed by a boundary, so it stays on its own line
    # inside a comment rather than being glued to trailing punctuation.
    readme = (OIDMCP / 'README.md').read_text()
    assert f'<!-- mcp-name: {_manifest()["name"]} -->' in readme


def test_readme_reaches_pypi_as_the_long_description():
    # A marker in a README that the wheel does not ship is invisible to the
    # registry, which is exactly how the 0.3.3 listing failed.
    pyproject = (OIDMCP / 'pyproject.toml').read_text()
    assert 'readme = "README.md"' in pyproject


def test_manifest_links_back_to_the_repository():
    # Without this block the registry listing has no route to the source, which
    # is how it read for the first four releases. The numeric id is what lets a
    # registry tell this repository from one that reused its name, so it has to
    # be the id GitHub reports (`gh api repos/<owner>/<repo> --jq .id`), and the
    # subfolder points at the server rather than the repository root.
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
