# Declarative buffer types

Open Image Debugger can be taught new buffer-bearing C/C++ types **without
writing Python**, by describing them in a JSON file that both the native tool
(gdb/lldb) and the VS Code extension read. You describe a type once; it works in
every surface. The built-in OpenCV and Eigen types are themselves defined this
way, in `resources/oidscripts/oidtypes/builtin_types.json` — read it as a
worked-example gallery.

## The five-field floor

The common case — "a struct with a pointer and dimensions" — needs five fields:

```json
{
  "version": 1,
  "types": [
    { "match": "^MyImage$",
      "pointer": "{sym}.data",
      "width": "{sym}.w",
      "height": "{sym}.h",
      "dtype": "float32" }
  ]
}
```

`match` is a Python regular expression tested against the variable's type name;
the four remaining required fields are **debugger expressions** evaluated in the
current frame. Everything else has a default.

## Where the file lives (discovery & precedence)

At session start OID loads types from, in order:

1. **`OID_TYPES_PATH`** — if set, an `os.pathsep`-separated list of JSON file
   paths (this is how the VS Code extension points at your workspace file).
2. Otherwise, the nearest **`.oid/types.json`** found by walking up from the
   current working directory.

Entries are matched in precedence order and the **first match wins**:

1. your JSON entries (workspace file, in file order),
2. your Python `TypeInspectorInterface` subclasses (still supported),
3. the JSON built-ins (`cv::Mat`, `CvMat`, `IplImage`, `Eigen::Matrix`,
   `Eigen::Map`).

So a workspace entry overrides everything, and the built-ins come last. Loading
happens once per debug session; edits take effect the next session.

### Validation while you edit

Point your file at the schema and any JSON-aware editor will check it as you
type: a missing required field, a misspelled key, most invalid values.

```json
{
  "$schema": "https://raw.githubusercontent.com/OpenImageDebugger/OpenImageDebugger/main/resources/schemas/oid-types-v1.json",
  "version": 1,
  "types": []
}
```

## File shape

```json
{
  "$schema": "https://raw.githubusercontent.com/OpenImageDebugger/OpenImageDebugger/main/resources/schemas/oid-types-v1.json",
  "version": 1,
  "types": [ { "…entry…": "…" } ]
}
```

- `$schema` — optional, for editor autocomplete only; the runtime ignores it.
- `version` — required; the v1 loader accepts exactly `1` and skips any other
  file with a warning.
- `types` — array of entries; array order is precedence order within the file.
- Unknown keys at the document level are tolerated (forward compatibility),
  so a file written for a future OID still loads. An unknown key inside an
  entry is not tolerated — see "Error handling" below. Keys inside a
  value-node object (`if`/`then`/`else`, `first_valid`, `expr`/`map`) are
  rejected too, as a typo.

## Entry fields

| Field | Default | Meaning |
|---|---|---|
| `match` | — (required) | Python regex (`re.match`) tested against the declared **and** canonical type strings; matches if either hits |
| `pointer` | — (required) | value node → buffer address |
| `width`, `height` | — (required) | value node → int |
| `dtype` | — (required) | value node → pixel type (see dtype rule) |
| `channels` | `1` | value node → int; `0` is rejected downstream when the buffer is plotted, not at load |
| `row_stride` | `"{width}"` | value node → int (pixels per row of the containing buffer) |
| `pixel_layout` | `"rgba"` | 4 chars from `r g b a` (repeats allowed), or an if/else over such literals. Describes channel **order**, so it only carries meaning for multi-channel data: a single-channel buffer is always displayed from its one channel whatever the layout says. Use the `{channels} >= 3` if/else when an entry can be either |
| `transpose` | `false` | value node → bool |
| `display_name` | `"{name} ({type})"` | template string; placeholders `{name}` (variable name), `{type}` (type string), `{targ:…}`; not debugger-evaluated |
| `language` | `"cpp"` | expression dialect; entries a backend can't evaluate are skipped |
| `name` | the `match` pattern | identifier used in error messages and the conformance tool |
| `description` | none — absent when unset | free text about the entry; never evaluated, never displayed. JSON has no comments, so this is where an expression gets explained |

Write template arguments with `,\s*` rather than a literal comma-space:
debug-info readers disagree on the spacing (`Foo<unsigned char, 3>` vs
`Foo<unsigned char,3>`), and a regex that encodes one spelling silently never
matches under the other. `{targ:N}` extraction is unaffected either way.

## Value grammar

