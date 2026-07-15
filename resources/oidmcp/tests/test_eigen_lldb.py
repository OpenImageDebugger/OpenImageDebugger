"""
Confirms the Eigen inspector works with LLDB-style symbols, where the
type is a plain name string (wrapped as TemplateTypeName) rather than a
GDB rich type. Exercises the real EigenXX.get_buffer_metadata through a
fake symbol/bridge, covering the two LLDB-specific fixes: reading
template arguments from the canonical name, and taking the dynamic
buffer pointer from m_storage.m_data (not the struct address).
"""

from oidscripts import symbols
from oidscripts.debuggers.template_args import TemplateTypeName
from oidscripts.oidtypes.eigen3 import EigenXX


class FakeInt:
    """A member whose int() yields a fixed value (like SymbolWrapper)."""

    def __init__(self, value):
        self._value = value

    def __int__(self):
        return self._value


class FakeSymbol:
    """Minimal stand-in for an LLDB SymbolWrapper node."""

    def __init__(self, type_name='', canonical=None, children=None):
        self.type = TemplateTypeName(type_name, canonical)
        self._children = children or {}

    def __getitem__(self, key):
        return self._children[key]


class FakeBridge:
    def __init__(self):
        self.casts = []

    def get_casted_pointer(self, typename, obj):
        self.casts.append((typename, obj))
        return ('casted', typename, obj)


def test_dynamic_matrixxf_metadata_under_lldb():
    # Eigen::MatrixXf (dynamic) stopped at 8 rows x 16 cols, column-major.
    m_data = FakeSymbol('float *')
    storage = FakeSymbol(children={
        'm_rows': FakeInt(8),
        'm_cols': FakeInt(16),
        'm_data': m_data,
    })
    picked = FakeSymbol(
        'Eigen::MatrixXf',
        canonical='Eigen::Matrix<float, -1, -1, 0, -1, -1>',
        children={'m_storage': storage})
    bridge = FakeBridge()

    meta = EigenXX().get_buffer_metadata('gradient', picked, bridge)

    assert meta['type'] == symbols.OID_TYPES_FLOAT32
    assert meta['channels'] == 1
    # Column-major (Options bit 0 == 0) -> transposed; dims swapped.
    assert meta['transpose_buffer'] is True
    assert (meta['width'], meta['height']) == (8, 16)
    # Plain matrix has no Stride template arg -> dense row stride == width.
    assert meta['row_stride'] == 8
    # The buffer pointer must come from m_storage.m_data, not the struct.
    assert bridge.casts[-1] == ('float', m_data)
    assert meta['pointer'] == ('casted', 'float', m_data)


def test_static_matrix_metadata_under_lldb():
    # Fixed-size Eigen::Matrix<float, 8, 16> (row-major here: Options bit 0
    # set) to exercise the non-dynamic dims and the static pointer branch.
    array = FakeSymbol('float[128]')
    m_data = FakeSymbol(children={'array': array})
    storage = FakeSymbol(children={'m_data': m_data})
    picked = FakeSymbol(
        'Eigen::Matrix<float, 8, 16, 1, 8, 16>',
        canonical='Eigen::Matrix<float, 8, 16, 1, 8, 16>',
        children={'m_storage': storage})
    bridge = FakeBridge()

    meta = EigenXX().get_buffer_metadata('m', picked, bridge)

    assert meta['type'] == symbols.OID_TYPES_FLOAT32
    # Options bit 0 == 1 -> row-major -> not transposed.
    assert meta['transpose_buffer'] is False
    assert (meta['width'], meta['height']) == (16, 8)
    assert meta['row_stride'] == 16
    assert bridge.casts[-1] == ('float', array)
