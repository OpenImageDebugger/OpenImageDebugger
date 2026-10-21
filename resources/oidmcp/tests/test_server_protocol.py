import asyncio

import pytest
from mcp import Client
from mcp.server.mcpserver.exceptions import ToolError

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

    def register(name, raises, is_async=False):
        if is_async:
            async def tool():
                raise raises
        else:
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
    assert _call('plot', {'symbol': 'grad'}).content[0].text == \
        'Error executing tool plot: the buffer is already displayed in that viewer'


def test_a_tool_error_gets_exactly_one_prefix(throwaway_tool):
    throwaway_tool('t_tool_error', raises=ToolError('use list_buffers first'))
    assert _call('t_tool_error', {}).content[0].text == \
        'Error executing tool t_tool_error: use list_buffers first'


def test_a_bug_stays_masked_and_keeps_its_traceback(throwaway_tool, caplog):
    throwaway_tool('t_bug', raises=IndexError('boom'))
    assert _call('t_bug', {}).content[0].text == 'Error executing tool t_bug'
    assert any(r.levelname == 'ERROR' and r.exc_info
               and isinstance(r.exc_info[1].__cause__, IndexError)
               for r in caplog.records)


def test_tool_registered_later_gets_readable_errors(throwaway_tool):
    throwaway_tool('t_late', raises=OSError('disk gone'), is_async=True)
    assert _call('t_late', {}).content[0].text == 'Error executing tool t_late: disk gone'
    # Guard: fails if a later 2.x stops routing tools/call through self.call_tool


def test_friendly_never_returns_blank_and_separates_hints():
    assert server._friendly(TimeoutError()) == 'TimeoutError'
    assert server._friendly(ControlError('internal', '')).startswith('internal')
    assert server._friendly(ControlError('symbol_not_found', "no symbol 'img2'")) \
        .startswith("no symbol 'img2'. Call list_buffers()")


@pytest.mark.parametrize('end', ['.', '!', '?'])
def test_friendly_does_not_double_sentence_punctuation(end):
    text = server._friendly(ControlError('symbol_not_found', f'no symbol{end}'))
    assert text.startswith(f'no symbol{end} Call list_buffers()')
