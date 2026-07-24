"""
Tests for declarative types file loading and discovery: OID_TYPES_PATH,
cwd walk-up, error-layer degradation (debug vs warning), language and
version filtering, and TypeBridge registration order.
"""

import json
import logging
import os

from oidscripts import typebridge
from oidscripts.debuggers.template_args import TemplateTypeName
from oidscripts.oidtypes import declarative

VALID_DOC = {
    'version': 1,
    'types': [{
        'name': 'MyImage',
        'match': '^MyImage$',
        'pointer': '{sym}.data',
        'width': '{sym}.w',
        'height': '{sym}.h',
        'dtype': 'float32',
    }],
}


class FakeSymbol:
    def __init__(self, type_name):
        self.type = TemplateTypeName(type_name)


class FakeBridge:
    """Scripted evaluate/cast pair for end-to-end TypeBridge routing."""

    def __init__(self, results):
        self.results = dict(results)

    def evaluate_expression(self, expression):
        if expression not in self.results:
            raise RuntimeError(f'Expression "{expression}" failed')
        return self.results[expression]

    def get_casted_pointer(self, typename, obj):
        return obj


def write_doc(path, document):
    path.write_text(json.dumps(document), encoding='utf-8')


def test_load_types_file_returns_inspectors(tmp_path):
    types_file = tmp_path / 'types.json'
    write_doc(types_file, VALID_DOC)
    inspectors = declarative._load_types_file(str(types_file))
    assert len(inspectors) == 1
    assert inspectors[0].name == 'MyImage'
    assert inspectors[0].source == str(types_file)


def test_nonexistent_file_is_quiet(tmp_path, caplog):
    caplog.set_level(logging.DEBUG, logger='oidscripts.logger')
    missing = tmp_path / 'nope.json'
    assert declarative._load_types_file(str(missing)) == []
    assert not [record for record in caplog.records
                if record.levelno >= logging.WARNING]


def test_missing_builtin_types_file_warns(monkeypatch, caplog):
    # A missing user file is normal (quiet), but a missing *builtin* file
    # is a packaging error and must surface as a warning.
    caplog.set_level(logging.DEBUG, logger='oidscripts.logger')
    monkeypatch.setattr(declarative, 'BUILTIN_TYPES_FILENAME',
                        'does_not_exist_builtin.json')
    assert declarative.load_builtin_inspectors() == []
    assert any('builtin types file' in record.message.lower()
               for record in caplog.records
               if record.levelno == logging.WARNING)


def test_invalid_json_warns_and_skips_file(tmp_path, caplog):
    caplog.set_level(logging.DEBUG, logger='oidscripts.logger')
    types_file = tmp_path / 'types.json'
    types_file.write_text('{not json', encoding='utf-8')
    assert declarative._load_types_file(str(types_file)) == []
    assert any(record.levelno == logging.WARNING
               for record in caplog.records)


def test_wrong_version_warns_and_skips_file(tmp_path, caplog):
    caplog.set_level(logging.DEBUG, logger='oidscripts.logger')
    types_file = tmp_path / 'types.json'
    write_doc(types_file, dict(VALID_DOC, version=2))
    assert declarative._load_types_file(str(types_file)) == []
    assert any('version' in record.message.lower()
               for record in caplog.records
               if record.levelno == logging.WARNING)


def test_unknown_language_entry_skipped_without_warning(tmp_path, caplog):
    caplog.set_level(logging.DEBUG, logger='oidscripts.logger')
    python_entry = dict(VALID_DOC['types'][0], name='PyImage',
                        language='python')
    document = {'version': 1,
                'types': [python_entry, VALID_DOC['types'][0]]}
    types_file = tmp_path / 'types.json'
    write_doc(types_file, document)
    inspectors = declarative._load_types_file(str(types_file))
    assert [inspector.name for inspector in inspectors] == ['MyImage']
    assert not [record for record in caplog.records
                if record.levelno >= logging.WARNING]


def test_invalid_entry_skipped_rest_of_file_loads(tmp_path, caplog):
    caplog.set_level(logging.DEBUG, logger='oidscripts.logger')
    broken = {'name': 'Broken', 'match': '^Broken$'}  # missing required
    document = {'version': 1, 'types': [broken, VALID_DOC['types'][0]]}
    types_file = tmp_path / 'types.json'
    write_doc(types_file, document)
    inspectors = declarative._load_types_file(str(types_file))
    assert [inspector.name for inspector in inspectors] == ['MyImage']
    assert any('Broken' in record.message for record in caplog.records
               if record.levelno == logging.WARNING)


def test_unknown_fields_are_ignored(tmp_path):
    entry = dict(VALID_DOC['types'][0], frobnicate=True)
    document = {'version': 1, 'types': [entry], 'future_key': {}}
    types_file = tmp_path / 'types.json'
    write_doc(types_file, document)
    assert len(declarative._load_types_file(str(types_file))) == 1


