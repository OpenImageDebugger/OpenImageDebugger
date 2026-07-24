"""
Unit tests for the declarative type engine: placeholder substitution,
type-string collection, template-argument resolution, value conversion,
node resolution and entry validation. No debugger involved — a recording
fake bridge stands in for expression evaluation.
"""

import pytest

from oidscripts import symbols
from oidscripts.debuggers.template_args import TemplateTypeName
from oidscripts.oidtypes import declarative


class GdbStyleType:
    """Mimics gdb.Type: str() plus strip_typedefs()."""

    def __init__(self, declared, canonical=None):
        self._declared = declared
        self._canonical = canonical or declared

    def __str__(self):
        return self._declared

    def strip_typedefs(self):
        return GdbStyleType(self._canonical)


class FakeSymbol:
    def __init__(self, type_obj):
        self.type = type_obj


def test_substitute_replaces_known_placeholders():
    result = declarative._substitute(
        '{sym}.cols + {width}', {'sym': '(img)', 'width': '10'}, None)
    assert result == '(img).cols + 10'


def test_substitute_leaves_non_placeholder_braces_untouched():
    result = declarative._substitute('{sym}.x = {0}', {'sym': '(img)'}, None)
    assert result == '(img).x = {0}'


def test_substitute_unknown_lowercase_token_raises():
    with pytest.raises(KeyError):
        declarative._substitute('{sym}.x + {bogus}', {'sym': '(img)'}, None)


def test_substitute_resolves_targ_tokens():
    symbol = FakeSymbol(TemplateTypeName(
        'Eigen::MatrixXf', 'Eigen::Matrix<float, -1, -1, 0, -1, -1>'))
    result = declarative._substitute(
        '{targ:1} + 1', {},
        lambda token: declarative._resolve_targ_token(symbol, token))
    assert result == '-1 + 1'


def test_resolve_targ_nested_path():
    symbol = FakeSymbol(TemplateTypeName(
        'Eigen::Map<Eigen::Matrix<float, 3, 4, 0, 3, 4>, 0, '
        'Eigen::Stride<0, 5> >'))
    assert declarative._resolve_targ_token(symbol, 'targ:0.0') == 'float'
    assert declarative._resolve_targ_token(symbol, 'targ:0.2') == '4'
    assert declarative._resolve_targ_token(symbol, 'targ:2.1') == '5'


def test_type_strings_includes_canonical_gdb_style():
    symbol = FakeSymbol(GdbStyleType('MatAlias', 'cv::Mat'))
    assert declarative._type_strings(symbol) == ['MatAlias', 'cv::Mat']


def test_type_strings_includes_canonical_lldb_style():
    symbol = FakeSymbol(TemplateTypeName(
        'Eigen::MatrixXf', 'Eigen::Matrix<float, -1, -1, 0, -1, -1>'))
    assert declarative._type_strings(symbol) == [
        'Eigen::MatrixXf', 'Eigen::Matrix<float, -1, -1, 0, -1, -1>']


def test_type_strings_deduplicates_when_no_typedef():
    symbol = FakeSymbol(TemplateTypeName('cv::Mat'))
    assert declarative._type_strings(symbol) == ['cv::Mat']


def test_symbol_expression_plain_and_pointer():
    plain = FakeSymbol(TemplateTypeName('cv::Mat'))
    pointer = FakeSymbol(TemplateTypeName('cv::Mat *'))
    assert declarative._symbol_expression('img', plain) == '(img)'
    assert declarative._symbol_expression('img', pointer) == '(*(img))'


def test_symbol_expression_derefs_typedefd_pointer():
    aliased = FakeSymbol(GdbStyleType('ImagePtr', 'MyImage *'))
    assert declarative._symbol_expression('img', aliased) == '(*(img))'


