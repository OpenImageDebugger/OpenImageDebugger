/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
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

namespace oid
{

long get_py_int(PyObject* obj)
{
#if PY_MAJOR_VERSION == 3
    return PyLong_AS_LONG(obj);
#else
#error "Unsupported Python version"
#endif
}

uint8_t* get_c_ptr_from_py_tuple(PyObject* obj, const int tuple_index)
{
    PyObject* tuple_item = PyTuple_GetItem(obj, tuple_index);
    return static_cast<uint8_t*>(PyLong_AsVoidPtr(tuple_item));
}

void copy_py_string(std::string& dst, PyObject* src)
{
    if (PyUnicode_Check(src)) {
        const char* utf8_str = PyUnicode_AsUTF8(src);
        dst                  = utf8_str != nullptr ? utf8_str : "";
    } else {
        assert(PyBytes_Check(src));
        dst = PyBytes_AS_STRING(src);
    }
}

int check_py_string_type(PyObject* obj)
{
    return PyUnicode_Check(obj) == 1 ? 1 : PyBytes_Check(obj);
}

void get_c_ptr_from_py_buffer(PyObject* obj,
                              uint8_t*& buffer_ptr,
                              size_t& buffer_size)
{
    assert(PyMemoryView_Check(obj));
    const auto py_buff = PyMemoryView_GET_BUFFER(obj);
    buffer_ptr         = static_cast<uint8_t*>(py_buff->buf);
    buffer_size        = static_cast<size_t>(py_buff->len);
}

} // namespace oid
