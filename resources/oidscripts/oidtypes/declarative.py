# -*- coding: utf-8 -*-

"""
Declarative custom-type support.

Loads JSON type descriptions (see resources/schemas/oid-types-v1.json and
doc/declarative-types.md) and exposes each entry as a
TypeInspectorInterface implementation. Field values are debugger
expressions: this module only substitutes placeholders and hands the
resulting strings to the debugger bridge for evaluation — it never
interprets expression semantics itself, so the debugger remains the single
source of expression meaning on every backend.
"""

import difflib
import json
import os
import re

from oidscripts import symbols
from oidscripts.logger import log
from oidscripts.oidtypes import interface

OID_TYPES_PATH_ENV = 'OID_TYPES_PATH'
BUILTIN_TYPES_FILENAME = 'builtin_types.json'
SUPPORTED_VERSION = 1

# Expression dialect families this loader can evaluate. 'cpp' means
# "native debugger expression" (C/C++, and any language gdb/lldb evaluates
# natively). Entries in other dialects are skipped quietly so one types
# file can serve future backends.
ACCEPTED_LANGUAGES = ('cpp',)

# Friendly dtype names plus the C scalar spellings produced by template
# arguments (e.g. Eigen's scalar parameter).
DTYPE_NAMES = {
    'uint8': symbols.OID_TYPES_UINT8,
    'unsigned char': symbols.OID_TYPES_UINT8,
    'uint8_t': symbols.OID_TYPES_UINT8,
    'uint16': symbols.OID_TYPES_UINT16,
    'unsigned short': symbols.OID_TYPES_UINT16,
    'uint16_t': symbols.OID_TYPES_UINT16,
    'int16': symbols.OID_TYPES_INT16,
    'short': symbols.OID_TYPES_INT16,
    'int16_t': symbols.OID_TYPES_INT16,
    'int32': symbols.OID_TYPES_INT32,
    'int': symbols.OID_TYPES_INT32,
    'int32_t': symbols.OID_TYPES_INT32,
    'float32': symbols.OID_TYPES_FLOAT32,
    'float': symbols.OID_TYPES_FLOAT32,
    'float64': symbols.OID_TYPES_FLOAT64,
    'double': symbols.OID_TYPES_FLOAT64,
}
VALID_DTYPE_CODES = frozenset(DTYPE_NAMES.values())
DTYPE_ELEMSIZE = {
    symbols.OID_TYPES_UINT8: 1,
    symbols.OID_TYPES_UINT16: 2,
    symbols.OID_TYPES_INT16: 2,
    symbols.OID_TYPES_INT32: 4,
    symbols.OID_TYPES_FLOAT32: 4,
    symbols.OID_TYPES_FLOAT64: 8,
}

REQUIRED_FIELDS = ('match', 'pointer', 'width', 'height', 'dtype')
FIELD_DEFAULTS = {
    'channels': 1,
    'row_stride': '{width}',
    'pixel_layout': 'rgba',
    'transpose': False,
    'display_name': '{name} ({type})',
}

# Fields resolve in this fixed order; each stage may reference the derived
# placeholders of every earlier stage (checked statically at load time).
# 'pointer' resolves to a cast object, never an int, so it is not
# available as a placeholder.
RESOLUTION_ORDER = ('dtype', 'transpose', 'width', 'height', 'channels',
                    'pointer', 'row_stride', 'pixel_layout')
_DERIVED_BY_STAGE = {
    'dtype': (),
    'transpose': ('dtype', 'elemsize'),
    'width': ('dtype', 'elemsize', 'transpose'),
    'height': ('dtype', 'elemsize', 'transpose', 'width'),
    'channels': ('dtype', 'elemsize', 'transpose', 'width', 'height'),
    'pointer': ('dtype', 'elemsize', 'transpose', 'width', 'height',
                'channels'),
    'row_stride': ('dtype', 'elemsize', 'transpose', 'width', 'height',
                   'channels'),
    'pixel_layout': ('dtype', 'elemsize', 'transpose', 'width', 'height',
                     'channels', 'row_stride'),
}
_ALL_DERIVED = ('dtype', 'elemsize', 'transpose', 'width', 'height',
                'channels', 'row_stride')

# Only these brace tokens are placeholders; any other brace content (e.g.
# C initializer braces) passes through substitution untouched.
_PLACEHOLDER_RE = re.compile(
    r'\{(sym|name|type|targ:\d+(?:\.\d+)*|[a-z_]+)\}')
