//-----------------------------------------------------------------------------
// Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
//
// Portions Copyright 2007-2015, Anthony Tuininga. All rights reserved.
//
// Portions Copyright 2001-2007, Computronix (Canada) Ltd., Edmonton, Alberta,
// Canada. All rights reserved.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// cxoError.c
//   Error handling.
//-----------------------------------------------------------------------------

#include "cxoModule.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
static void cxoError_free(cxoError *error);
static PyObject *cxoError_str(cxoError *error);
static PyObject *cxoError_new(PyTypeObject *type, PyObject *args,
        PyObject *keywordArgs);
static PyObject *cxoError_reduce(cxoError*);


//-----------------------------------------------------------------------------
// declaration of methods
//-----------------------------------------------------------------------------
static PyMethodDef cxoErrorMethods[] = {
    { "__reduce__", (PyCFunction) cxoError_reduce, METH_NOARGS },
    { NULL, NULL }
};


//-----------------------------------------------------------------------------
// declaration of members
//-----------------------------------------------------------------------------
static PyMemberDef cxoErrorMembers[] = {
    { "code", T_LONG, offsetof(cxoError, code), READONLY },
    { "offset", T_UINT, offsetof(cxoError, offset), READONLY },
    { "message", T_OBJECT, offsetof(cxoError, message), READONLY },
    { "context", T_OBJECT, offsetof(cxoError, context), READONLY },
    { "isrecoverable", T_BOOL, offsetof(cxoError, isRecoverable), READONLY },
    { NULL }
};


//-----------------------------------------------------------------------------
// declaration of Python type
//-----------------------------------------------------------------------------
PyTypeObject cxoPyTypeError = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "cx_Oracle._Error",                 // tp_name
    sizeof(cxoError),                   // tp_basicsize
    0,                                  // tp_itemsize
    (destructor) cxoError_free,         // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    (reprfunc) cxoError_str,            // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0,                                  // tp_doc
    0,                                  // tp_traverse
    0,                                  // tp_clear
    0,                                  // tp_richcompare
    0,                                  // tp_weaklistoffset
    0,                                  // tp_iter
    0,                                  // tp_iternext
    cxoErrorMethods,                    // tp_methods
    cxoErrorMembers,                    // tp_members
    0,                                  // tp_getset
    0,                                  // tp_base
    0,                                  // tp_dict
    0,                                  // tp_descr_get
    0,                                  // tp_descr_set
    0,                                  // tp_dictoffset
    0,                                  // tp_init
    0,                                  // tp_alloc
    cxoError_new,                       // tp_new
    0,                                  // tp_free
    0,                                  // tp_is_gc
    0                                   // tp_bases
};


//-----------------------------------------------------------------------------
// cxoError_free()
//   Deallocate the error.
//-----------------------------------------------------------------------------
static void cxoError_free(cxoError *error)
{
    Py_CLEAR(error->message);
    Py_CLEAR(error->context);
    PyObject_Del(error);
}


//-----------------------------------------------------------------------------
// cxoError_new()
//   Create a new error object. This is intended to only be used by the
// unpickling routine, and not by direct creation!
//-----------------------------------------------------------------------------
static PyObject *cxoError_new(PyTypeObject *type, PyObject *args,
        PyObject *keywordArgs)
{
    PyObject *message, *context;
    int isRecoverable, code;
    cxoError *error;
    unsigned offset;

    isRecoverable = 0;
    if (!PyArg_ParseTuple(args, "OiIO|i", &message, &code, &offset, &context,
            &isRecoverable))
        return NULL;
    error = (cxoError*) type->tp_alloc(type, 0);
    if (!error)
        return NULL;

    error->code = code;
    error->offset = offset;
    error->isRecoverable = (char) isRecoverable;
    Py_INCREF(message);
    error->message = message;
    Py_INCREF(context);
    error->context = context;

    return (PyObject*) error;
}


//-----------------------------------------------------------------------------
// cxoError_newFromInfo()
//   Internal method for creating an error object from the DPI error
// information.
//-----------------------------------------------------------------------------
cxoError *cxoError_newFromInfo(dpiErrorInfo *errorInfo)
{
    cxoError *error;

    // create error object and initialize it
    error = (cxoError*) cxoPyTypeError.tp_alloc(&cxoPyTypeError, 0);
    if (!error)
        return NULL;
    error->code = errorInfo->code;
    error->offset = errorInfo->offset;
    error->isRecoverable = (char) errorInfo->isRecoverable;

    // create message
    error->message = cxoPyString_fromEncodedString(errorInfo->message,
            errorInfo->messageLength, errorInfo->encoding, NULL);
    if (!error->message) {
        Py_DECREF(error);
        return NULL;
    }

    // create context composed of function name and action
#if PY_MAJOR_VERSION >= 3
    error->context = PyUnicode_FromFormat("%s: %s", errorInfo->fnName,
            errorInfo->action);
#else
    error->context = PyString_FromFormat("%s: %s", errorInfo->fnName,
            errorInfo->action);
#endif
    if (!error->context) {
        Py_DECREF(error);
        return NULL;
    }

    return error;
}