def test_to_bool_handles_lldb_bool_strings():
    class Rendered:
        def __init__(self, text):
            self._text = text

        def __str__(self):
            return self._text

    assert declarative._to_bool(Rendered('true')) is True
    assert declarative._to_bool(Rendered('false')) is False
    assert declarative._to_bool(1) is True
    assert declarative._to_bool(0) is False
    assert declarative._to_bool(True) is True


class RecordingBridge:
    """
    Fake bridge for engine tests: records every expression string and
    returns scripted results (or raises scripted errors). Tests assert on
    the exact strings the debugger would receive — the single-evaluator
    rule made visible.
    """

    def __init__(self, results=None):
        self.requests = []
        self.results = dict(results or {})

    def evaluate_expression(self, expression):
        self.requests.append(expression)
        if expression not in self.results:
            raise RuntimeError(
                f'Expression "{expression}" failed: not scripted')
        result = self.results[expression]
        if isinstance(result, Exception):
            raise result
        return result


def make_resolution(bridge, type_name='MyImage', field='width'):
    resolution = declarative._Resolution(
        'TestEntry', 'img', FakeSymbol(TemplateTypeName(type_name)), bridge)
    resolution.field = field
    return resolution


def test_evaluate_bare_int_skips_debugger():
    bridge = RecordingBridge()
    resolution = make_resolution(bridge)
    resolution.placeholders['width'] = '10'
    assert resolution.evaluate('{width}') == 10
    assert bridge.requests == []


def test_evaluate_sends_substituted_text_to_bridge():
    bridge = RecordingBridge({'(img).cols': 640})
    resolution = make_resolution(bridge)
    assert resolution.evaluate('{sym}.cols') == 640
    assert bridge.requests == ['(img).cols']


def test_evaluate_wraps_bridge_error_with_entry_and_field():
    bridge = RecordingBridge()
    resolution = make_resolution(bridge)
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        resolution.evaluate('{sym}.cols')
    message = str(excinfo.value)
    assert "entry 'TestEntry'" in message
    assert "field 'width'" in message
    assert '(img).cols' in message


def test_first_valid_min_rejects_dynamic_targ_without_debugger():
    bridge = RecordingBridge({'(img).m_storage.m_rows': 8})
    resolution = make_resolution(
        bridge, type_name='Eigen::Matrix<float, -1, -1, 0, -1, -1>')
    candidates = [{'expr': '{targ:1}', 'min': 1}, '{sym}.m_storage.m_rows']
    value = declarative._resolve_first_valid(
        resolution, candidates, declarative._leaf_int)
    assert value == 8


def test_leaf_int_rejects_float_literal():
    # A float size literal must error (with entry/field context) rather
    # than be silently truncated by int() (640.9 -> 640).
    resolution = make_resolution(RecordingBridge(), field='width')
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._resolve_node(resolution, 640.9, declarative._leaf_int)
    assert excinfo.value.field == 'width'


def test_leaf_int_rejects_bool_literal():
    # A JSON boolean size literal must error rather than coerce to 0/1.
    resolution = make_resolution(RecordingBridge(), field='height')
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._resolve_node(resolution, True, declarative._leaf_int)
    assert excinfo.value.field == 'height'


def test_leaf_int_rejects_negative_literal():
    resolution = make_resolution(RecordingBridge(), field='width')
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._resolve_node(resolution, -4, declarative._leaf_int)
    assert 'non-negative' in str(excinfo.value)
    assert excinfo.value.field == 'width'


def test_leaf_int_rejects_negative_expression_result():
    # Validation cannot see a resolved value, so a negative from an
    # expression must be rejected at runtime.
    bridge = RecordingBridge({'neg_expr': -1})
    resolution = make_resolution(bridge, field='height')
    with pytest.raises(declarative.EntryEvaluationError):
        declarative._resolve_node(
            resolution, 'neg_expr', declarative._leaf_int)
    assert bridge.requests == ['neg_expr']


