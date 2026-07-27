"""Built-in declarative types: schema conformance and match anchoring.

A follow-up appends the fake-symbol dual-run (old Python inspector vs. new
JSON entry) to this module. Kept together because both are the same
guarantee: the shipped builtin_types.json reproduces the retired Python
inspectors.
"""

import json
from pathlib import Path

import jsonschema
import pytest

from oidscripts.debuggers.template_args import TemplateTypeName
from oidscripts.oidtypes import declarative

RESOURCES = Path(__file__).resolve().parents[2]
BUILTIN_TYPES = RESOURCES / 'oidscripts' / 'oidtypes' / 'builtin_types.json'
EDITOR_SCHEMA = RESOURCES / 'schemas' / 'oid-types-v1.json'

BUILTIN_NAMES = ['cv::Mat', 'CvMat', 'IplImage', 'Eigen::Matrix', 'Eigen::Map']


def test_builtin_types_validate_against_editor_schema():
    document = json.loads(BUILTIN_TYPES.read_text(encoding='utf-8'))
    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    jsonschema.validate(document, schema)


def test_builtins_register_five_named_inspectors_in_order():
    inspectors = declarative.load_builtin_inspectors()
    assert [inspector.name for inspector in inspectors] == BUILTIN_NAMES


def _first_matching_name(type_name):
    symbol = _TypeOnlySymbol(type_name)
    for inspector in declarative.load_builtin_inspectors():
        if inspector.is_symbol_observable(symbol, 'sym'):
            return inspector.name
    return None


class _TypeOnlySymbol:
    """Carries only `.type`, which is all is_symbol_observable reads."""

    def __init__(self, type_name):
        self.type = TemplateTypeName(type_name)


@pytest.mark.parametrize('type_name,expected', [
    ('cv::Mat', 'cv::Mat'),
    ('const cv::Mat', 'cv::Mat'),
    ('cv::Mat_<float>', 'cv::Mat'),
    ('cv::Mat *', 'cv::Mat'),
    ('CvMat', 'CvMat'),
    ('const CvMat *', 'CvMat'),
    ('IplImage', 'IplImage'),
    ('IplImage &', 'IplImage'),
    ('Eigen::Matrix<float, 3, 4, 1, 3, 4>', 'Eigen::Matrix'),
    ('Eigen::Map<Eigen::Matrix<float, 3, 4, 1, 3, 4>, 0, '
     'Eigen::Stride<0, 0> >', 'Eigen::Map'),
])
def test_match_hits_expected_builtin(type_name, expected):
    assert _first_matching_name(type_name) == expected


@pytest.mark.parametrize('type_name', [
    'MyCvMatWrapper',            # anchored CvMat regex must not match (diff #1)
    'CvMatHelper',              # anchored CvMat regex must not match (diff #1)
    'MyIplImageView',           # anchored IplImage regex must not match (diff #1)
    'Eigen::Quaternion<float>',    # Eigen match tightened to Matrix/Map (diff #2)
    'Eigen::Array<float, 3, 1>',   # not a Matrix/Map (diff #2)
    'std::vector<int>',
])
def test_match_misses_non_builtins(type_name):
    assert _first_matching_name(type_name) is None


import collections

from oidscripts import symbols
from oidscripts.oidtypes.declarative import EntryEvaluationError

# The retired Python inspectors are the comparison basis. Once they are
# removed the import fails and every @legacy_only test self-skips; the
# JSON path (test_new_entry_matches_golden) is then authoritative.
try:
    from oidscripts.oidtypes.opencv import Mat, CvMat, IplImage
    from oidscripts.oidtypes.eigen3 import EigenXX
    _LEGACY = {'cv::Mat': Mat, 'CvMat': CvMat, 'IplImage': IplImage,
               'Eigen::Matrix': EigenXX, 'Eigen::Map': EigenXX}
except ImportError:
    _LEGACY = None

legacy_only = pytest.mark.skipif(
    _LEGACY is None,
    reason='Python inspectors deleted; the JSON entries are authoritative')


