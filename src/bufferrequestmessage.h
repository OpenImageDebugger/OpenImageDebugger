#ifndef BUFFERREQUESTMESSAGE_H
#define BUFFERREQUESTMESSAGE_H

#include <string>

#include "buffer.hpp"

void copyPyString(std::string& dst,
                  PyObject* src);

struct BufferRequestMessage {
    BufferRequestMessage(const BufferRequestMessage& buff);

    BufferRequestMessage(PyObject* pybuffer,
                         PyObject* var_name,
                         int buffer_width_i,
                         int buffer_height_i,
                         int channels,
                         int type,
                         int step,
                         PyObject* pixel_layout);

    ~BufferRequestMessage();

    BufferRequestMessage() = delete;
    BufferRequestMessage& operator=(const BufferRequestMessage&) = delete;

    PyObject* py_buffer;
    std::string var_name_str;
    int width_i;
    int height_i;
    int channels;
    Buffer::BufferType type;
    int step;
    std::string pixel_layout;
};


#endif // BUFFERREQUESTMESSAGE_H
