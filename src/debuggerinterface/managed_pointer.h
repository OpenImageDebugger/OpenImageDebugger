#ifndef MANAGEDPOINTER_H
#define MANAGEDPOINTER_H

#include <memory>
#include <Python.h>

std::shared_ptr<uint8_t> makeSharedPyObject(PyObject* obj);

std::shared_ptr<uint8_t> makeFloatBufferFromDouble(double* buff, int length);

#endif // MANAGEDPOINTER_H
