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

#ifndef BUFFER_REQUEST_MESSAGE_H_
#define BUFFER_REQUEST_MESSAGE_H_

#include <string>
#include <QBuffer>
#include <QSharedMemory>

#include <Python.h>


void copy_py_string(std::string& dst, PyObject* src);

enum class BufferType {
    UnsignedByte  = 0,
    UnsignedShort = 2,
    Short         = 3,
    Int32         = 4,
    Float32       = 5,
    Float64       = 6
};

struct BufferRequestMessage
{
    std::shared_ptr<uint8_t> managed_buffer;
    std::string variable_name_str;
    std::string display_name_str;
    int width_i;
    int height_i;
    int channels;
    BufferType type;
    int step;
    std::string pixel_layout;
    bool transpose_buffer;

    BufferRequestMessage(const BufferRequestMessage& buff);

    BufferRequestMessage(PyObject* pybuffer,
                         PyObject* variable_name,
                         PyObject* display_name,
                         int buffer_width_i,
                         int buffer_height_i,
                         int channels,
                         int type,
                         int step,
                         PyObject* pixel_layout,
                         bool transpose);

    ~BufferRequestMessage();

    BufferRequestMessage() {}

    std::shared_ptr<uint8_t> make_shared_buffer_object(const QByteArray& obj);
    std::shared_ptr<uint8_t> make_float_buffer_from_double(const double* buff, int length);

    void send_buffer_plot_request(QSharedMemory& shared_memory);
    bool retrieve_buffer_plot_request(QSharedMemory& shared_memory);

    BufferRequestMessage& operator=(const BufferRequestMessage&) = delete;

    /**
     * Returns buffer width taking into account its transposition flag
     */
    int get_visualized_width() const;

    /**
     * Returns buffer height taking into account its transposition flag
     */
    int get_visualized_height() const;
};

#endif // BUFFER_REQUEST_MESSAGE_H_