A field value is a **scalar** or one of **three object nodes**.

Scalars are context-typed: numbers and booleans are literals; a string is a
**debugger expression** in numeric/pointer/bool contexts and a
**literal/template** in string contexts (`pixel_layout`, `display_name`).

### Placeholders inside expression strings

- `{sym}` — the inspected symbol, substituted as `(name)` — or `(*(name))` when
  the symbol is a pointer, so you never spell out dereferencing.
- `{targ:i}` / nested `{targ:i.j}` — template-argument path, resolved on the
  canonical type. Value arguments substitute as decimal text, type arguments as
  the type-name string.
- Derived `{width} {height} {channels} {dtype} {elemsize} {transpose}` —
  decimal ints from already-resolved fields. `{elemsize}` is bytes per scalar
  (1/2/2/4/4/8); `{transpose}` is `1`/`0`; `{dtype}` is the OID type code.

Fields resolve in a fixed order, and referencing a not-yet-resolved field is
caught at load time: `dtype → transpose → width/height → channels → pointer →
row_stride → pixel_layout` (then `display_name`).

### Object nodes

- `{"first_valid": [c₁, c₂, …]}` — candidates tried in order; a candidate is
  rejected if placeholder/template-arg resolution fails, the evaluation errors,
  or — with the wrapper `{"expr": "…", "min": N}` — the integer result is below
  `N`. All rejected → the field errors. The `min` wrapper gates a numeric
  field's candidate; it is not allowed for `pointer`, which stays a debugger
  value for the bridge to cast rather than an integer.
- `{"if": "<expr>", "then": v, "else": v}` — the condition is a debugger
  expression (nonzero = true); each branch is any value node, so nodes nest.
- `{"expr": "<expr>", "map": {"k": v, …}, "default": v?}` — evaluate, stringify
  the integer result (`str(int(v))`), then look it up. A miss without `default`
  errors, naming the unmapped value.

## dtype names

A `dtype` string that is a known name uses the table below; otherwise it is
evaluated as an expression whose integer result must be a valid OID code
(`0, 2, 3, 4, 5, 6`).

| Canonical | Aliases | OID code | bytes |
|---|---|---|---|
| `uint8` | `unsigned char`, `uint8_t` | 0 | 1 |
| `uint16` | `unsigned short`, `uint16_t` | 2 | 2 |
| `int16` | `short`, `int16_t` | 3 | 2 |
| `int32` | `int`, `int32_t` | 4 | 4 |
| `float32` | `float` | 5 | 4 |
| `float64` | `double` | 6 | 8 |

There is no signed 8-bit type; a value resolving to code `1` (e.g. OpenCV's
`CV_8S`) is rejected with an error naming the offending value.

## Error handling

Nothing a types file does can break your debug session. A missing file is
skipped silently; an unreadable/invalid/wrong-version file is skipped with a
warning; a malformed or under-specified entry is skipped with a warning naming
the entry, while the rest of the file still loads. At plot time, an evaluation
failure (bad expression, exhausted `first_valid`, invalid/unmapped dtype, null
pointer) is reported with the entry name, the field, and the **substituted**
expression text.

An entry carrying a key that is not in the field list above is **skipped**,
with a warning naming the offending key. This is deliberate: a misspelled
field name is not applied, so the entry would otherwise run with a default
silently substituted for the value its author intended. Use `description` for
notes — JSON has no comment syntax.

## Migrating a Python inspector

Two shipped built-ins double as worked migrations — a simple one and a
thorough one.

### cv::Mat — the simple case

The retired `cv::Mat` inspector unpacked OpenCV's packed `flags` word by hand
(channel count and pixel depth are bit-fields of it) and scaled the byte step
down to a pixel row stride:

```python
# (retired) oidtypes/opencv.py, abridged
class Mat(interface.TypeInspectorInterface):
    def get_buffer_metadata(self, obj_name, picked_obj, debugger_bridge):
        flags = int(picked_obj['flags'])
        channels = (((flags) & 4088) >> 3) + 1        # CV_MAT_CN_MASK, CV_CN_SHIFT
        type_value = flags & 7                          # already an OID dtype code
        row_stride = int(picked_obj['step']['buf'][0] / channels)
        if type_value in (UINT16, INT16):    row_stride //= 2
        elif type_value in (INT32, FLOAT32): row_stride //= 4
        elif type_value == FLOAT64:          row_stride //= 8
        ...
```

