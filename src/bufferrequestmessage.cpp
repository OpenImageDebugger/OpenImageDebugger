#include "bufferrequestmessage.h"

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
    var_name_str(buff.var_name_str),
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
                                           PyObject* var_name,
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

    copyPyString(this->var_name_str, var_name);
    copyPyString(this->pixel_layout, pixel_layout);
}

BufferRequestMessage::~BufferRequestMessage() {
    Py_DECREF(py_buffer);
}
