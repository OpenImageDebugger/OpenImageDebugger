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
