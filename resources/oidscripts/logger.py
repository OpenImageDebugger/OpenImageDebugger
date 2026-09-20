# -*- coding: utf-8 -*-

"""
Handle the configuration and creation of a global logger.
"""

import logging

# create a logging format
formatter = logging.Formatter("[OpenImageDebugger] %(levelname)s: %(message)s")
handler = logging.StreamHandler()
handler.setFormatter(formatter)

# Setup logger
log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)
log.addHandler(handler)
