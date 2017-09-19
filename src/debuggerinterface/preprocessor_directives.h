#ifndef PREPROCESSOR_DIRECTIVES_H_
#define PREPROCESSOR_DIRECTIVES_H_

#define RAISE_PY_EXCEPTION(exception_type, msg)    \
    PyGILState_STATE gstate = PyGILState_Ensure(); \
    PyErr_SetString(exception_type, msg);          \
    PyGILState_Release(gstate);

#define RAISE_PY_EXCEPTION(exception_type, msg)    \
    PyGILState_STATE gstate = PyGILState_Ensure(); \
    PyErr_SetString(exception_type, msg);          \
    PyGILState_Release(gstate);

#define CHECK_FIELD_PROVIDED(name, current_ctx_name)                  \
    if (py_##name == nullptr) {                                       \
        RAISE_PY_EXCEPTION(                                           \
            PyExc_KeyError,                                           \
            "Missing key in dictionary provided to " current_ctx_name \
            ": Was expecting <" #name "> key");                       \
        return;                                                       \
    }

#define CHECK_FIELD_TYPE(name, type_checker_funct, current_ctx_name)    \
    if (type_checker_funct(py_##name) == 0) {                           \
        RAISE_PY_EXCEPTION(                                             \
            PyExc_TypeError,                                            \
            "Key " #name " provided to " current_ctx_name " does not "  \
            "have the expected type (" #type_checker_funct " failed)"); \
        return;                                                         \
    }


#define FALSE 0
#define TRUE (!FALSE)


#endif // PREPROCESSOR_DIRECTIVES_H_
