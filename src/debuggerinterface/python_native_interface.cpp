#include "python_native_interface.h"


int getPyInt(PyObject* obj)
{
    return PyLong_AS_LONG(obj);
}


int checkPyStringType(PyObject* obj)
{
    return PyUnicode_Check(obj) == 1 ? 1 : PyBytes_Check(obj);
}


void* getCPtrFromPyBuffer(PyObject* obj)
{
    assert(PyMemoryView_Check(obj));
    return PyMemoryView_GET_BUFFER(obj)->buf;
}
