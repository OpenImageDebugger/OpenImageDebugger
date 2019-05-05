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

#include <iostream>

#include <QDataStream>

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
    : managed_buffer(buff.managed_buffer)
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
    : width_i(buffer_width_i)
    , height_i(buffer_height_i)
    , channels(channels)
    , type(static_cast<BufferType>(type))
    , step(step)
    , transpose_buffer(transpose)
{
    copy_py_string(this->variable_name_str, variable_name);
    copy_py_string(this->display_name_str, display_name);
    copy_py_string(this->pixel_layout, pixel_layout);
}


BufferRequestMessage::~BufferRequestMessage()
{
}

std::shared_ptr<uint8_t> BufferRequestMessage::make_shared_buffer_object(const QByteArray& obj)
{
    const int length = obj.size();
    uint8_t* copied_buffer = new uint8_t[length];
    for(int i = 0; i < length; ++i) {
        copied_buffer[i] = static_cast<uint8_t>(obj.constData()[i]);
    }

    return std::shared_ptr<uint8_t>(
        copied_buffer, [](uint8_t* obj) { delete[] obj; });
}


std::shared_ptr<uint8_t> BufferRequestMessage::make_float_buffer_from_double(const double* buff, int length)
{
    std::shared_ptr<uint8_t> result(
        reinterpret_cast<uint8_t*>(new float[length]),
        [](uint8_t* buff) { delete[] reinterpret_cast<float*>(buff); });

    // Cast from double to float
    float* dst = reinterpret_cast<float*>(result.get());
    for (int i = 0; i < length; ++i) {
        dst[i] = static_cast<float>(buff[i]);
    }

    return result;
}

void BufferRequestMessage::send_buffer_plot_request(QSharedMemory& shared_memory)
{
    // TODO write
}

bool BufferRequestMessage::retrieve_buffer_plot_request(QSharedMemory &shared_memory)
{
    QBuffer buffer;
    QDataStream in(&buffer);

    if(!shared_memory.attach()) {
        // TODO display error message maybe?
        //std::cerr << "[Error] Could not attach to shared memory" << std::endl;
        return false;
    }

    bool status = false;

    shared_memory.lock();
    buffer.setData(static_cast<const char*>(shared_memory.constData()), shared_memory.size());
    buffer.open(QBuffer::ReadOnly);
    qint8 qtype;
    QByteArray buffer_content;

    in >> qtype;
    type = static_cast<BufferType>(qtype);
    // TODO fill remaining fields

    shared_memory.unlock();
    shared_memory.detach();

    if (type == BufferType::Float64) {
        managed_buffer = make_float_buffer_from_double(
            reinterpret_cast<const double*>(buffer_content.constData()),
            width_i * height_i * channels);
    } else {
        managed_buffer = make_shared_buffer_object(buffer_content.constData());
    }

    return status;
}


int BufferRequestMessage::get_visualized_width() const
{
    if (!transpose_buffer) {
        return width_i;
    } else {
        return height_i;
    }
}


int BufferRequestMessage::get_visualized_height() const
{
    if (!transpose_buffer) {
        return height_i;
    } else {
        return width_i;
    }
}
