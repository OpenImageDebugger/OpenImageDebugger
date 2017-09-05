#include "buffer_request_message.h"

void copyPyString(std::string& dst,
                  PyObject* src) {
    if(PyUnicode_Check(src)) {
        // Unicode sring
        PyObject *src_bytes = PyUnicode_AsEncodedString(src,
                                                        "ASCII",
                                                        "strict");
        dst = PyBytes_AS_STRING(src_bytes);
        Py_DECREF(src_bytes);
    } else {
        assert(PyBytes_Check(src));
        dst = PyBytes_AS_STRING(src);
    }
}

BufferRequestMessage::BufferRequestMessage(const BufferRequestMessage& buff) :
    py_buffer(buff.py_buffer),
    variable_name_str(buff.variable_name_str),
    display_name_str(buff.display_name_str),
    width_i(buff.width_i),
    height_i(buff.height_i),
    channels(buff.channels),
    type(buff.type),
    step(buff.step),
    pixel_layout(buff.pixel_layout)
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
                                           PyObject* pixel_layout) :
    py_buffer(pybuffer),
    width_i(buffer_width_i),
    height_i(buffer_height_i),
    channels(channels),
    type(static_cast<Buffer::BufferType>(type)),
    step(step)
{
    Py_INCREF(py_buffer);

    copyPyString(this->variable_name_str, variable_name);
    copyPyString(this->display_name_str, display_name);
    copyPyString(this->pixel_layout, pixel_layout);
}

BufferRequestMessage::~BufferRequestMessage() {
    Py_DECREF(py_buffer);
}
