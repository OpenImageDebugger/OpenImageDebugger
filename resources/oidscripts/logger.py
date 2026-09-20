# -*- coding: utf-8 -*-

"""
Handle the configuration and creation of a global logger.
"""

import logging
import sys


class _CurrentStdout:
    def write(self, text):
        return sys.stdout.write(text)

    def flush(self):
        sys.stdout.flush()


# create a logging format
formatter = logging.Formatter("[OpenImageDebugger] %(levelname)s: %(message)s")
handler = logging.StreamHandler(_CurrentStdout())
handler.setFormatter(formatter)

# Setup logger
log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)
log.addHandler(handler)