def test_first_valid_min_accepts_static_targ_without_debugger():
    bridge = RecordingBridge()
    resolution = make_resolution(
        bridge, type_name='Eigen::Matrix<float, 3, 4, 0, 3, 4>')
    candidates = [{'expr': '{targ:1}', 'min': 1}, '{sym}.m_storage.m_rows']
    assert declarative._resolve_first_valid(
        resolution, candidates, declarative._leaf_int) == 3
    assert bridge.requests == []


def test_first_valid_falls_through_on_evaluation_error():
    bridge = RecordingBridge({'(img).data': 12345})
    resolution = make_resolution(bridge, field='pointer')
    candidates = ['{sym}.data.ptr', '{sym}.data']
    value = declarative._resolve_first_valid(
        resolution, candidates, declarative._leaf_int)
    assert value == 12345
    assert bridge.requests == ['(img).data.ptr', '(img).data']


def test_first_valid_exhausted_raises_entry_error():
    bridge = RecordingBridge()
    resolution = make_resolution(bridge)
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._resolve_first_valid(
            resolution, ['{sym}.a', '{sym}.b'], declarative._leaf_int)
    assert 'first_valid' in str(excinfo.value)


def test_if_node_picks_branch_from_derived_placeholder():
    bridge = RecordingBridge()
    resolution = make_resolution(bridge)
    node = {'if': '{transpose}', 'then': 3, 'else': 4}
    resolution.placeholders['transpose'] = '1'
    assert declarative._resolve_node(
        resolution, node, declarative._leaf_int) == 3
    resolution.placeholders['transpose'] = '0'
    assert declarative._resolve_node(
        resolution, node, declarative._leaf_int) == 4
    assert bridge.requests == []


def test_if_condition_can_use_debugger():
    bridge = RecordingBridge({'3 >= 3': 1})
    resolution = make_resolution(bridge)
    resolution.placeholders['channels'] = '3'
    node = {'if': '{channels} >= 3', 'then': 1, 'else': 2}
    assert declarative._resolve_node(
        resolution, node, declarative._leaf_int) == 1
    assert bridge.requests == ['3 >= 3']


def test_map_node_normalizes_integral_keys():
    bridge = RecordingBridge({'(img).depth & 0xffffffff': 8})
    resolution = make_resolution(bridge, field='dtype')
    node = {'expr': '{sym}.depth & 0xffffffff',
            'map': {'8': 'uint8', '64': 'float64'}}
    assert declarative._resolve_node(
        resolution, node,
        declarative._leaf_dtype) == symbols.OID_TYPES_UINT8


def test_map_node_stringifies_non_integral_results():
    class Rendered:
        def __str__(self):
            return 'foo'

    bridge = RecordingBridge({'(img).tag': Rendered()})
    resolution = make_resolution(bridge, field='dtype')
    node = {'expr': '{sym}.tag', 'map': {'foo': 'uint8'}}
    assert declarative._resolve_node(
        resolution, node,
        declarative._leaf_dtype) == symbols.OID_TYPES_UINT8


def test_map_node_miss_without_default_raises():
    bridge = RecordingBridge({'(img).depth': 99})
    resolution = make_resolution(bridge, field='dtype')
    node = {'expr': '{sym}.depth', 'map': {'8': 'uint8'}}
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._resolve_node(resolution, node, declarative._leaf_dtype)
    assert '99' in str(excinfo.value)


def test_map_node_uses_default_on_miss():
    bridge = RecordingBridge({'(img).depth': 99})
    resolution = make_resolution(bridge, field='dtype')
    node = {'expr': '{sym}.depth', 'map': {'8': 'uint8'},
            'default': 'float32'}
    assert declarative._resolve_node(
        resolution, node,
        declarative._leaf_dtype) == symbols.OID_TYPES_FLOAT32


def test_dtype_name_from_template_arg_needs_no_debugger():
    bridge = RecordingBridge()
    resolution = make_resolution(
        bridge, type_name='Eigen::Matrix<float, 3, 3, 0, 3, 3>',
        field='dtype')
    assert declarative._leaf_dtype(
        resolution, '{targ:0}') == symbols.OID_TYPES_FLOAT32
    assert bridge.requests == []