//-----------------------------------------------------------------------------
// cxoError_newFromString()
//   Internal method for creating an error object from the DPI error
// information.
//-----------------------------------------------------------------------------
static cxoError *cxoError_newFromString(const char *message)
{
    cxoError *error;

    error = (cxoError*) cxoPyTypeError.tp_alloc(&cxoPyTypeError, 0);
    if (!error)
        return NULL;
    Py_INCREF(Py_None);
    error->context = Py_None;
    error->message = cxoPyString_fromAscii(message);
    if (!error->message) {
        Py_DECREF(error);
        return NULL;
    }

    return error;
}


//-----------------------------------------------------------------------------
// cxoError_raiseAndReturnInt()
//   Internal method for raising an exception from an error generated from DPI.
// Return -1 as a convenience to the caller.
//-----------------------------------------------------------------------------
int cxoError_raiseAndReturnInt(void)
{
    dpiErrorInfo errorInfo;

    dpiContext_getError(cxoDpiContext, &errorInfo);
    return cxoError_raiseFromInfo(&errorInfo);
}


//-----------------------------------------------------------------------------
// cxoError_raiseAndReturnNull()
//   Internal method for raising an exception from an error generated from DPI.
// Return NULL as a convenience to the caller.
//-----------------------------------------------------------------------------
PyObject *cxoError_raiseAndReturnNull(void)
{
    cxoError_raiseAndReturnInt();
    return NULL;
}


//-----------------------------------------------------------------------------
// cxoError_raiseFromInfo()
//   Internal method for raising an exception given an error information
// structure from DPI. Return -1 as a convenience to the caller.
//-----------------------------------------------------------------------------
int cxoError_raiseFromInfo(dpiErrorInfo *errorInfo)
{
    PyObject *exceptionType;
    cxoError *error;

    error = cxoError_newFromInfo(errorInfo);
    if (!error)
        return -1;
    switch (errorInfo->code) {
        case 1:      // unique constraint violated
        case 1400:   // cannot insert NULL
        case 2290:   // check constraint violated
        case 2291:   // integrity constraint violated - parent key not found
        case 2292:   // integrity constraint violated - child record found
        case 40479:  // internal JSON serializer error
            exceptionType = cxoIntegrityErrorException;
            break;
        case 22:     // invalid session ID; access denied
        case 378:    // buffer pools cannot be created as specified
        case 600:    // internal error code
        case 602:    // internal programming exception
        case 603:    // ORACLE server session terminated by fatal error
        case 604:    // error occurred at recursive SQL level
        case 609:    // could not attach to incoming connection
        case 1012:   // not logged on
        case 1013:   // user requested cancel of current operation
        case 1033:   // ORACLE initialization or shutdown in progress
        case 1034:   // ORACLE not available
        case 1041:   // internal error. hostdef extension doesn't exist
        case 1043:   // user side memory corruption
        case 1089:   // immediate shutdown or close in progress
        case 1090:   // shutdown in progress - connection is not permitted
        case 1092:   // ORACLE instance terminated. Disconnection forced
        case 3113:   // end-of-file on communication channel
        case 3114:   // not connected to ORACLE
        case 3122:   // attempt to close ORACLE-side window on user side
        case 3135:   // connection lost contact
        case 12153:  // TNS:not connected
        case 12203:  // TNS:unable to connect to destination
        case 12500:  // TNS:listener failed to start a dedicated server process
        case 12571:  // TNS:packet writer failure
        case 27146:  // post/wait initialization failed
        case 28511:  // lost RPC connection to heterogeneous remote agent
            exceptionType = cxoOperationalErrorException;
            break;
        default:
            exceptionType = cxoDatabaseErrorException;
            break;
    }
    PyErr_SetObject(exceptionType, (PyObject*) error);
    Py_DECREF(error);
    return -1;
}


//-----------------------------------------------------------------------------
// cxoError_raiseFromString()
//   Internal method for raising an exception given an error information
// structure from DPI. Return -1 as a convenience to the caller.
//-----------------------------------------------------------------------------
PyObject *cxoError_raiseFromString(PyObject *exceptionType,
        const char *message)
{
    cxoError *error;

    error = cxoError_newFromString(message);
    if (!error)
        return NULL;
    PyErr_SetObject(exceptionType, (PyObject*) error);
    Py_DECREF(error);
    return NULL;
}


//-----------------------------------------------------------------------------
// cxoError_reduce()
//   Method provided for pickling/unpickling of Error objects.
//-----------------------------------------------------------------------------
static PyObject *cxoError_reduce(cxoError *error)
{
    return Py_BuildValue("(O(OiIO))", Py_TYPE(error), error->message,
            error->code, error->offset, error->context);
}


//-----------------------------------------------------------------------------
// cxoError_str()
//   Return a string representation of the error variable.
//-----------------------------------------------------------------------------
static PyObject *cxoError_str(cxoError *error)
{
    Py_INCREF(error->message);
    return error->message;
}