def test_env_var_lists_files_in_order(tmp_path, monkeypatch):
    first = tmp_path / 'first.json'
    second = tmp_path / 'second.json'
    write_doc(first, VALID_DOC)
    write_doc(second, {'version': 1, 'types': [dict(
        VALID_DOC['types'][0], name='Other', match='^Other$')]})
    monkeypatch.setenv('OID_TYPES_PATH',
                       os.pathsep.join([str(first), str(second)]))
    assert declarative.discover_user_type_files() == [str(first),
                                                      str(second)]
    names = [inspector.name
             for inspector in declarative.load_user_inspectors()]
    assert names == ['MyImage', 'Other']


def test_walk_up_finds_nearest_types_json(tmp_path, monkeypatch):
    monkeypatch.delenv('OID_TYPES_PATH', raising=False)
    root_file = tmp_path / '.oid' / 'types.json'
    root_file.parent.mkdir()
    write_doc(root_file, VALID_DOC)
    nested = tmp_path / 'a' / 'b'
    nested.mkdir(parents=True)
    monkeypatch.chdir(nested)
    found = [os.path.realpath(path)
             for path in declarative.discover_user_type_files()]
    assert found == [os.path.realpath(str(root_file))]


def test_walk_up_prefers_closest_directory(tmp_path, monkeypatch):
    monkeypatch.delenv('OID_TYPES_PATH', raising=False)
    for directory in (tmp_path, tmp_path / 'a'):
        (directory / '.oid').mkdir(parents=True)
        write_doc(directory / '.oid' / 'types.json', VALID_DOC)
    monkeypatch.chdir(tmp_path / 'a')
    found = [os.path.realpath(path)
             for path in declarative.discover_user_type_files()]
    assert found == [os.path.realpath(
        str(tmp_path / 'a' / '.oid' / 'types.json'))]


def test_empty_env_var_disables_discovery_without_walk_up(tmp_path,
                                                          monkeypatch):
    # An explicitly empty OID_TYPES_PATH means "no user type files": it is
    # present, so it wins over the walk-up fallback and returns [] rather
    # than loading a nearby .oid/types.json.
    (tmp_path / '.oid').mkdir()
    write_doc(tmp_path / '.oid' / 'types.json', VALID_DOC)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    monkeypatch.chdir(tmp_path)
    assert declarative.discover_user_type_files() == []


def test_env_var_wins_over_walk_up(tmp_path, monkeypatch):
    env_file = tmp_path / 'from_env.json'
    write_doc(env_file, VALID_DOC)
    (tmp_path / '.oid').mkdir()
    write_doc(tmp_path / '.oid' / 'types.json', VALID_DOC)
    monkeypatch.setenv('OID_TYPES_PATH', str(env_file))
    monkeypatch.chdir(tmp_path)
    assert declarative.discover_user_type_files() == [str(env_file)]


def test_type_bridge_registers_user_json_first(tmp_path, monkeypatch):
    types_file = tmp_path / 'types.json'
    write_doc(types_file, VALID_DOC)
    monkeypatch.setenv('OID_TYPES_PATH', str(types_file))
    bridge_of_types = typebridge.TypeBridge()
    first = bridge_of_types._type_inspectors[0]
    assert isinstance(first, declarative.DeclarativeInspector)
    assert first.name == 'MyImage'


def test_type_bridge_constructs_without_user_files(tmp_path, monkeypatch):
    # Would raise TypeError (bare DeclarativeInspector instantiation)
    # without the class guard in TypeBridge.__init__.
    monkeypatch.delenv('OID_TYPES_PATH', raising=False)
    monkeypatch.chdir(tmp_path)
    bridge_of_types = typebridge.TypeBridge()
    assert isinstance(bridge_of_types._type_inspectors, list)


def test_user_json_shadows_python_inspector(tmp_path, monkeypatch):
    document = {'version': 1, 'types': [{
        'name': 'Mat override',
        'match': '^cv::Mat$',
        'pointer': '{sym}.data',
        'width': '{sym}.cols',
        'height': '{sym}.rows',
        'dtype': 'uint8',
    }]}
    types_file = tmp_path / 'types.json'
    write_doc(types_file, document)
    monkeypatch.setenv('OID_TYPES_PATH', str(types_file))
    bridge_of_types = typebridge.TypeBridge()
    debugger = FakeBridge({
        '(img).data': 4096,
        '(img).cols': 320,
        '(img).rows': 200,
    })
    symbol = FakeSymbol('cv::Mat')
    metadata = bridge_of_types.get_buffer_metadata('img', symbol, debugger)
    assert metadata['width'] == 320
    assert metadata['type'] == 0  # uint8 — the override, not opencv.py
