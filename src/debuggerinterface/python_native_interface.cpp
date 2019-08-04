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

#include "python_native_interface.h"


long get_py_int(PyObject* obj)
{
#if PY_MAJOR_VERSION==2
    return PyLong_AsLong(obj);
#elif PY_MAJOR_VERSION==3
    return PyLong_AS_LONG(obj);
#else
#error "Unsupported Python version"
#endif
}


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


int check_py_string_type(PyObject* obj)
{
    return PyUnicode_Check(obj) == 1 ? 1 : PyBytes_Check(obj);
}


void* get_c_ptr_from_py_buffer(PyObject* obj)
{
    assert(PyMemoryView_Check(obj));
    return PyMemoryView_GET_BUFFER(obj)->buf;
}