_PIXEL_LAYOUT_RE = re.compile(r'^[rgba]{4}$')


def _is_targ_token(token):
    return token.startswith('targ:')


def _substitute(text, placeholder_values, targ_resolver):
    """
    Replace {…} placeholders in an expression template. Unknown lowercase
    tokens raise KeyError (rejected earlier, at load time, by entry
    validation); non-placeholder brace content is left untouched.
    """
    def _replace(match_obj):
        token = match_obj.group(1)
        if _is_targ_token(token):
            return targ_resolver(token)
        if token in placeholder_values:
            return placeholder_values[token]
        raise KeyError(token)

    return _PLACEHOLDER_RE.sub(_replace, text)


def _type_strings(symbol_obj):
    """
    Declared plus canonical (typedef-stripped) type strings, for matching.
    GDB rich types expose strip_typedefs(); the LLDB bridge's
    TemplateTypeName carries its canonical name.
    """
    symbol_type = symbol_obj.type
    declared = str(symbol_type)
    strings = [declared]
    strip_typedefs = getattr(symbol_type, 'strip_typedefs', None)
    if strip_typedefs is not None:
        try:
            canonical = str(strip_typedefs())
        except Exception:
            canonical = None
    else:
        canonical = getattr(symbol_type, 'canonical', None)
    if canonical and str(canonical) != declared:
        strings.append(str(canonical))
    return strings


def _template_base(symbol_obj):
    """Type object used for {targ:…} resolution (typedef-stripped on GDB)."""
    symbol_type = symbol_obj.type
    strip_typedefs = getattr(symbol_type, 'strip_typedefs', None)
    if strip_typedefs is not None:
        try:
            return strip_typedefs()
        except Exception:
            return symbol_type
    return symbol_type


def _resolve_targ_token(symbol_obj, token):
    """
    Resolve a {targ:i.j} token to substitution text: decimal for value
    arguments, the type name for type arguments.
    """
    current = _template_base(symbol_obj)
    for index in token[len('targ:'):].split('.'):
        current = current.template_argument(int(index))
    try:
        return str(int(current))
    except (TypeError, ValueError):
        return str(current)


def _symbol_expression(obj_name, symbol_obj):
    """
    {sym} substitution text. Pointer symbols get dereferenced here so
    entries never spell out '->' (mirrors debugger member access, which
    auto-dereferences picked objects).
    """
    for type_string in _type_strings(symbol_obj):
        if type_string.rstrip().endswith('*'):
            return f'(*({obj_name}))'
    return f'({obj_name})'


def _to_int(value):
    return int(value)


def _to_bool(value):
    if isinstance(value, bool):
        return value
    try:
        return int(value) != 0
    except (TypeError, ValueError):
        # LLDB renders boolean expression results as 'true'/'false'.
        text = str(value).strip().lower()
        if text == 'true':
            return True
        if text == 'false':
            return False
        raise ValueError(f'cannot interpret {value!r} as a boolean')


class EntryEvaluationError(RuntimeError):
    """Field evaluation failure carrying the entry and field context."""

    def __init__(self, entry_name, field, detail):
        super().__init__(f"entry '{entry_name}', field '{field}': {detail}")
        self.entry_name = entry_name
        self.field = field
        self.detail = detail


class _Resolution:
    """
    Per-plot resolution state for one entry: the substitution texts
    resolved so far plus the error context (entry and field names). All
    debugger traffic funnels through evaluate_text(), which also shortcuts
    substituted text that is already a bare integer (e.g. a template
    argument) so it never reaches the debugger.
    """

    def __init__(self, entry_name, obj_name, symbol_obj, bridge):
        self.entry_name = entry_name
        self.symbol_obj = symbol_obj
        self.bridge = bridge
        self.field = None
        self.placeholders = {'sym': _symbol_expression(obj_name, symbol_obj)}

    def substitute(self, text):
        try:
            return _substitute(
                text, self.placeholders,
                lambda token: _resolve_targ_token(self.symbol_obj, token))
        except KeyError as error:
            raise EntryEvaluationError(
                self.entry_name, self.field,
                f'unknown placeholder {{{error.args[0]}}} in {text!r}')
        except Exception as error:
            raise EntryEvaluationError(
                self.entry_name, self.field,
                f'template argument resolution failed in {text!r}: {error}')

    def evaluate_text(self, substituted):
        try:
            return int(substituted.strip())
        except ValueError:
            pass
        try:
            return self.bridge.evaluate_expression(substituted)
        except RuntimeError as error:
            raise EntryEvaluationError(
                self.entry_name, self.field,
                f'{substituted!r} failed: {error}')

    def evaluate(self, text):
        return self.evaluate_text(self.substitute(text))