def test_dtype_expression_result_must_be_valid_code():
    bridge = RecordingBridge({'(img).flags & 7': 1})
    resolution = make_resolution(bridge, field='dtype')
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._leaf_dtype(resolution, '{sym}.flags & 7')
    assert 'valid OID pixel type' in str(excinfo.value)


def test_dtype_near_miss_names_closest_known_name():
    bridge = RecordingBridge()
    resolution = make_resolution(bridge, field='dtype')
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._leaf_dtype(resolution, 'flaot32')
    assert 'float32' in str(excinfo.value)


def test_dtype_leaf_wraps_non_convertible_result_without_near_miss():
    class Rendered:
        def __str__(self):
            return 'weird-tag'

    bridge = RecordingBridge({'(img).tag': Rendered()})
    resolution = make_resolution(bridge, field='dtype')
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._leaf_dtype(resolution, '{sym}.tag')
    message = str(excinfo.value)
    assert "entry 'TestEntry'" in message
    assert "field 'dtype'" in message
    assert not isinstance(excinfo.value, (TypeError, ValueError))


def test_int_leaf_accepts_literals_and_expressions():
    bridge = RecordingBridge({'(img).cols': 640})
    resolution = make_resolution(bridge)
    assert declarative._leaf_int(resolution, 7) == 7
    assert declarative._leaf_int(resolution, '{sym}.cols') == 640


def test_int_leaf_wraps_non_convertible_result_in_entry_error():
    bridge = RecordingBridge({'(img).cols': 'not-a-number'})
    resolution = make_resolution(bridge)
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        declarative._leaf_int(resolution, '{sym}.cols')
    message = str(excinfo.value)
    assert "entry 'TestEntry'" in message
    assert "field 'width'" in message
    assert not isinstance(excinfo.value, (TypeError, ValueError))


def test_bool_leaf_handles_lldb_rendered_expression():
    class Rendered:
        def __str__(self):
            return 'true'

    bridge = RecordingBridge({'(0 & 1) == 0': Rendered()})
    resolution = make_resolution(bridge, field='transpose')
    assert declarative._leaf_bool(resolution, '(0 & 1) == 0') is True
    assert declarative._leaf_bool(resolution, False) is False


FLOOR_ENTRY = {
    'match': '^MyImage$',
    'pointer': '{sym}.data',
    'width': '{sym}.w',
    'height': '{sym}.h',
    'dtype': 'float32',
}


def test_validate_floor_entry_is_clean():
    assert declarative._validate_entry(dict(FLOOR_ENTRY)) == []


def test_validate_reports_missing_required_fields():
    entry = dict(FLOOR_ENTRY)
    del entry['pointer']
    errors = declarative._validate_entry(entry)
    assert any('pointer' in error for error in errors)


def test_validate_rejects_bad_match_regex():
    entry = dict(FLOOR_ENTRY, match='(')
    errors = declarative._validate_entry(entry)
    assert any('match regex' in error for error in errors)


def test_validate_rejects_not_yet_resolved_placeholder():
    entry = dict(FLOOR_ENTRY, width='{channels} * 2')
    errors = declarative._validate_entry(entry)
    assert any('{channels}' in error and 'width' in error
               for error in errors)


def test_validate_allows_derived_placeholders_in_later_stages():
    entry = dict(FLOOR_ENTRY,
                 row_stride='{sym}.step / {channels} / {elemsize}')
    assert declarative._validate_entry(entry) == []


def test_validate_rejects_unknown_placeholder():
    entry = dict(FLOOR_ENTRY, height='{bogus}')
    errors = declarative._validate_entry(entry)
    assert any('{bogus}' in error for error in errors)