class CInt(int):
    """An int whose `/` truncates like C integer division, so a fixture
    expression such as `step / channels / elemsize` yields the same value
    the debugger computes on integer registers (operands here are positive,
    where floor and truncation agree)."""

    def __truediv__(self, other):
        return CInt(int(self) // int(other))

    def __rtruediv__(self, other):
        return CInt(int(other) // int(self))

    def __floordiv__(self, other):
        return CInt(int(self) // int(other))


class Struct:
    """A fake debugger value read through two lenses at once.

    Legacy Python inspectors use item access, `picked_obj['field']`, plus
    `picked_obj.type` for the C++ type name. The new engine's fake bridge
    reads the SAME struct by attribute access, `(sym).field`, via eval_view()
    — where a member literally named `type` (CvMat) or `data` resolves to
    the struct member, exactly as a real debugger expression would, without
    colliding with the C++ type that `.type` exposes to the legacy lens.
    `ctype` is that C++ type string (None for anonymous inner nodes); `ptr`
    is the address `get_casted_pointer` returns for pointer nodes.
    """

    def __init__(self, fields=None, ptr=None, ctype=None):
        self._fields = dict(fields or {})
        self._ptr = ptr
        self._ctype = ctype

    def __getitem__(self, key):
        return self._fields[key]

    @property
    def type(self):
        if self._ctype is None:
            return None
        return TemplateTypeName(self._ctype)

    def __int__(self):
        if self._ptr is None:
            raise TypeError('struct %r is not a pointer node' % self._ctype)
        return self._ptr

    def eval_view(self):
        return _EvalView(self)


class _EvalView:
    """Attribute-access view of a Struct for the fake bridge's eval: `(sym).f`
    reads member `f`, INCLUDING a member named `type` or `data`."""

    def __init__(self, struct):
        object.__setattr__(self, '_struct', struct)

    def __getattr__(self, key):
        try:
            value = self._struct._fields[key]
        except KeyError:
            raise AttributeError(key)
        return _wrap_eval(value)

    def __int__(self):
        return int(self._struct)


def _wrap_eval(value):
    if isinstance(value, Struct):
        return _EvalView(value)
    if isinstance(value, list):
        return [_wrap_eval(item) for item in value]
    return value


class EvalFakeBridge:
    """Test-only bridge. evaluate_expression() runs the substituted text
    through Python eval over the fixture tree (a valid built-in expression is
    member access plus integer arithmetic — a shared C/Python subset, with
    CInt supplying C division). get_casted_pointer() ints the pointer node."""

    def __init__(self, symbol, obj_name):
        self._namespace = {obj_name: symbol.eval_view()}
        self.casts = []

    def evaluate_expression(self, expression):
        try:
            return eval(expression, {'__builtins__': {}}, dict(self._namespace))
        except Exception as error:
            raise RuntimeError('%r failed: %s' % (expression, error))

    def get_casted_pointer(self, typename, obj):
        self.casts.append((typename, obj))
        return int(obj)


# --- fixture builders --------------------------------------------------------

def _mat(flags, data, cols, rows, step0):
    return Struct(ctype='cv::Mat', fields={
        'flags': CInt(flags), 'data': CInt(data),
        'cols': CInt(cols), 'rows': CInt(rows),
        'step': Struct(fields={'p': [CInt(step0)]}),
    })


def _cvmat(type_flags, data_ptr, cols, rows, step):
    return Struct(ctype='CvMat', fields={
        'type': CInt(type_flags),
        'data': Struct(fields={'ptr': CInt(data_ptr)}, ptr=data_ptr),
        'cols': CInt(cols), 'rows': CInt(rows), 'step': CInt(step),
    })


def _iplimage(depth, channels, width, height, image_data, width_step):
    return Struct(ctype='IplImage', fields={
        'depth': CInt(depth), 'nChannels': CInt(channels),
        'width': CInt(width), 'height': CInt(height),
        'imageData': CInt(image_data), 'widthStep': CInt(width_step),
    })


def _eigen_static(ctype, data_ptr):
    return Struct(ctype=ctype, fields={
        'm_storage': Struct(fields={
            'm_data': Struct(fields={'array': CInt(data_ptr)}),
        }),
    })


def _eigen_dynamic(ctype, rows, cols, data_ptr):
    return Struct(ctype=ctype, fields={
        'm_storage': Struct(fields={
            'm_rows': CInt(rows), 'm_cols': CInt(cols),
            'm_data': Struct(fields={}, ptr=data_ptr),
        }),
    })


def _eigen_map(ctype, data_ptr):
    return Struct(ctype=ctype, fields={'m_data': CInt(data_ptr)})


T = symbols
Case = collections.namedtuple('Case', 'id symbol obj_name legacy_key golden')

# CV type words: 0x42FF0000 is cv::Mat's magic flag bits, 0x42420000 CvMat's;
# the low bits are CV_MAKETYPE(depth, channels) = depth | ((channels-1) << 3).
_MAT_MAGIC = 0x42FF0000
_CVMAT_MAGIC = 0x42420000
_EIGEN_STATIC = 'Eigen::Matrix<float, 3, 4, 1, 3, 4>'
_EIGEN_DYNAMIC = 'Eigen::Matrix<double, -1, -1, 0, -1, -1>'
_EIGEN_MAP = ('Eigen::Map<Eigen::Matrix<float, 3, 4, 1, 3, 4>, 0, '
              'Eigen::Stride<0, 0> >')

CLEAN_CASES = [
    Case('mat_8uc3',
         _mat(_MAT_MAGIC | 16, 4096, 640, 480, 1920), 'mat', 'cv::Mat',
         {'display_name': 'mat (cv::Mat)', 'pointer': 4096,
          'width': 640, 'height': 480, 'channels': 3,
          'type': T.OID_TYPES_UINT8, 'row_stride': 640,
          'pixel_layout': 'bgra', 'transpose_buffer': False}),
    Case('mat_32fc1',
         _mat(_MAT_MAGIC | 5, 4096, 320, 200, 1280), 'mat', 'cv::Mat',
         {'display_name': 'mat (cv::Mat)', 'pointer': 4096,
          'width': 320, 'height': 200, 'channels': 1,
          'type': T.OID_TYPES_FLOAT32, 'row_stride': 320,
          'pixel_layout': 'rgba', 'transpose_buffer': False}),
    Case('cvmat_16uc1',
         _cvmat(_CVMAT_MAGIC | 2, 8192, 100, 50, 200), 'cvmat', 'CvMat',
         {'display_name': 'cvmat (CvMat)', 'pointer': 8192,
          'width': 100, 'height': 50, 'channels': 1,
          'type': T.OID_TYPES_UINT16, 'row_stride': 100,
          'pixel_layout': 'rgba', 'transpose_buffer': False}),
    Case('iplimage_8uc3',
         _iplimage(8, 3, 64, 48, 12288, 192), 'ipl', 'IplImage',
         {'display_name': 'ipl (IplImage)', 'pointer': 12288,
          'width': 64, 'height': 48, 'channels': 3,
          'type': T.OID_TYPES_UINT8, 'row_stride': 64,
          'pixel_layout': 'bgra', 'transpose_buffer': False}),
    Case('eigen_static',
         _eigen_static(_EIGEN_STATIC, 16384), 'm', 'Eigen::Matrix',
         {'display_name': 'm (%s)' % _EIGEN_STATIC, 'pointer': 16384,
          'width': 4, 'height': 3, 'channels': 1,
          'type': T.OID_TYPES_FLOAT32, 'row_stride': 4,
          'pixel_layout': 'rgba', 'transpose_buffer': False}),
    Case('eigen_dynamic',
         _eigen_dynamic(_EIGEN_DYNAMIC, 5, 7, 16384), 'm', 'Eigen::Matrix',
         {'display_name': 'm (%s)' % _EIGEN_DYNAMIC, 'pointer': 16384,
          'width': 5, 'height': 7, 'channels': 1,
          'type': T.OID_TYPES_FLOAT64, 'row_stride': 5,
          'pixel_layout': 'rgba', 'transpose_buffer': True}),
    Case('eigen_map',
         _eigen_map(_EIGEN_MAP, 20480), 'm', 'Eigen::Map',
         {'display_name': 'm (%s)' % _EIGEN_STATIC, 'pointer': 20480,
          'width': 4, 'height': 3, 'channels': 1,
          'type': T.OID_TYPES_FLOAT32, 'row_stride': 4,
          'pixel_layout': 'rgba', 'transpose_buffer': False}),
]


def _match_builtin(symbol, obj_name):
    matches = [insp for insp in declarative.load_builtin_inspectors()
               if insp.is_symbol_observable(symbol, obj_name)]
    assert len(matches) == 1, (
        'expected exactly one builtin to match %r, got %r'
        % (symbol.type, [m.name for m in matches]))
    return matches[0]


def _new_metadata(symbol, obj_name):
    inspector = _match_builtin(symbol, obj_name)
    bridge = EvalFakeBridge(symbol, obj_name)
    return inspector.get_buffer_metadata(obj_name, symbol, bridge)


def _legacy_metadata(legacy_key, symbol, obj_name):
    bridge = EvalFakeBridge(symbol, obj_name)
    return _LEGACY[legacy_key]().get_buffer_metadata(obj_name, symbol, bridge)


@pytest.mark.parametrize('case', CLEAN_CASES, ids=[c.id for c in CLEAN_CASES])
def test_new_entry_matches_golden(case):
    assert _new_metadata(case.symbol, case.obj_name) == case.golden


@legacy_only
@pytest.mark.parametrize('case', CLEAN_CASES, ids=[c.id for c in CLEAN_CASES])
def test_legacy_inspector_matches_golden(case):
    assert _legacy_metadata(case.legacy_key, case.symbol,
                            case.obj_name) == case.golden


# Blessed diffs: the JSON path improves on the Python inspectors.
# Diffs #1 (anchored CvMat/IplImage regexes) and #2 (Eigen tightened to
# Matrix/Map) are covered by test_match_misses_non_builtins above.
# Diff #6 is the general rule "old raised / new succeeds is an improvement",
# with no dedicated case. The three metadata-affecting diffs are pinned here.

def test_diff5_null_mat_now_errors_but_legacy_returned_zero():
    # #5: null buffers now error uniformly; the Python Mat had no null check.
    symbol = _mat(_MAT_MAGIC | 16, 0, 640, 480, 1920)
    with pytest.raises(EntryEvaluationError) as excinfo:
        _new_metadata(symbol, 'mat')
    assert excinfo.value.field == 'pointer'


@legacy_only
def test_diff5_legacy_mat_passes_null_pointer_through():
    symbol = _mat(_MAT_MAGIC | 16, 0, 640, 480, 1920)
    assert _legacy_metadata('cv::Mat', symbol, 'mat')['pointer'] == 0


def test_diff4_cv_8s_rejected_by_new_entry():
    # #4: CV_8S (OID code 1) has no OID equivalent; the new dtype rule rejects
    # it. flags & 7 == 1 for CV_8SC1.
    symbol = _mat(_MAT_MAGIC | 1, 4096, 640, 480, 640)
    with pytest.raises(EntryEvaluationError) as excinfo:
        _new_metadata(symbol, 'mat')
    assert excinfo.value.field == 'dtype'


@legacy_only
def test_diff4_legacy_mat_returns_invalid_code_one():
    symbol = _mat(_MAT_MAGIC | 1, 4096, 640, 480, 640)
    assert _legacy_metadata('cv::Mat', symbol, 'mat')['type'] == 1


# IPL_DEPTH_16S = 0x80000010; as an unsigned 32-bit word that is 2147483664.
_IPL_16S_GOLDEN = {'display_name': 'ipl (IplImage)', 'pointer': 12288,
                   'width': 30, 'height': 20, 'channels': 1,
                   'type': T.OID_TYPES_INT16, 'row_stride': 30,
                   'pixel_layout': 'rgba', 'transpose_buffer': False}


def test_diff3_signed_iplimage_new_stride_is_correct():
    # #3: for signed depths the Python inspector's `widthStep / depth * 8`
    # divides by the huge signed-depth word (row_stride collapses to 0); the
    # JSON divides by elemsize and gets the real stride.
    symbol = _iplimage(0x80000010, 1, 30, 20, 12288, 60)
    assert _new_metadata(symbol, 'ipl') == _IPL_16S_GOLDEN


@legacy_only
def test_diff3_legacy_signed_iplimage_stride_collapses_to_zero():
    symbol = _iplimage(0x80000010, 1, 30, 20, 12288, 60)
    metadata = _legacy_metadata('IplImage', symbol, 'ipl')
    assert metadata['type'] == T.OID_TYPES_INT16   # dtype was already correct
    assert metadata['row_stride'] == 0            # the bug the JSON path fixes


def test_diff7_single_channel_entries_declare_an_r_first_layout():
    # #7: pixel_layout names the channel ORDER of multi-channel data, and its
    # first character also selects which channel the viewer samples. A
    # single-channel buffer is uploaded as GL_RED, where only .r carries data,
    # so a layout starting with 'g'/'b'/'a' makes the fragment shader sample a
    # constant 0 and the whole buffer renders black. The Eigen entries declared
    # 'bgra' unconditionally even though Eigen matrices are always
    # single-channel; the OpenCV entries already conditioned on {channels}.
    for case in CLEAN_CASES:
        metadata = _new_metadata(case.symbol, case.obj_name)
        if metadata['channels'] == 1:
            assert metadata['pixel_layout'][0] == 'r', (
                '%s is single-channel but declares layout %r'
                % (case.id, metadata['pixel_layout']))


@legacy_only
def test_diff7_legacy_eigen_declared_bgra_for_single_channel():
    metadata = _legacy_metadata('Eigen::Matrix',
                                _eigen_static(_EIGEN_STATIC, 16384), 'm')
    assert metadata['channels'] == 1
    assert metadata['pixel_layout'] == 'bgra'  # the bug the JSON path fixes


# --- Editor schema tightness: schema-valid must imply loader-loadable ------
#
# The editor-facing JSON Schema is meant to be at least as strict as the
# loader's own structural validation, so that any file which validates
# against the schema is guaranteed to load. Each fixture below is a shape
# the loader rejects; the schema must reject it too, or the two contracts
# would disagree.

def _minimal_entry(**overrides):
    entry = {
        'match': '^X$',
        'pointer': '{sym}.data',
        'width': '{sym}.w',
        'height': '{sym}.h',
        'dtype': 'float32',
    }
    entry.update(overrides)
    return entry


SCHEMA_REJECTED_FIXTURES = {
    'min_wrapper_float_min':
        _minimal_entry(width={'first_valid': [
            {'expr': '{sym}.w', 'min': 1.5}]}),
    'min_wrapper_missing_min':
        _minimal_entry(width={'first_valid': [{'expr': '{sym}.w'}]}),
    'min_wrapper_extra_key':
        _minimal_entry(width={'first_valid': [
            {'expr': '{sym}.w', 'min': 1, 'extra': 2}]}),
    'min_wrapper_on_pointer':
        _minimal_entry(pointer={'first_valid': [
            {'expr': '{sym}.data', 'min': 1}, '{sym}.data']}),
    'pixel_layout_not_four_rgba_chars':
        _minimal_entry(pixel_layout='xyzw'),
    'pixel_layout_given_number':
        _minimal_entry(pixel_layout=5),
    'map_dtype_float_value':
        _minimal_entry(dtype={'expr': '{sym}.t', 'map': {'8': 5.5}}),
    'map_dtype_bool_value':
        _minimal_entry(dtype={'expr': '{sym}.t', 'map': {'8': True}}),
    'map_pointer_numeric_value':
        _minimal_entry(pointer={'expr': '{sym}.t', 'map': {'8': 7}}),
    'map_empty':
        _minimal_entry(width={'expr': '{sym}.w', 'map': {}}),
    'map_value_non_node_dict':
        _minimal_entry(height={'expr': '{sym}.h',
                               'map': {'8': {'bogus': 1}}}),
    'entry_unknown_key':
        _minimal_entry(widht='{sym}.w'),
    'entry_unknown_key_alongside_valid':
        _minimal_entry(channels='{sym}.c', bogus=1),
    'description_not_a_string':
        _minimal_entry(description=5),
    'description_is_none':
        _minimal_entry(description=None),
}


@pytest.mark.parametrize('entry', SCHEMA_REJECTED_FIXTURES.values(),
                         ids=list(SCHEMA_REJECTED_FIXTURES))
def test_loader_rejected_shapes_are_also_schema_invalid(entry, tmp_path):
    # The loader rejects the entry structurally...
    assert declarative._validate_entry(entry)

    # ...and end to end: a types file containing only this entry loads no
    # inspectors.
    types_file = tmp_path / 'types.json'
    types_file.write_text(
        json.dumps({'version': 1, 'types': [entry]}), encoding='utf-8')
    assert declarative._load_types_file(str(types_file)) == []

    # The editor schema must reject the same shape, or a file that
    # validates in an editor could still fail to load.
    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    with pytest.raises(jsonschema.ValidationError):
        jsonschema.validate({'version': 1, 'types': [entry]}, schema)


# --- Editor schema tightness, converse direction: loader-loadable must ----
# also be schema-valid -------------------------------------------------------
#
# The other half of the same guarantee: a shape the loader accepts (a
# first_valid candidate that is itself a nested if/then/else node, or a
# first_valid literal of a type the field's number policy allows) must
# also validate against the editor schema, or a file that loads correctly
# would be flagged as broken in an editor.

LOADER_ACCEPTED_FIXTURES = {
    'first_valid_nested_if_branch':
        _minimal_entry(width={'first_valid': [
            '{sym}.w',
            {'if': '{sym}.f', 'then': 1, 'else': 2}]}),
    'first_valid_boolean_literal':
        _minimal_entry(transpose={'first_valid': [True]}),
}


@pytest.mark.parametrize('entry', LOADER_ACCEPTED_FIXTURES.values(),
                         ids=list(LOADER_ACCEPTED_FIXTURES))
def test_loader_accepted_shapes_are_also_schema_valid(entry):
    assert declarative._validate_entry(entry) == []

    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    jsonschema.validate({'version': 1, 'types': [entry]}, schema)


# --- Editor schema tightness, document-level `version` field --------------
#
# The loader's version gate requires a real int equal to 1 (bool, string,
# float, and other ints are all rejected). JSON Schema's `const`/`type`
# keywords cannot fully express that: draft-07 has no separate "float" vs
# "int" data type, so a numeric literal like 1.0 is indistinguishable from
# 1 in the document model, and `{"type": "integer", "const": 1}` is the
# tightest constraint the schema can state. This test pins that contract:
# schema and loader agree everywhere except the irreducible `1.0` case.

def _builtin_entry():
    document = json.loads(BUILTIN_TYPES.read_text(encoding='utf-8'))
    return document['types'][0]


@pytest.mark.parametrize('version', [True, '1', 2, 1.5])
def test_version_parity_both_reject(version, tmp_path):
    document = {'version': version, 'types': [_builtin_entry()]}

    types_file = tmp_path / 'types.json'
    types_file.write_text(json.dumps(document), encoding='utf-8')
    assert declarative._load_types_file(str(types_file)) == []

    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    with pytest.raises(jsonschema.ValidationError):
        jsonschema.validate(document, schema)


def test_version_documented_residual_float_one_accepted_by_schema_only(
        tmp_path):
    # 1.0 is the one shape where the contracts diverge: JSON Schema draft-07
    # has no way to say "reject a numeric literal for being a float", since
    # 1.0 and 1 are the same JSON number. The loader still rejects it
    # (isinstance(version, int) is False for a decoded 1.0), but the schema
    # cannot follow — this is a known, irreducible residual, not a bug.
    document = {'version': 1.0, 'types': [_builtin_entry()]}

    types_file = tmp_path / 'types.json'
    types_file.write_text(json.dumps(document), encoding='utf-8')
    assert declarative._load_types_file(str(types_file)) == []

    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    jsonschema.validate(document, schema)  # does not raise


def test_version_one_accepted_by_both(tmp_path):
    document = {'version': 1, 'types': [_builtin_entry()]}

    types_file = tmp_path / 'types.json'
    types_file.write_text(json.dumps(document), encoding='utf-8')
    assert declarative._load_types_file(str(types_file)) != []

    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    jsonschema.validate(document, schema)  # does not raise


# --- Unknown entry keys ---------------------------------------------------
#
# A typo'd field name used to load silently: the loader ignored the unknown
# key and the entry ran with a default in place of the field the author
# meant to set. Both sides now reject it, which keeps the bidirectional
# contract intact -- tightening only the schema would make a loader-accepted
# entry schema-invalid.

def test_description_is_a_known_key_on_both_sides():
    entry = _minimal_entry(description='why the mask is 4088')
    assert declarative._validate_entry(entry) == []

    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    jsonschema.validate({'version': 1, 'types': [entry]}, schema)


def test_known_entry_keys_matches_the_schema_exactly():
    # The two lists are maintained by hand in different files and different
    # languages; if they drift, one side accepts what the other rejects.
    schema = json.loads(EDITOR_SCHEMA.read_text(encoding='utf-8'))
    schema_keys = set(schema['definitions']['entry']['properties'])
    assert declarative.KNOWN_ENTRY_KEYS == schema_keys
