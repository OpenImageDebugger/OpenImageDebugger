import asyncio
import json
import os
import subprocess
import sys

import pytest
from mcp import Client

from oidmcp import server
from oidmcp.protocol import ControlError


def _call(name, args, mode='legacy'):
    async def run():
        async with Client(server.mcp, mode=mode) as client:
            return await client.call_tool(name, args)
    return asyncio.run(run())


@pytest.fixture
def throwaway_tool():
    names = []

    def register(name, raises):
        def tool():
            raise raises
        server.mcp.add_tool(tool, name=name)
        names.append(name)

    yield register
    for name in names:
        server.mcp.remove_tool(name)


@pytest.mark.parametrize('mode', ['legacy', 'auto'])
def test_hint_reaches_the_client(tmp_path, monkeypatch, mode):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'empty'))
    result = _call('view', {'symbol': 'img'}, mode=mode)
    assert result.is_error
    assert 'OID_AGENT=1' in result.content[0].text


@pytest.mark.parametrize('mode', ['legacy', 'auto'])
@pytest.mark.parametrize('error, text', [
    (ConnectionRefusedError(61, 'Connection refused'), 'Connection refused'),
    (TimeoutError('timed out'), 'timed out'),
])
def test_raw_socket_error_reaches_the_client(live_endpoint, monkeypatch,
                                             mode, error, text):
    def fail(self, info):
        raise error
    monkeypatch.setattr(server.SessionManager, '_connect', fail)
    result = _call('list_buffers', {}, mode=mode)
    assert result.is_error
    assert text in result.content[0].text


@pytest.mark.parametrize('mode', ['legacy', 'auto'])
def test_a_refused_overwrite_reaches_the_client(live_endpoint, dump_dir,
                                                mode):
    args = {'symbol': 'grad', 'path': 'out.npy'}
    assert not _call('dump', args, mode=mode).is_error
    result = _call('dump', args, mode=mode)
    assert result.is_error
    assert 'overwrite=true' in result.content[0].text


def test_deliberate_error_keeps_its_exact_text(live_viewer):
    live_viewer()
    assert _call('plot', {'symbol': 'grad'}).content[0].text == (
        'Error executing tool plot: '
        'the buffer is already displayed in that viewer')


def test_a_bug_stays_masked_and_keeps_its_traceback(throwaway_tool, caplog):
    throwaway_tool('t_bug', raises=IndexError('boom'))
    assert _call('t_bug', {}).content[0].text == 'Error executing tool t_bug'
    assert any(r.levelname == 'ERROR' and r.exc_info
               and isinstance(r.exc_info[1].__cause__, IndexError)
               for r in caplog.records)


@pytest.mark.parametrize('name, exc', [
    ('t_notimpl', NotImplementedError('todo')),
    ('t_deep', RecursionError('too deep')),
    ('t_unicode', UnicodeDecodeError('utf-8', b'\xff', 0, 1, 'bad')),
    ('t_json', json.JSONDecodeError('x', 'doc', 0)),
])
def test_a_bug_type_stays_masked(throwaway_tool, name, exc):
    throwaway_tool(name, raises=exc)
    assert _call(name, {}).content[0].text == f'Error executing tool {name}'


def test_friendly_never_returns_blank_and_separates_hints():
    assert server._friendly(TimeoutError()) == 'TimeoutError'
    assert server._friendly(ControlError('internal', '')) \
        .startswith('internal')
    assert server._friendly(
        ControlError('symbol_not_found', "no symbol 'img2'")
    ).startswith("no symbol 'img2'. Call list_buffers()")


def test_friendly_joins_a_sentence_to_its_hint_with_one_space():
    text = server._friendly(ControlError('symbol_not_found', 'no symbol.'))
    assert text.startswith('no symbol. Call list_buffers()')


def test_server_version_is_the_installed_distribution(monkeypatch):
    monkeypatch.setattr(server.importlib.metadata, 'version',
                        lambda name: '9.9.9' if name == 'oid-mcp' else None)
    assert server._server_version() == '9.9.9'


def test_server_version_is_empty_when_not_installed(monkeypatch):
    def missing(name):
        raise server.importlib.metadata.PackageNotFoundError(name)
    monkeypatch.setattr(server.importlib.metadata, 'version', missing)
    assert server._server_version() == ''


_PRINT_SERVER_VERSION = """
import asyncio, importlib.metadata
real = importlib.metadata.version
importlib.metadata.version = (
    lambda name: '9.9.9' if name == 'oid-mcp' else real(name))
from mcp import Client
from oidmcp import server

async def run():
    async with Client(server.mcp, mode='legacy') as client:
        print(client.server_info.version)
asyncio.run(run())
"""


def test_clients_see_the_installed_server_version():
    # A child process: server.mcp reads the version once, at import.
    env = dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path))
    child = subprocess.run([sys.executable, '-c', _PRINT_SERVER_VERSION],
                           env=env, capture_output=True, text=True,
                           timeout=60)
    assert child.stdout.strip() == '9.9.9', child.stderr
