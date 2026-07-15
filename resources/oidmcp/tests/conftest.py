import sys
from pathlib import Path

# Make `import oidscripts.agentendpoint` work: the endpoint lives in the
# sibling debugger-scripts tree, not in this package. Append (lowest
# precedence) to avoid shadowing similarly named modules on the path.
RESOURCES_DIR = str(Path(__file__).resolve().parents[2])
if RESOURCES_DIR not in sys.path:
    sys.path.append(RESOURCES_DIR)

from oidscripts.debuggers.interfaces import raise_if_too_large


class FakeBridge:
    """Synchronous stand-in for GdbBridge/LldbBridge in tests."""

    def __init__(self, symbols=(), buffers=None, backend='gdb',
                 error=None, dead=False):
        self.symbols = list(symbols)
        self.buffers = dict(buffers or {})
        self.backend = backend
        self.error = error          # raised by get_buffer_metadata if set
        self.dead = dead            # queued requests are never executed
        self.pending = []
        self.fetch_count = 0

    def queue_request(self, callable_request):
        if self.dead:
            self.pending.append(callable_request)
        else:
            callable_request()

    def get_backend_name(self):
        return self.backend

    def get_available_symbols(self):
        return list(self.symbols)

    def get_buffer_metadata(self, variable, max_bytes=None):
        self.fetch_count += 1
        if self.error is not None:
            raise self.error
        if variable not in self.buffers:
            return None
        meta = dict(self.buffers[variable])
        raise_if_too_large(len(meta['pointer']), max_bytes)
        return meta


class FakeWindow:
    def __init__(self, ready=False, plot_result=1):
        self._ready = ready
        self._plot_result = plot_result
        self.plotted = []

    def is_ready(self):
        return self._ready

    def plot_variable(self, name):
        self.plotted.append(name)
        return self._plot_result


def make_meta(width, height, channels=1, type_value=5, row_stride=None,
              pixel_layout='rgba', transpose_buffer=False, raw=None):
    """Build a bridge-style metadata dict with 'pointer' holding bytes."""
    itemsize = {0: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 8}[type_value]
    stride = row_stride if row_stride is not None else width
    if raw is None:
        raw = bytes(height * stride * channels * itemsize)
    return {
        'display_name': 'fake',
        'pointer': raw,
        'width': width,
        'height': height,
        'channels': channels,
        'type': type_value,
        'row_stride': stride,
        'pixel_layout': pixel_layout,
        'transpose_buffer': transpose_buffer,
        'variable_name': 'fake',
    }


import numpy as np
import pytest

from oidscripts import agentendpoint


def gradient_meta():
    """4x3 float32 single-channel gradient buffer, values row*10+col."""
    arr = (np.arange(3)[:, None] * 10 + np.arange(4)[None, :]) \
        .astype(np.float32)
    return make_meta(4, 3, channels=1, type_value=5, raw=arr.tobytes())


@pytest.fixture
def live_endpoint(tmp_path, monkeypatch):
    """Real agentendpoint server backed by a FakeBridge."""
    from oidmcp import discovery

    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'agent'))
    bridge = FakeBridge(symbols=['img', 'grad'],
                        buffers={'grad': gradient_meta()})
    agentendpoint.start(bridge, FakeWindow(ready=True))
    sessions = discovery.live_sessions()
    assert len(sessions) == 1
    yield sessions[0], bridge
    agentendpoint.shutdown()