def test_validate_pixel_layout_literal_and_if():
    good = dict(FLOOR_ENTRY, pixel_layout={
        'if': '{channels} >= 3', 'then': 'bgra', 'else': 'rgba'})
    assert declarative._validate_entry(good) == []
    for bad_layout in ('bgr', 'xyzw', '{sym}.layout',
                       {'if': '{channels} >= 3', 'then': 'bgra'}):
        entry = dict(FLOOR_ENTRY, pixel_layout=bad_layout)
        assert declarative._validate_entry(entry) != []


def test_validate_rejects_literal_pointer():
    entry = dict(FLOOR_ENTRY, pointer=1234)
    errors = declarative._validate_entry(entry)
    assert any('pointer' in error for error in errors)


def test_validate_rejects_numeric_pointer_leaf_in_first_valid():
    # A literal number is never a valid pointer expression -- not only at
    # the top level but also as a leaf nested inside first_valid/map/if,
    # where it would otherwise reach get_casted_pointer as a bare int.
    entry = dict(FLOOR_ENTRY,
                 pointer={'first_valid': ['{sym}.data', 0]})
    errors = declarative._validate_entry(entry)
    assert any('pointer' in error and 'literal number' in error
               for error in errors)


def test_validate_allows_string_pointer_leaf_in_first_valid():
    entry = dict(FLOOR_ENTRY,
                 pointer={'first_valid': ['{sym}.data.ptr', '{sym}.data']})
    assert declarative._validate_entry(entry) == []


def test_pointer_decimal_literal_resolves_via_debugger_not_int():
    # A pointer expression that substitutes to a bare decimal must still be
    # evaluated by the debugger so the result is a value object the bridge
    # can cast; the int() fast path used by numeric fields would hand
    # get_casted_pointer a Python int and break the cast.
    sentinel = object()
    bridge = RecordingBridge({'12345': sentinel})
    resolution = make_resolution(bridge, field='pointer')
    value = declarative._resolve_node(
        resolution, '12345', declarative._leaf_pointer)
    assert value is sentinel
    assert bridge.requests == ['12345']


def test_validate_first_valid_shapes():
    good = dict(FLOOR_ENTRY, width={'first_valid': [
        {'expr': '{targ:1}', 'min': 1}, '{sym}.rows']})
    assert declarative._validate_entry(good) == []
    for bad_width in ({'first_valid': []},
                      {'first_valid': [{'expr': 1, 'min': 1}]},
                      {'foo': 1}):
        entry = dict(FLOOR_ENTRY, width=bad_width)
        assert declarative._validate_entry(entry) != []


def test_validate_map_node_shapes():
    good = dict(FLOOR_ENTRY, dtype={
        'expr': '{sym}.depth', 'map': {'8': 'uint8'}, 'default': 'uint8'})
    assert declarative._validate_entry(good) == []
    bad = dict(FLOOR_ENTRY, dtype={'expr': '{sym}.depth', 'map': {}})
    assert declarative._validate_entry(bad) != []


def test_validate_display_name_placeholders():
    good = dict(FLOOR_ENTRY, display_name='{name} ({targ:0})')
    assert declarative._validate_entry(good) == []
    bad = dict(FLOOR_ENTRY, display_name='{width} wide')
    errors = declarative._validate_entry(bad)
    assert any('display_name' in error for error in errors)


def test_validate_null_field_is_rejected():
    entry = dict(FLOOR_ENTRY, channels=None)
    assert declarative._validate_entry(entry) != []


MY_IMAGE_ENTRY = {
    'name': 'MyImage',
    'match': '^MyImage(?:\\s*[*&])?$',
    'pointer': '{sym}.data',
    'width': '{sym}.w',
    'height': '{sym}.h',
    'dtype': 'float32',
}


class CastingBridge(RecordingBridge):
    """RecordingBridge plus the pointer-cast half of the bridge contract."""

    def __init__(self, results=None):
        super().__init__(results)
        self.casts = []

    def get_casted_pointer(self, typename, obj):
        self.casts.append((typename, obj))
        return obj


