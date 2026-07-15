import sys
from pathlib import Path

# Make `import oidscripts.agentendpoint` work: the endpoint lives in the
# sibling debugger-scripts tree, not in this package.
RESOURCES_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RESOURCES_DIR))
