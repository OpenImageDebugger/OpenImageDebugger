/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "buffer_request_message.h"


void copy_py_string(std::string& dst, PyObject* src)
{
    if (PyUnicode_Check(src)) {
        // Unicode sring
        PyObject* src_bytes = PyUnicode_AsEncodedString(src, "ASCII", "strict");
        dst                 = PyBytes_AS_STRING(src_bytes);
        Py_DECREF(src_bytes);
    } else {
        assert(PyBytes_Check(src));
        dst = PyBytes_AS_STRING(src);
    }
}


BufferRequestMessage::BufferRequestMessage(const BufferRequestMessage& buff)
    : py_buffer(buff.py_buffer)
    , variable_name_str(buff.variable_name_str)
    , display_name_str(buff.display_name_str)
    , width_i(buff.width_i)
    , height_i(buff.height_i)
    , channels(buff.channels)
    , type(buff.type)
    , step(buff.step)
    , pixel_layout(buff.pixel_layout)
    , transpose_buffer(buff.transpose_buffer)
{
    Py_INCREF(py_buffer);
}


BufferRequestMessage::BufferRequestMessage(PyObject* pybuffer,
                                           PyObject* variable_name,
                                           PyObject* display_name,
                                           int buffer_width_i,
                                           int buffer_height_i,
                                           int channels,
                                           int type,
                                           int step,
                                           PyObject* pixel_layout,
                                           bool transpose)
    : py_buffer(pybuffer)
    , width_i(buffer_width_i)
    , height_i(buffer_height_i)
    , channels(channels)
    , type(static_cast<Buffer::BufferType>(type))
    , step(step)
    , transpose_buffer(transpose)
{
    Py_INCREF(py_buffer);

    copy_py_string(this->variable_name_str, variable_name);
    copy_py_string(this->display_name_str, display_name);
    copy_py_string(this->pixel_layout, pixel_layout);
}


BufferRequestMessage::~BufferRequestMessage()
{
    Py_DECREF(py_buffer);
}
