# Custom buffer types via Python inspectors (legacy)

> **Prefer the declarative format.** New buffer types should be described in
> JSON — see [declarative-types.md](declarative-types.md). A JSON type works the
> same under gdb, lldb, and the VS Code extension without any Python. The Python
> inspector path documented here is still fully supported and remains the escape
> hatch for anything the declarative format cannot express, but it is no longer
> the preferred approach for new types.

If you use a different buffer type, you can create a Python parser inside the
folder `resources/oidscripts/oidtypes`. This is actually pretty simple and only
involves implementing a class according to the interface `TypeInspectorInterface`
defined in `resources/oidscripts/oidtypes/interface.py`. This interface only
defines the methods `get_buffer_metadata()` and `is_symbol_observable()`.

## `get_buffer_metadata()`

The function `get_buffer_metadata()` must return a dictionary with the following
fields:

* **display_name** Name of the buffer as it must appear in the Open Image Debugger
  window. Can be customized to also show its typename, for instance.
* **pointer** Pointer to the buffer
* **width**  Width of the ROI
* **height** Height of the ROI
* **channels** Number of color channels. Valid range is `1 <= channels <= 4`
  (grayscale through RGBA); a buffer whose channel count is outside that range
  is rejected when it is plotted.
* **type** Identifier for the type of the underlying buffer. The supported
  values, defined under `resources/oidscripts/symbols.py`, are:
  * `OID_TYPES_UINT8` = 0
  * `OID_TYPES_UINT16` = 2
  * `OID_TYPES_INT16` = 3
  * `OID_TYPES_INT32` = 4
  * `OID_TYPES_FLOAT32` = 5
  * `OID_TYPES_FLOAT64` = 6
* **row_stride** Number of pixels you have to skip in order to reach the pixel
  right below any arbitrary pixel. In other words, this can be thought of as
  the width, in pixels, of the underlying containing buffer. If the ROI is the
  total buffer size, this is the same of the buffer width.
* **pixel_layout** String describing how internal channels should be ordered
  for display purposes. The default value for buffers of 3 and 4 channels is
  `'bgra'`, and `'rgba'` for images of 1 and 2 channels. This string must
  contain exactly four characters, and each one must be one of `'r'`, `'g'`,
  `'b'` or `'a'`.  Repeated channels, such as 'rrgg' are also valid.
* **transpose_buffer** Boolean indicating whether or not to transpose the
  buffer in the interface. Can be very useful if your data structure represents
  transposition with an internal metadata.

## `is_symbol_observable()`

The function `is_symbol_observable()` receives a symbol and a string containing
the variable name, and must only return `True` if that symbol is of the
observable type (the buffer you are dealing with).

## Debugging your inspector

It is possible to debug your custom inspector methods by using the python
decorators `@interface.debug_buffer_metadata` and
`@interface.debug_symbol_observable` in the methods `get_buffer_metadata` and
`is_symbol_observable`, respectively. This will print information about all
analyzed symbols in the debugger console every time a breakpoint is hit.
