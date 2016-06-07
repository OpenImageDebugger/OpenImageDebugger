# gdb-imagewatch
An OpenGL based advanced buffer visualization tool for GDB.

![](https://raw.githubusercontent.com/csantosbh/gdb-imagewatch/master/doc/sample_window.png)

## Features

* GUI interactivity: Scroll to zoom, left click+drag to move the buffer around.
* Buffer values: Zoom in close enough to see the numerical values of the
  buffer.
* Auto update: Whenever a breakpoint is hit, the buffer view is automatically
  updated.
* Auto contrast
* Editable contrast clamp values, useful when inspecting a buffer that contains
  uninitialized values.
* Link views together, moving all watched buffers when a single buffer is moved
  on the screen
* GPU accelerated
* Supported buffer types: uint8_t and float
* Supported buffer channels: Grayscale and RGB
* Supports big buffers whose dimensions exceed GL_MAX_TEXTURE_SIZE.
* Supports data structures that map to a ROI of a bigger buffer.

## Roadmap

* QtCreator integration.
* Show all available buffers of the user type from the current context.
* Support more data types: Short, int and double
* Support buffers with 2 channels
* Special plot types:
  * 3D topologies.
  * Histograms.
* Rotate visualized buffers
* Allow dumping buffers to a file

## Installation

### Dependencies

GDB-ImageWatch requires python 3+, Qt SDK, GLEW, GLFW, Qt 4.8+ and GDB 7.10+
(which must be compiled with python 3 support). On Ubuntu:

    sudo apt-get install libpython3-dev libglew-dev python3-numpy python3-pip qt-sdk texinfo
    sudo pip3 install pysigset

Download and install the latest GDB (if you already don't have it):

    wget http://ftp.gnu.org/gnu/gdb/gdb-7.10.tar.gz
    tar -zxvf gdb-7.10.tar.gz
    cd gdb-7.10
    ./configure --with-python=python3 --disable-werror
    make -j8
    sudo make install

Finally, download and install [GLFW3][1].

### Build plugin and configure GDB

To build this plugin, create a `build` folder, open a terminal window inside it
and run:

    qmake ..
    make -j4
    make install

The `make install` step is absolutely required, and will only copy the
dependencies to the build folder (thus, it doesn't require any special user
privileges).

Now edit the `~/.gdbinit` file and append the following line: 

    source /path/to/gdb-imagewatch/build/gdb-imagewatch.py



### Configure plugin

By default, the plugin assumes that your buffer data structure has the
following signature:

```cpp
struct Buffer {
    void* data;
    int cols; // Width
    int rows; // Height
    int flags; // OpenCV flags
    struct {
       int buf[2]; // Buf[0] = width of the containing
                   // buffer*channels; buff[1] = channels
    } step;
};
```

This is the signature found in the `Mat` type from OpenCV. If you use a
different buffer type, you need to adapt the file `resources/gdbiwtype.py` to
your needs. This is actually pretty simple and only involves editing the
functions `get_buffer_info()` and `is_symbol_observable()`.

The function `get_buffer_info()` must return a tuple with the following fields,
in order:

 * **buffer** Pointer to the buffer
 * **width**  Width of the ROI
 * **height** Height of the ROI 
 * **channels** Number of color channels (currently, only 1 and 3 are
   supported)
 * **type** Identifier for the type of the underlying buffer. The supported
   values are:
   * 0 for `float`
   * 1 for `uint8_t`
 * **step** Width, in pixels, of the underlying containing buffer. If the ROI
   is the total buffer size, this is the same of the buffer width.

The function `is_symbol_observable()` receives a gdb symbol and only returns
`True` if that symbol is of the observable type (the buffer you are dealing
with). By default, it works well with the `cv::Mat` type.

### Using plugin

When GDB hits a breakpoint, the imagewatch window will be opened. You only need
to type the name of the buffer to be watched in the "add symbols" input, and
press `<enter>`.

Alternatively, you can also invoke the imagewatch window from GDB with the
following command:

    plot variable_name

#### Configure your IDE to use GDB 7.10

If you're not using gdb from the command line, make sure that your IDE is
correctly configured to use GDB 7.10.


[1]: http://www.glfw.org/
