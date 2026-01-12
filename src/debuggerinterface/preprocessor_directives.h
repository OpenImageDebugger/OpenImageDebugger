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

#ifndef PREPROCESSOR_DIRECTIVES_H_
#define PREPROCESSOR_DIRECTIVES_H_

#define RAISE_PY_EXCEPTION(exception_type, msg)        \
    do {                                               \
        PyErr_SetString(exception_type, msg);          \
    } while (0)

#define CHECK_FIELD_PROVIDED_RET(name, current_ctx_name, ret)         \
    if (py_##name == nullptr) {                                       \
        RAISE_PY_EXCEPTION(                                           \
            PyExc_KeyError,                                           \
            "Missing key in dictionary provided to " current_ctx_name \
            ": Was expecting <" #name "> key");                       \
        return ret;                                                   \
    }

#define OID_EMPTY_PARAMETER
#define CHECK_FIELD_PROVIDED(name, current_ctx_name) \
    CHECK_FIELD_PROVIDED_RET(name, current_ctx_name, OID_EMPTY_PARAMETER)

#define CHECK_FIELD_TYPE(name, type_checker_funct, current_ctx_name)    \
    if (type_checker_funct(py_##name) == 0) {                           \
        RAISE_PY_EXCEPTION(                                             \
            PyExc_TypeError,                                            \
            "Key " #name " provided to " current_ctx_name " does not "  \
            "have the expected type (" #type_checker_funct " failed)"); \
        return;                                                         \
    }

#endif // PREPROCESSOR_DIRECTIVES_H_