def _resolve_node(resolution, node, leaf):
    """Resolve a value node; 'leaf' interprets terminal scalars."""
    if isinstance(node, dict):
        if 'first_valid' in node:
            return _resolve_first_valid(resolution, node['first_valid'], leaf)
        if 'if' in node:
            condition = _to_bool(resolution.evaluate(node['if']))
            branch = node['then'] if condition else node['else']
            return _resolve_node(resolution, branch, leaf)
        return _resolve_map(resolution, node, leaf)
    return leaf(resolution, node)


def _resolve_first_valid(resolution, candidates, leaf):
    """
    Try candidates in order. A candidate is rejected when substitution or
    evaluation fails, the result does not convert, or — for the
    {"expr": …, "min": N} wrapper — the integer result is below N.
    """
    for candidate in candidates:
        try:
            if isinstance(candidate, dict) and 'min' in candidate:
                value = _to_int(resolution.evaluate(candidate['expr']))
                if value < candidate['min']:
                    continue
                return value
            return _resolve_node(resolution, candidate, leaf)
        except (EntryEvaluationError, TypeError, ValueError):
            continue
    raise EntryEvaluationError(
        resolution.entry_name, resolution.field,
        'all first_valid candidates failed')


def _resolve_map(resolution, node, leaf):
    raw = resolution.evaluate(node['expr'])
    try:
        key = str(int(raw))
    except (TypeError, ValueError):
        key = str(raw)
    mapping = node['map']
    if key in mapping:
        return _resolve_node(resolution, mapping[key], leaf)
    if 'default' in node:
        return _resolve_node(resolution, node['default'], leaf)
    raise EntryEvaluationError(
        resolution.entry_name, resolution.field,
        f'value {key!r} is not in the map and no default is given')


def _leaf_int(resolution, node):
    if isinstance(node, str):
        return _to_int(resolution.evaluate(node))
    return _to_int(node)


def _leaf_bool(resolution, node):
    if isinstance(node, str):
        return _to_bool(resolution.evaluate(node))
    return _to_bool(node)


def _leaf_dtype(resolution, node):
    """
    dtype rule: after substitution a known dtype name resolves via the
    table; anything else is a debugger expression whose integer result
    must be a valid OID pixel type code.
    """
    if isinstance(node, str):
        text = resolution.substitute(node).strip()
        if text in DTYPE_NAMES:
            return DTYPE_NAMES[text]
        try:
            code = _to_int(resolution.evaluate_text(text))
        except (EntryEvaluationError, TypeError, ValueError) as error:
            close = difflib.get_close_matches(text, DTYPE_NAMES, 1)
            if close:
                raise EntryEvaluationError(
                    resolution.entry_name, resolution.field,
                    f'{text!r} is not a known dtype name (closest is '
                    f"'{close[0]}') and failed to evaluate: {error}")
            raise
    else:
        code = _to_int(node)
    if code not in VALID_DTYPE_CODES:
        raise EntryEvaluationError(
            resolution.entry_name, resolution.field,
            f'{code} is not a valid OID pixel type code '
            f'(valid: {sorted(VALID_DTYPE_CODES)})')
    return code


def _string_node_errors(text, available):
    """Placeholder-availability problems in one expression string."""
    errors = []
    for match_obj in _PLACEHOLDER_RE.finditer(text):
        token = match_obj.group(1)
        if token == 'sym' or _is_targ_token(token):
            continue
        if token not in available:
            errors.append(
                f'placeholder {{{token}}} is not available in this field')
    return errors


