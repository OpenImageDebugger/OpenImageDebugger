# -*- coding: utf-8 -*-

"""End-to-end: the JSON resolver inside gdb itself, not the sys.modules fakes.

Every other test in this suite drives the resolver through fake debugger
modules; this one proves the same entry points against gdb's embedded
Python — block iteration, parse_and_eval field reads, and the user-types
latch — using the repo's own testbench/.oid/types.json as the user-type file
so that artifact is exercised too.

Skipped wherever gdb or g++ is missing (macOS ships neither); the dedicated
CI job installs both so the test always runs there.
"""

import json
import os
import pathlib
import shutil
import subprocess
import sys

import pytest

GDB = shutil.which('gdb')
GXX = shutil.which('g++')

# darwin is excluded even when a gdb binary exists (homebrew ships one):
# without the codesigning ritual macOS gdb cannot control an inferior, so
# the run fails on the environment, not the code. The CI lane is Linux.
pytestmark = pytest.mark.skipif(
    sys.platform == 'darwin' or GDB is None or GXX is None,
    reason='needs a Linux host with gdb and g++ (see the gdb-live CI lane)')

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
RESOURCES = REPO_ROOT / 'resources'
TYPES_JSON = REPO_ROOT / 'testbench' / '.oid' / 'types.json'

# Mirrors testbench/realtypes.cpp's PackedGray8 (data/w/h), which the
# checked-in types.json matches; kept minimal so the lane needs only g++.
# Holder exists for the two member paths: a struct-typed local expands to
# qualified members (`holder.member_gray`), and inside a method the members
# of `this` surface BARE (`member_gray`), matching lldbbridge's naming.
FIXTURE_CPP = """\
struct PackedGray8 {
    unsigned char* data;
    int w;
    int h;
};

struct Holder {
    PackedGray8 member_gray;
    int tag;

    void probe() {
        volatile int oid_method_breakpoint = 0;
        (void)oid_method_breakpoint;
    }
};

int main() {
    unsigned char backing[6] = {10, 20, 30, 40, 50, 60};
    PackedGray8 custom_gray{backing, 3, 2};
    Holder holder{{backing, 3, 2}, 7};
    volatile int oid_breakpoint = 0;
    (void)oid_breakpoint;
    (void)custom_gray;
    holder.probe();
    return 0;
}
"""

PROBE_PRELUDE = """\
import json
import sys

sys.path.insert(0, {resources!r})

from oidscripts import oid_resolve

SENTINEL = '|OIDEND'


def payload(raw):
    assert raw.endswith(SENTINEL), raw
    return json.loads(raw[:-len(SENTINEL)])
"""

PROBE_MAIN_PY = PROBE_PRELUDE + """\


page = payload(oid_resolve.list_observable(0))
print('OIDPROBE list ' + json.dumps(page))
meta = payload(oid_resolve.resolve('custom_gray'))
print('OIDPROBE resolve ' + json.dumps(meta))
"""

# Runs at the Holder::probe() stop: list_observable must surface the
# member of `this` under its bare name, and that exact name must resolve.
PROBE_METHOD_PY = PROBE_PRELUDE + """\


page = payload(oid_resolve.list_observable(0))
print('OIDPROBE list2 ' + json.dumps(page))
meta = payload(oid_resolve.resolve('member_gray'))
print('OIDPROBE resolve2 ' + json.dumps(meta))
"""


def _probe_lines(stdout):
    out = {}
    for line in stdout.splitlines():
        if line.startswith('OIDPROBE '):
            _, kind, doc = line.split(' ', 2)
            out[kind] = json.loads(doc)
    return out


def test_resolver_answers_under_gdb(tmp_path):
    source = tmp_path / 'fixture.cpp'
    source.write_text(FIXTURE_CPP)
    binary = tmp_path / 'fixture'
    subprocess.run(
        [GXX, '-g', '-O0', '-o', str(binary), str(source)],
        check=True, capture_output=True, text=True)

    probe_main = tmp_path / 'probe_main.py'
    probe_main.write_text(PROBE_MAIN_PY.format(resources=str(RESOURCES)))
    probe_method = tmp_path / 'probe_method.py'
    probe_method.write_text(PROBE_METHOD_PY.format(resources=str(RESOURCES)))

    lines = FIXTURE_CPP.splitlines()
    bp_main = lines.index('    volatile int oid_breakpoint = 0;') + 1
    bp_method = lines.index(
        '        volatile int oid_method_breakpoint = 0;') + 1

    env = dict(os.environ)
    env['OID_TYPES_PATH'] = str(TYPES_JSON)
    proc = subprocess.run(
        # The probes ride as -x argv elements (executed in order with -ex),
        # never spliced into a 'source ...' command line: gdb takes source's
        # rest-of-line verbatim, so a tmp path with spaces would mis-parse.
        [GDB, '-batch', '-nx',
         '-ex', 'set confirm off',
         '-ex', 'break fixture.cpp:%d' % bp_main,
         '-ex', 'break fixture.cpp:%d' % bp_method,
         '-ex', 'run',
         '-x', str(probe_main),
         '-ex', 'continue',
         '-x', str(probe_method),
         str(binary)],
        capture_output=True, text=True, timeout=120,
        cwd=str(tmp_path), env=env)

    transcript = 'stdout:\n%s\nstderr:\n%s' % (proc.stdout, proc.stderr)
    probes = _probe_lines(proc.stdout)
    assert 'list' in probes, transcript
    assert 'resolve' in probes, transcript
    assert 'list2' in probes, transcript
    assert 'resolve2' in probes, transcript

    page = probes['list']
    assert 'error' not in page, transcript
    names = [item['name'] for item in page['items']]
    assert 'custom_gray' in names, transcript
    # A struct-typed local that is not itself observable expands into its
    # observable members under qualified names.
    assert 'holder.member_gray' in names, transcript
    assert 'holder' not in names, transcript
    # The observable filter must hold under gdb too: a raw byte array
    # and a scalar local match no type entry and stay out of the page.
    assert 'backing' not in names, transcript
    assert 'oid_breakpoint' not in names, transcript

    meta = probes['resolve']
    assert 'error' not in meta, transcript
    assert meta['width'] == 3, transcript
    assert meta['height'] == 2, transcript
    assert meta['channels'] == 1, transcript
    assert meta['byte_count'] == 6, transcript
    assert meta['pointer'] > 0, transcript

    # Inside Holder::probe(): members of `this` surface bare, exactly as
    # lldbbridge names them, and the listed name resolves as-is.
    page2 = probes['list2']
    assert 'error' not in page2, transcript
    names2 = [item['name'] for item in page2['items']]
    assert 'member_gray' in names2, transcript
    assert 'this.member_gray' not in names2, transcript
    assert 'this' not in names2, transcript

    meta2 = probes['resolve2']
    assert 'error' not in meta2, transcript
    assert meta2['width'] == 3, transcript
    assert meta2['height'] == 2, transcript
    assert meta2['byte_count'] == 6, transcript
    assert meta2['pointer'] > 0, transcript
