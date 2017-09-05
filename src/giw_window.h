#ifndef GIW_WINDOW_H_
#define GIW_WINDOW_H_

#include <Python.h>

#ifndef GIW_API
#if __GNUC__ >= 4
#    define GIW_API __attribute__((visibility("default")))
#else
#    define GIW_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AppHandler;
typedef void* WindowHandler;

/**
 * Initialize GIW window
 *
 * The thread in which this function will be called will become the GUI thread;
 * all later functions (with exception of giw_destroy_window()) must be called
 * in the same thread.
 *
 * @return  Application context
 */
GIW_API
AppHandler giw_initialize();

GIW_API
void giw_cleanup(AppHandler handler);

GIW_API
void giw_terminate();

/**
 * Execute GUI loop
 *
 * This methods blocks the caller until the GIW window is closed. If
 * non-blocking behavior is desired, it must be called from a dedicated thread.
 * Notice that it must also be called in the same thread as giw_initialize().
 *
 * @param handler  Application context
 */
GIW_API
void giw_exec(AppHandler handler);

/**
 * Create and open new window
 *
 * Must be called after giw_initialize(), in the same thread as it was called.
 *
 * @param plot_callback  Callback function to be called when the user requests
 *     a symbol name from the giw window
 *
 */
GIW_API
WindowHandler giw_create_window(int(*plot_callback)(const char*));

GIW_API
int giw_is_window_ready(WindowHandler handler);

GIW_API
void giw_destroy_window(WindowHandler handler);

GIW_API
PyObject* giw_get_observed_buffers(WindowHandler handler);

GIW_API
void giw_set_available_symbols(WindowHandler handler,
                               PyObject* available_set);

/**
 * Add a buffer to the plot list
 *
 * @param handler  Handler of the window where the buffer should be plotted
 * @param buffer_metadata  Python dictionary with the following elements:
 *     - [pointer     ] PyMemoryView object wrapping the target buffer
 *     - [display_name] Variable name as it shall be displayed
 *     - [width       ] Buffer width, in pixels
 *     - [height      ] Buffer height, in pixels
 *     - [channels    ] Number of channels (1 to 4)
 *     - [type        ] Buffer type (see symbols.py for details)
 *     - [row_stride  ] Row stride, in pixels
 *     - [pixel_layout] String defining pixel channel layout (e.g. 'rgba')
 * */
GIW_API
void giw_plot_buffer(WindowHandler handler,
                     PyObject* bufffer_metadata);

#ifdef __cplusplus
}
#endif

#endif
