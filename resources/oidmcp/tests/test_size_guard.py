import pytest

from oidscripts.debuggers.interfaces import (
    BufferTooLargeError,
    raise_if_too_large,
)


def test_within_limit_is_silent():
    # At the limit: allowed, returns the documented None (no raise).
    assert raise_if_too_large(100, 100) is None


def test_none_disables_check():
    # No limit configured: any size allowed, yet the guard is still
    # live when a real limit is supplied.
    assert raise_if_too_large(10 ** 12, None) is None
    with pytest.raises(BufferTooLargeError):
        raise_if_too_large(10 ** 12, 100)


def test_over_limit_raises_with_sizes():
    with pytest.raises(BufferTooLargeError) as excinfo:
        raise_if_too_large(101, 100)
    assert excinfo.value.size_bytes == 101
    assert excinfo.value.max_bytes == 100
    assert '101' in str(excinfo.value)
