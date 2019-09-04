[![Build Status](https://cloud.drone.io/api/badges/OpenImageDebugger/OpenImageDebugger/status.svg)](https://cloud.drone.io/OpenImageDebugger/OpenImageDebugger)
[![Build Status](https://travis-ci.com/OpenImageDebugger/OpenImageDebugger.svg?branch=wip%2Fdarwin-support)](https://travis-ci.com/OpenImageDebugger/OpenImageDebugger)

# Open Image Debugger: Enabling visualization of in-memory buffers on GDB/LLDB
Open Image Debugger is a tool for visualizing in-memory buffers during debug
sessions, compatible with both GDB and LLDB. It works out of the box with
instances of the OpenCV `Mat` class and `Eigen` matrices, but can also be
customized to work with any arbitrary data structure.

![](doc/sample_window.png)

### Features

* GUI interactivity:
    * Scroll to zoom, left click+drag to move the buffer around;
    * Rotate buffers 90&deg; clockwise or counterclockwise;
    * Go-to widget that quickly takes you to any arbitrary pixel location;
* Buffer values: Zoom in close enough to inspect the numerical contents of any pixel.
* Auto update: Whenever a breakpoint is hit, the buffer view is automatically
  updated.
* Auto contrast: The entire range of values present in the buffer can be
  automatically mapped to the visualization range `[0, 1]`, where `0`
  represents black and `1` represents white.
* The contrast range can be manually adjusted, which is useful for inspecting
  buffers with extreme values (e.g. infinity, nan and other outliers).
* Link views together, moving all watched buffers simultaneously when any
  single buffer is moved on the screen
* Supported buffer types: uint8_t, int16_t, uint16_t, int32_t, uint32_t,
  float and double
* Supported buffer channels: Up to four channels (Grayscale, two-channels, RGB
  and RGBA)
* GPU accelerated
* Supports large buffers whose dimensions exceed GL_MAX_TEXTURE_SIZE.
* Supports data structures that map to a ROI of a larger buffer.
* Exports buffers as png images (with auto contrast) or octave/matlab matrix
  files (unprocessed).
* Auto-load buffers being visualized in the previous debug session
* Designed to scale well for HighDPI displays
* Works on Linux and macOS X

### Requirements

 * A C++11 compliant compiler (gcc-5 or later is recommended)
 * GDB **7.10+** or LLDB **6+**
 * Qt **5.6+** (required due to the HighDPI display support - download it
   [here](https://info.qt.io/download-qt-for-application-development))
 * Python development packages
 * OpenGL 2.1+ support

## Installation

### Ubuntu Linux dependencies

On Ubuntu, you can install most of the dependencies with the following command:

```shell
sudo apt-get install build-essential libpython3-dev python3-dev libpython2.7-dev python2-dev
```

### Building the Open Image Debugger
Clone the source code to any folder you prefer and initialize the
submodules:

```shell
git clone https://github.com/OpenImageDebugger/OpenImageDebugger.git
cd OpenImageDebugger
git submodule init
git submodule update
```

Now run the following commands to build it:

```shell
mkdir build && cd build
qmake .. BUILD_MODE=release PREFIX=/path/to/installation/folder
make -j4
sudo make install
```

**GDB integration:** Edit the file `~/.gdbinit` (create it if it doesn't exist)
and append the following line:
```
source /path/to/OpenImageDebugger/oid.py
```

**LLDB integration:** Edit the file `~/.lldbinit` (create it if it doesn't
exist) and append the following line:
```
command script import /path/to/OpenImageDebugger/oid.py
```

### Ubuntu 16.04 Automated Installation

The script `configure_ubuntu_16.sh` automates some of the installation steps
for Open Image Debugger on Ubuntu 16.04.  From the root directory of this project,
execute the following:

```shell
bash configure_ubuntu_16.sh
```

Note that this script will not instal Qt 5.6+. If you don't have this
package, the best choice is to get it from Qt website.

When you are done, follow the instructions below in the section `Testing your
installation`.

### Testing your installation

After compiling the plugin, you can test it by running the following command:

```shell
python /path/to/OpenImageDebugger/oid.py --test
```

If the installation was succesful, you should see the Open Image Debugger window
with the buffers `sample_buffer_1` and `sample_buffer_2`.

## Troubleshooting

### QtCreator configuration
If you are using QtCreator, you can change your Qt version under
Tools->Options->Build & Run->Kits. Make sure you have Qt >= 5.6 selected.

### libGL linking issues on Linux
Some linux users might experience a linking error if the libGL.so is not
found by qmake, especially when using a nvidia graphics card. This issue will
usually present itself with the message `cannot find -lGL`.

To fix that, you need to find the location for your libGL.so file. The
following commands should help you find it:

```shell
sudo updatedb
locate -i libgl.so
```

If you have installed the proprietary drivers, you don't want to use the mesa
folder. For example, running the command above could result in the following
output:

```
/usr/lib/nvidia-384/libGL.so
/usr/lib/nvidia-384/libGL.so.1
/usr/lib/nvidia-384/libGL.so.384.90
/usr/lib/x86_64-linux-gnu/mesa/libGL.so.1
/usr/lib/x86_64-linux-gnu/mesa/libGL.so.1.2.0
/usr/lib32/nvidia-384/libGL.so
/usr/lib32/nvidia-384/libGL.so.1
/usr/lib32/nvidia-384/libGL.so.384.90
```

In this case, since I'm using the proprietary nvidia drivers, I'll choose the
folder `/usr/lib/nvidia-384`.

Copy the name of the folder you found, and paste it in the file
`oid.pro` under the definition of the `QMAKE_LFLAGS` variable. In my
case, this variable now looks like this:

```
QMAKE_LFLAGS += \
  # If you have an error "cannot find -lGL", uncomment the following line and
  # replace the folder by the location of your libGL.so
  -L/usr/lib/nvidia-384/ \
  -Wl,--exclude-libs,ALL
```

Now, just run the build steps again, including the `qmake` command, as
described above.

## Using plugin

When the debugger hits a breakpoint, the Open Image Debugger window will be
opened. You only need to type the name of the buffer to be watched in the
"add symbols" input, and press `<enter>`.

### <img src="doc/auto-contrast.svg" width="20"/> Auto-contrast and manual contrast

The (min) and (max) fields on top of the buffer view can be changed to control
autocontrast settings. By default, Open Image Debugger will automatically fill these
fields with the mininum and maximum values inside the entire buffer, and the
channel values will be normalized from these values to the range [0, 1] inside
the renderer.

Sometimes, your buffer may contain trash, uninitialized values that are either
too large or too small, making the entire image look flat because of this
normalization. If you know the expected range for your image, you can manually
change the (min) and (max) values to focus on the range that you are
interested.

### <img src="doc/link-views.svg" width="20"/> Locking buffers

Sometimes you want to compare two buffers being visualized, and need to zoom in
different places of these buffers. If they are large enough, this can become a
very hard task, especially if you are comparing pixel values. This task is made
easier by the `lock buffers` tool (which is toggled by the button with a chain
icon).

When it is activated, all buffers are moved/zoomed simultaneously by the same
amount. This means you only need to align the buffers being compared once;
after activating the `lock buffers` mode, you can zoom in anywhere you wish in
one buffer that all other buffers will be zoomed in the same location.

### <img src="doc/location.svg" width="20"/> Quickly moving to arbitrary coordinates

If you need to quickly move to any pixel location, then the *go to*
functionality is what you are looking for. Press *Ctrl+L* and two input fields
corresponding to the target destination in format `<x, y>` will appear at the
bottom right corner of the buffer screen. Type the desired location, then press
enter to quickly zoom into that location.

### Exporting bufers

Sometimes you may want to export your buffers to be able to process them in an
external tool. In order to do that, right click the thumbnail corresponding to
the buffer you wish to export on the left pane and select "export buffer".

Open Image Debugger supports two export modes. You can save your buffer as a PNG
(which may result in loss of data if your buffer type is not `uint8_t`) or as a
binary file that can be opened with any tool.

### Loading exported buffers on Octave/Matlab

Buffers exported in the `Octave matrix` format can be loaded with the function
`giw_load.m`, which is available in the `matlab` folder. To use it, add this
folder to Octave/Matlab `path` variable and call
`giw_load('/path/to/buffer.dump')`.

## Basic configuration

The settings file for the plugin can be located under
`$HOME/.config/OpenImageDebugger.ini`. You can change the following settings:

 * **Rendering**
    * *maximum_framerate* Determines the maximum framerate for the buffer
    rendering backend. Must be greater than 0.

## Advanced configuration

By default, the plugin works with several data types, including OpenCV's `Mat`
and `CvMat` and Eigen's `Matrix`.

If you use a different buffer type, you can create a python parser inside the
folder `resources/giwscripts/giwtypes`. This is actually pretty simple and
only involves implementing a class according to the interface
`TypeInspectorInterface` defined in
`resources/giwscripts/giwtypes/interface.py`. This interface only defines the
methods `get_buffer_metadata()` and `is_symbol_observable()`.

The function `get_buffer_metadata()` must return a dictionary with the following
fields:

 * **display_name** Name of the buffer as it must appear in the Open Image Debugger
   window. Can be customized to also show its typename, for instance.
 * **pointer** Pointer to the buffer
 * **width**  Width of the ROI
 * **height** Height of the ROI
 * **channels** Number of color channels (Must be in the range `1 <= channels
   <= 3`)
 * **type** Identifier for the type of the underlying buffer. The supported
   values, defined under `resources/giwscripts/symbols.py`, are:
   * `GIW_TYPES_UINT8` = 0
   * `GIW_TYPES_UINT16` = 2
   * `GIW_TYPES_INT16` = 3
   * `GIW_TYPES_INT32` = 4
   * `GIW_TYPES_FLOAT32` = 5
   * `GIW_TYPES_FLOAT64` = 6
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

The function `is_symbol_observable()` receives a symbol and a string
containing the variable name, and must only return `True` if that symbol is of
the observable type (the buffer you are dealing with).

It is possible to debug your custom inspector methods by using the python
decorators `@interface.debug_buffer_metadata` and
`@interface.debug_symbol_observable` in the methods `get_buffer_metadata` and
`is_symbol_observable`, respectively. This will print information about all
analyzed symbols in the debugger console every time a breakpoint is hit.
