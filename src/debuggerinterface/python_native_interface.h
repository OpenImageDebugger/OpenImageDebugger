/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 OpenImageDebugger contributors
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

#ifndef PYTHON_NATIVE_INTERFACE_H_
#define PYTHON_NATIVE_INTERFACE_H_

#include <string>

#include <Python.h>


#if PY_MAJOR_VERSION==2
#define PY_INT_CHECK_FUNC PyInt_Check
#else
#define PY_INT_CHECK_FUNC PyLong_Check
#endif


long get_py_int(PyObject* obj);


int check_py_string_type(PyObject* obj);


void get_c_ptr_from_py_buffer(PyObject* obj, uint8_t*& buffer_ptr, size_t& buffer_size);


uint8_t* get_c_ptr_from_py_tuple(PyObject* obj, int tuple_index);


void copy_py_string(std::string& dst, PyObject* src);

#endif // PYTHON_NATIVE_INTERFACE_H_
