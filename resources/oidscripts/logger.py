# -*- coding: utf-8 -*-

"""
Handle the configuration and creation of a global logger.
"""

import logging
import sys

# create a logging format
formatter = logging.Formatter("[OpenImageDebugger] %(levelname)s: %(message)s")
handler = logging.StreamHandler(sys.stdout)
handler.setFormatter(formatter)

# Setup logger
log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)
log.addHandler(handler)
