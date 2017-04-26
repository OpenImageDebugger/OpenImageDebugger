#include <cstring>
#include "managed_pointer.h"

using namespace std;

shared_ptr<uint8_t> makeSharedPyObject(PyObject* obj) {
    return shared_ptr<uint8_t>(reinterpret_cast<uint8_t*>(obj),
                               [](uint8_t* obj) {
        Py_DECREF(reinterpret_cast<PyObject*>(obj));
    });
}

shared_ptr<uint8_t> makeFloatBufferFromDouble(double* buff, int length) {
    shared_ptr<uint8_t> result(reinterpret_cast<uint8_t*>(new float[length]),
                               [](uint8_t* buff) {
        delete[] reinterpret_cast<float*>(buff);
    });

    // Cast from double to float
    float* dst = reinterpret_cast<float*>(result.get());
    for(int i = 0; i < length; ++i) {
        dst[i] = static_cast<float>(buff[i]);
    }

    return result;
}