The same bit arithmetic drops straight into expressions. `{channels}` and
`{elemsize}` reuse the already-resolved fields, so the depth-dependent step
division collapses into a single term instead of the if/elif ladder:

```json
{
  "name": "cv::Mat",
  "match": "^(?:const\\s+)?cv::Mat_?(?:<[^>]{1,32}>)?(?:\\s*[*&])?$",
  "pointer": "{sym}.data",
  "width": "{sym}.cols",
  "height": "{sym}.rows",
  "channels": { "if":   "sizeof({sym}.step.p) == sizeof({sym}.step.p[0])",
                "then": "(({sym}.flags & 4088) >> 3) + 1",
                "else": "(({sym}.flags & 4064) >> 5) + 1" },
  "dtype": { "if":   "sizeof({sym}.step.p) == sizeof({sym}.step.p[0])",
             "then": "{sym}.flags & 7",
             "else": "{sym}.flags & 31" },
  "row_stride": "{sym}.step.p[0] / {channels} / {elemsize}",
  "pixel_layout": { "if": "{channels} >= 3", "then": "bgra", "else": "rgba" }
}
```

Because the masked depth already IS an OID type code (`0, 2, 3, 4, 5, 6`),
`dtype` needs no `map`; a depth with no OID code (OpenCV's 8S and 16F, and
every depth OpenCV 5 added) fails dtype validation with a typed error naming
the entry, the field and the code. The `pixel_layout` if/else picks BGRA for
color Mats and RGBA otherwise.

Both `if` conditions are the same version probe. OpenCV 5 moved the split
between the depth and channel bit-fields (bit 3, masks 7/4088, became bit 5,
masks 31/4064), and the low bits collide across versions (the word 8 means
8UC2 on OpenCV 4 and 16BFC1 on OpenCV 5), so no single mask can decode both.
Rather than asking for a version number the debuggee does not expose, the
probe reads the layout structurally: OpenCV 4's `step.p` is a `size_t*` where
OpenCV 5's is an inline `size_t[10]`, so comparing `sizeof` of the member and
its first element is true exactly on OpenCV 4. `sizeof` is evaluated
statically by the debugger, so the probe costs no debuggee memory read.

### IplImage — the thorough case

The retired `IplImage` Python inspector is a good worked example because it uses
every kind of node. The Python version read members and mapped the IPL depth
constant to an OID type:

```python
# (retired) oidtypes/opencv.py, abridged
class IplImage(interface.TypeInspectorInterface):
    types = {8: UINT8, 0x80000008: UINT8, 16: UINT16,
             0x80000010: INT16, 32: FLOAT32, 64: FLOAT64}

    def get_buffer_metadata(self, obj_name, picked_obj, debugger_bridge):
        buffer = debugger_bridge.get_casted_pointer('char',
                                                    picked_obj['imageData'])
        depth = int(picked_obj['depth'])
        row_stride = int(int(picked_obj['widthStep']) / depth * 8)
        channels = int(picked_obj['nChannels'])
        ...
```

The declarative equivalent — the shipped built-in — is:

```json
{
  "name": "IplImage",
  "match": "^(?:const\\s+)?IplImage(?:\\s*[*&])?$",
  "pointer": "{sym}.imageData",
  "width": "{sym}.width",
  "height": "{sym}.height",
  "channels": "{sym}.nChannels",
  "dtype": { "expr": "{sym}.depth & 0xffffffff",
             "map": { "8": "uint8", "2147483656": "uint8",
                      "16": "uint16", "2147483664": "int16",
                      "32": "float32", "64": "float64" } },
  "row_stride": "{sym}.widthStep / {channels} / {elemsize}",
  "pixel_layout": { "if": "{channels} >= 3", "then": "bgra", "else": "rgba" }
}
```

The `map` node replaces the Python `types` dict. `widthStep` counts bytes for a
whole row, so reaching pixels divides by `{channels}` as well as `{elemsize}`.
`{elemsize}` also replaces the hand-written `/ depth * 8`, which fixes the row
stride for signed depths, where the old arithmetic collapsed to zero.

Verify a migration against a live symbol with the conformance tool at the
debugger prompt:

```text
(gdb) python from oidscripts import conformance
(gdb) python conformance.compare('my_image')
JSON:IplImage (…/builtin_types.json) -> {'display_name': 'my_image (IplImage)', 'pointer': …, 'width': …, …}
```

It runs the symbol through every registered inspector and prints each result, so
you can see your new entry's metadata (and catch a shadowing entry that wins
ahead of it).
