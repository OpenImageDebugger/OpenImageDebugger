import logging
import sys


def test_logger_never_holds_a_reference_to_stdout():
    from oidscripts import logger

    streams = [
        handler.stream
        for handler in logger.log.handlers
        if isinstance(handler, logging.StreamHandler)
    ]
    assert streams, 'the module installs no StreamHandler'
    assert all(stream is not sys.stdout for stream in streams)
