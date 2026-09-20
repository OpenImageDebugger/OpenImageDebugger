import io
import logging
import sys


def _stream_handlers():
    from oidscripts import logger

    return [h for h in logger.log.handlers if isinstance(h, logging.StreamHandler)]


def test_logger_never_holds_a_reference_to_stdout():
    handlers = _stream_handlers()
    assert handlers, 'the module installs no StreamHandler'
    assert all(h.stream is not sys.stdout for h in handlers)


def test_logger_writes_to_whichever_stdout_is_current(monkeypatch):
    from oidscripts import logger

    replacement = io.StringIO()
    monkeypatch.setattr(sys, 'stdout', replacement)
    logger.log.warning('probe line')
    assert '[OpenImageDebugger] WARNING: probe line' in replacement.getvalue()
