#ifndef PREPROCESSOR_DIRECTIVES_H_
#define PREPROCESSOR_DIRECTIVES_H_

#define RAISE_PY_EXCEPTION(exception_type, msg) \
    PyGILState_STATE gstate = PyGILState_Ensure(); \
    PyErr_SetString(exception_type, msg); \
    PyGILState_Release(gstate);

#endif // PREPROCESSOR_DIRECTIVES_H_
