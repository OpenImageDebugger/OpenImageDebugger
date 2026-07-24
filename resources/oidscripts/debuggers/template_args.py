# -*- coding: utf-8 -*-

"""
Debugger-agnostic access to C++ template arguments by parsing a type's
canonical name.

GDB exposes template parameters through ``gdb.Type.template_argument(i)``,
which the Eigen inspector relies on. LLDB has no equivalent on the plain
type-name string it hands back, so this module provides ``TemplateTypeName``
- a ``str`` subclass that behaves exactly like the type-name string
everywhere it is used, but additionally offers a ``template_argument(i)``
method compatible with the GDB one, backed by parsing the canonical name
(e.g. ``Eigen::Matrix<float, -1, -1, 0, -1, -1>``).

This module is stdlib-only so it imports inside the debugger's embedded
Python and can be unit tested without a debugger.
"""


def _matching_angle(type_name, start):
    """Index of the '>' matching the '<' at ``start``, or -1 if unbalanced."""
    depth = 0
    for i in range(start, len(type_name)):
        char = type_name[i]
        if char == '<':
            depth += 1
        elif char == '>':
            depth -= 1
            if depth == 0:
                return i
    return -1


def _split_top_level_commas(inner):
    """Split ``inner`` on commas that are not inside nested angle brackets."""
    args = []
    depth = 0
    current = ''
    for char in inner:
        if char == '<':
            depth += 1
        elif char == '>':
            depth -= 1
        if char == ',' and depth == 0:
            args.append(current.strip())
            current = ''
        else:
            current += char
    if current.strip():
        args.append(current.strip())
    return args


def split_template_args(type_name):
    """
    Return the top-level template arguments of ``type_name`` as a list of
    trimmed strings, honoring nested angle brackets. Returns an empty list
    when the name has no template argument list.

    >>> split_template_args('Eigen::Matrix<float, -1, -1, 0, -1, -1>')
    ['float', '-1', '-1', '0', '-1', '-1']
    >>> split_template_args('float')
    []
    """
    start = type_name.find('<')
    if start == -1:
        return []
    end = _matching_angle(type_name, start)
    if end == -1:
        return []
    return _split_top_level_commas(type_name[start + 1:end])


class TemplateTypeName(str):
    """
    A type-name string that also exposes GDB-style ``template_argument(i)``.

    The string value is the (possibly typedef'd) type name, so this behaves
    identically to the plain name in every existing use. Template arguments
    are read from ``canonical`` - the fully expanded type name - defaulting
    to the string value itself when no separate canonical name is supplied
    (which is what nested arguments carry).
    """

    def __new__(cls, name, canonical=None):
        obj = super(TemplateTypeName, cls).__new__(cls, name)
        obj._canonical = canonical if canonical is not None else str(name)
        return obj

    def template_argument(self, index):
        """
        Return the ``index``-th template argument as a ``TemplateTypeName``.

        The returned value stringifies to the argument (e.g. ``'float'`` or
        ``'-1'``) so callers can ``str()`` a type argument or ``int()`` a
        non-type one, and can recurse via ``template_argument`` again (used
        for Eigen::Map's nested matrix/stride types). Raises ``IndexError``
        when the argument does not exist.
        """
        args = split_template_args(self._canonical)
        token = args[index]
        return TemplateTypeName(token)

    @property
    def canonical(self):
        """The fully expanded type name backing template_argument()."""
        return self._canonical
