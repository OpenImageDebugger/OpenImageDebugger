# gdb-imagewatch
An OpenGL based advanced buffer visualization tool for GDB.

## Features

* GUI interactivity: Scroll to zoom, left click+drag to move the buffer around.
* Buffer values: Zoom in close enough to see the numerical values of the
  buffer.
* Auto update: Whenever a breakpoint is hit, the buffer view is automatically
  updated.
* Auto contrast
* GPU accelerated
* Supported buffer types: uint8_t and float
* Supported buffer channels: Grayscale and RGB
* Supports big buffers whose dimensions exceed GL_MAX_TEXTURE_SIZE.
* Supports data structures that map to a ROI of a bigger buffer.

## Roadmap

* QtCreator integration.
* Improve user extensibility by importing modules that describe the user buffer
  data structure.
* Create a more sophisticated GUI, possibly based on Qt, allowing for more
  features to be exposed, configured and toggled at runtime.
* Show all available buffers of the user type from the current context.
* Support more data types: Short, int and double
* Support buffers with 2 channels
* Special plot types:
  * 3D topologies.
  * Histograms.

## Installation

### Dependencies

GDB-ImageWatch requires python 3+, GLEW, GLFW, Qt 4.8+ and GDB 7.10+ compiled
with python 3 support. On Ubuntu:

    sudo apt-get install libpython3-dev libglew-dev python3-numpy python3-pip
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

To build the plugin, edit the `CMakeLists.txt` file according to your needs
(the default settings should work on a Ubuntu 14.04 if all dependencies are
correctly installed), enter the `build` folder and run:

    cmake ..
    make -j4

Now edit the `~/.gdbinit` file and add: 

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
different buffer type, you need to adapt the `gdb-imagewatch.py` file to your
needs. This is actually pretty simple and only involves editing the function
`get_buffer_info()`. It must return a tuple with the following fields, in
order:

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

### Using plugin

When GDB hits a breakpoint, simply call:

    plot variable_name

[1]: http://www.glfw.org/
