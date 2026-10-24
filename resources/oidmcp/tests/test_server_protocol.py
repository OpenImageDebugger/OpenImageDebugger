import asyncio
import json

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


def test_raw_socket_error_reaches_the_client(live_endpoint, monkeypatch):
    def refuse(self, info):
        raise ConnectionRefusedError(61, 'Connection refused')
    monkeypatch.setattr(server.SessionManager, '_connect', refuse)
    assert 'Connection refused' in _call('list_buffers', {}).content[0].text


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


def test_clients_see_the_installed_server_version():
    version = server._server_version()
    if not version:
        pytest.skip('oid-mcp is not installed as a distribution')

    async def run():
        async with Client(server.mcp, mode='legacy') as client:
            return client.server_info.version
    assert asyncio.run(run()) == version