def _validate_node(node, available):
    """
    Structural check of one value node. Returns a list of problems
    (empty when valid).
    """
    if isinstance(node, (bool, int, float)):
        return []
    if isinstance(node, str):
        return _string_node_errors(node, available)
    if isinstance(node, dict):
        if 'first_valid' in node:
            candidates = node['first_valid']
            if not isinstance(candidates, list) or not candidates:
                return ['first_valid must be a non-empty array']
            errors = []
            for candidate in candidates:
                if isinstance(candidate, dict) and 'min' in candidate:
                    if (set(candidate) != {'expr', 'min'}
                            or not isinstance(candidate['expr'], str)
                            or isinstance(candidate['min'], bool)
                            or not isinstance(candidate['min'], int)):
                        errors.append('min wrapper must be '
                                      '{"expr": <string>, "min": <integer>}')
                    else:
                        errors.extend(_string_node_errors(
                            candidate['expr'], available))
                else:
                    errors.extend(_validate_node(candidate, available))
            return errors
        if 'if' in node:
            errors = []
            if not isinstance(node['if'], str):
                errors.append('if condition must be an expression string')
            else:
                errors.extend(_string_node_errors(node['if'], available))
            for branch in ('then', 'else'):
                if branch not in node:
                    errors.append(f'if node is missing "{branch}"')
                else:
                    errors.extend(_validate_node(node[branch], available))
            return errors
        if 'expr' in node and 'map' in node:
            errors = []
            if not isinstance(node['expr'], str):
                errors.append('map expr must be an expression string')
            else:
                errors.extend(_string_node_errors(node['expr'], available))
            if not isinstance(node['map'], dict) or not node['map']:
                errors.append('map must be a non-empty object')
            else:
                for value in node['map'].values():
                    errors.extend(_validate_node(value, available))
            if 'default' in node:
                errors.extend(_validate_node(node['default'], available))
            return errors
        return ['unknown node shape (expected first_valid, if/then/else '
                'or expr/map)']
    return [f'unsupported value type {type(node).__name__}']


def _validate_pixel_layout(node, available):
    """pixel_layout: 4-char rgba literal, or if/else over such literals."""
    if isinstance(node, str):
        if _PIXEL_LAYOUT_RE.match(node):
            return []
        return [f'pixel_layout {node!r} must be exactly 4 characters '
                'from "rgba"']
    if isinstance(node, dict) and 'if' in node:
        errors = []
        if not isinstance(node['if'], str):
            errors.append('if condition must be an expression string')
        else:
            errors.extend(_string_node_errors(node['if'], available))
        for branch in ('then', 'else'):
            if branch not in node:
                errors.append(f'if node is missing "{branch}"')
            else:
                errors.extend(
                    _validate_pixel_layout(node[branch], available))
        return errors
    return ['pixel_layout must be a literal layout string or an '
            'if/then/else over literal layout strings']


def _validate_entry(entry):
    """
    Load-time structural validation of one entry. Returns a list of
    problems; a non-empty list means the loader skips the entry with a
    warning. Runtime evaluation of a validated entry can then only fail
    inside the debugger (handled by EntryEvaluationError).
    """
    if not isinstance(entry, dict):
        return ['entry must be an object']
    errors = []
    for field in REQUIRED_FIELDS:
        if field not in entry:
            errors.append(f'missing required field "{field}"')
    if 'match' in entry:
        if not isinstance(entry['match'], str):
            errors.append('match must be a regex string')
        else:
            try:
                re.compile(entry['match'])
            except re.error as error:
                errors.append(f'match regex does not compile: {error}')
    for field in RESOLUTION_ORDER:
        if field not in entry and field in REQUIRED_FIELDS:
            continue  # absence already reported above
        node = entry[field] if field in entry else FIELD_DEFAULTS[field]
        available = frozenset(_DERIVED_BY_STAGE[field])
        if field == 'pixel_layout':
            problems = _validate_pixel_layout(node, available)
        elif field == 'pointer' and isinstance(node, (bool, int, float)):
            problems = ['pointer must be an expression or node, not a '
                        'literal number']
        else:
            problems = _validate_node(node, available)
        errors.extend(f'field "{field}": {problem}' for problem in problems)
    display_name = entry.get('display_name', FIELD_DEFAULTS['display_name'])
    if not isinstance(display_name, str):
        errors.append('display_name must be a string')
    else:
        for match_obj in _PLACEHOLDER_RE.finditer(display_name):
            token = match_obj.group(1)
            if token in ('name', 'type') or _is_targ_token(token):
                continue
            errors.append(
                f'display_name placeholder {{{token}}} is not supported '
                '(use {name}, {type} or {targ:...})')
    return errors
