#ifndef BUFFERREQUESTMESSAGE_H
#define BUFFERREQUESTMESSAGE_H

#include <string>

#include <Python.h>

// TODO acho que eu nao deveria de precisar disso aqui... acho que tenho q inverter a ordem, BufferType deveria fazer parte do debugger_interface e ser utilizado dentro do component
#include "visualization/components/buffer.h"

// TODO move to PNI.h
void copyPyString(std::string& dst,
                  PyObject* src);

struct BufferRequestMessage {
    BufferRequestMessage(const BufferRequestMessage& buff);

    BufferRequestMessage(PyObject* pybuffer,
                         PyObject* variable_name,
                         PyObject* display_name,
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
    std::string variable_name_str;
    std::string display_name_str;
    int width_i;
    int height_i;
    int channels;
    Buffer::BufferType type;
    int step;
    std::string pixel_layout;
};


#endif // BUFFERREQUESTMESSAGE_H
