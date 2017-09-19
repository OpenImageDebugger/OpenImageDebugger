#ifndef PYTHON_NATIVE_INTERFACE_H_
#define PYTHON_NATIVE_INTERFACE_H_

#include <Python.h>


int getPyInt(PyObject* obj);


int checkPyStringType(PyObject* obj);


void* getCPtrFromPyBuffer(PyObject* obj);

#endif // PYTHON_NATIVE_INTERFACE_H_