def test_inspector_full_metadata_for_floor_entry():
    bridge = CastingBridge({
        '(img).data': 4096,
        '(img).w': 640,
        '(img).h': 480,
    })
    inspector = declarative.DeclarativeInspector(MY_IMAGE_ENTRY, 'test')
    symbol = FakeSymbol(TemplateTypeName('MyImage'))
    metadata = inspector.get_buffer_metadata('img', symbol, bridge)
    assert metadata == {
        'display_name': 'img (MyImage)',
        'pointer': 4096,
        'width': 640,
        'height': 480,
        'channels': 1,
        'type': symbols.OID_TYPES_FLOAT32,
        'row_stride': 640,
        'pixel_layout': 'rgba',
        'transpose_buffer': False,
    }
    assert bridge.casts == [('char', 4096)]


def test_inspector_derefs_pointer_symbols():
    bridge = CastingBridge({
        '(*(img)).data': 4096,
        '(*(img)).w': 8,
        '(*(img)).h': 4,
    })
    inspector = declarative.DeclarativeInspector(MY_IMAGE_ENTRY, 'test')
    symbol = FakeSymbol(TemplateTypeName('MyImage *'))
    metadata = inspector.get_buffer_metadata('img', symbol, bridge)
    assert metadata['width'] == 8
    assert metadata['height'] == 4


def test_inspector_raises_on_null_pointer():
    bridge = CastingBridge({
        '(img).data': 0,
        '(img).w': 640,
        '(img).h': 480,
    })
    inspector = declarative.DeclarativeInspector(MY_IMAGE_ENTRY, 'test')
    symbol = FakeSymbol(TemplateTypeName('MyImage'))
    with pytest.raises(declarative.EntryEvaluationError) as excinfo:
        inspector.get_buffer_metadata('img', symbol, bridge)
    assert 'null' in str(excinfo.value)


def test_inspector_matches_declared_and_canonical():
    inspector = declarative.DeclarativeInspector(MY_IMAGE_ENTRY, 'test')
    aliased = FakeSymbol(GdbStyleType('ImageAlias', 'MyImage'))
    other = FakeSymbol(TemplateTypeName('NotMyImage'))
    assert inspector.is_symbol_observable(aliased, 'img') is True
    assert inspector.is_symbol_observable(other, 'img') is False


def test_inspector_row_stride_uses_derived_placeholders():
    entry = dict(MY_IMAGE_ENTRY, dtype='uint8', channels=3,
                 row_stride='{sym}.step / {channels} / {elemsize}')
    bridge = CastingBridge({
        '(img).data': 4096,
        '(img).w': 640,
        '(img).h': 480,
        '(img).step / 3 / 1': 1920,
    })
    inspector = declarative.DeclarativeInspector(entry, 'test')
    symbol = FakeSymbol(TemplateTypeName('MyImage'))
    metadata = inspector.get_buffer_metadata('img', symbol, bridge)
    assert metadata['row_stride'] == 1920
    assert metadata['channels'] == 3


def test_inspector_display_name_with_targ():
    entry = dict(MY_IMAGE_ENTRY, match='^Wrap<', name='Wrap',
                 display_name='{name} ({targ:0})')
    bridge = CastingBridge({
        '(img).data': 4096,
        '(img).w': 2,
        '(img).h': 2,
    })
    inspector = declarative.DeclarativeInspector(entry, 'test')
    symbol = FakeSymbol(TemplateTypeName('Wrap<float>'))
    metadata = inspector.get_buffer_metadata('img', symbol, bridge)
    assert metadata['display_name'] == 'img (float)'


def test_resolve_pixel_layout_walks_if_chain():
    bridge = RecordingBridge()
    resolution = make_resolution(bridge, field='pixel_layout')
    resolution.placeholders['channels'] = '3'
    node = {'if': '{channels}', 'then': 'bgra', 'else': 'rgba'}
    assert declarative._resolve_pixel_layout(resolution, node) == 'bgra'
    resolution.placeholders['channels'] = '0'
    assert declarative._resolve_pixel_layout(resolution, node) == 'rgba'
    assert bridge.requests == []
