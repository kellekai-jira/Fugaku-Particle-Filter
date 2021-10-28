#ifndef _MY_PYTHONINTERFACE_H_
#define _MY_PYTHONINTERFACE_H_

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <csignal>
#include <cstdlib>

#include "utils.h"
#include "melissa_da_stype.h"
#include "api_common.h"

class PythonInterface {

private:
    PyObject *pFunc = nullptr;
    PyObject *pModule = nullptr;
    wchar_t *program = nullptr;
    PyObject *pBackground = nullptr;
    PyObject *pHidden = nullptr;

    PyObject *pArray_assimilated_index = nullptr;
    PyObject *pArray_assimilated_varid = nullptr;

public:
    PythonInterface() {

        // This works only in python 3.8 and higher:
        //    PyConfig config;
        //    PyConfig_InitPythonConfig(&config);

        //    config.buffered_stdio = 0;  // equals to python -u

        //    PyStatus status = Py_InitializeFromConfig(&config);
        //    err(!PyStatus_Exception(status), "Could not init python");
        //    So we do the workaround: sys.stdout.reconfigure(line_buffering=True)
        PyObject *pName = nullptr;

        if (!Py_IsInitialized())
        {
            MDBG("Initing Python");

            program = Py_DecodeLocale("melissa_da_api", NULL);
            Py_SetProgramName(program);
            Py_Initialize();
            _import_array();  // init numpy
        } else {
            MDBG("Not initing python as runner probably a python program");
        }


        if (NPY_VERSION != PyArray_GetNDArrayCVersion())
        {
            MERR(
                    "Error! Numpy version conflict that might lead to undefined behavior. Recompile numpy!");
        }

        // workaround for unbuffered stdout/ stderr (working also with > python3.7 ... < python3.8):
        PyRun_SimpleString("import sys");
        PyRun_SimpleString(
                "if sys.version_info < (3,7):\n    print('Please use a newer Python version (>3.7) for more verbose error log')\nelse:\n    sys.stdout.reconfigure(line_buffering=True)\n    sys.stderr.reconfigure(line_buffering=True)");

        char *module_name = getenv("MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE");
        if (!module_name)
        {
            MPRT("MELISSA_DA_PYTHON_ASSIMILATOR_MODULE not set! exiting now");
            exit(EXIT_FAILURE);
        }

        pName = PyUnicode_DecodeFSDefault(module_name);
        err(pName != NULL, "Invalid module name");

        pModule = PyImport_Import(pName);
        Py_DECREF(pName);

        MDBG("looking for module %s...",module_name);

        err(pModule != NULL,
                "Cannot find the module file. Is its path in PYTHONPATH?");                   // Could not find module
        pFunc = PyObject_GetAttrString(pModule, "calculate_weight");

        err(pFunc && PyCallable_Check(pFunc),
                "Could not find callable calculate_weight function");

        if (field.local_vect_size >= LONG_MAX)
        {
            MERR("too large vectsize for python assimilator");
        }


        npy_intp dims_index_map[1] =
        {static_cast<npy_intp>(local_index_map.size())};
        // std::cout << "dims_index_map=" << dims_index_map[0] << std::endl;

        // jump over second int. See https://stackoverflow.com/questions/53097952/how-to-understand-numpy-strides-for-layman
        npy_intp strides[1] = {static_cast<npy_intp>(2*sizeof(int))};

        // assert(PyArray_CheckStrides(sizeof(int), 1, 2*sizeof(int), dims_index_map, strides));

        // FIXME: assuming memory is aligned linear for index_map_s struct....
        pArray_assimilated_index =
            PyArray_New(&PyArray_Type, 1, dims_index_map, NPY_INT32, strides,
                    local_index_map.data(), 0, 0, NULL);
        pArray_assimilated_varid =
            PyArray_New(&PyArray_Type, 1, dims_index_map, NPY_INT32, strides,
                    reinterpret_cast<char*>(local_index_map.data()) +
                    sizeof(int), 0, 0, NULL);
        assert(sizeof(index_map_t) == 2*sizeof(int));
    }

    ~PythonInterface() {

        Py_DECREF(pFunc);
        Py_DECREF(pModule);

        // this will also decref list members.
        Py_DECREF(pBackground);
        Py_DECREF(pHidden);

        PyMem_RawFree(program);

        Py_Finalize();
        MDBG("Freed python context.");
    }

    void err(bool no_fail, const char * error_str) {
        if (!no_fail || PyErr_Occurred())
        {
            MPRT("Error! %s", error_str);
            PyErr_Print();
            std::raise(SIGINT);
            exit(1);
        }
    }

    double calculate_weight(const int current_step, const int current_state_id, VEC_T *values,
            VEC_T *hidden_values) {

        // REM: in theory the python callback may manipulate the state! but it SHOULDN'T !

        static bool is_first_time = true;

        if (is_first_time) {
            is_first_time = false;
            // init fields
            npy_intp dims[1] = { static_cast<npy_intp>(field.local_vect_size) };
            npy_intp dims_hidden[1] =
            { static_cast<npy_intp>(field.local_hidden_vect_size) };
            MPRT("Creating Python object for background state");

            pBackground = PyArray_SimpleNewFromData(1, dims, NPY_UINT8,
                    values);
            err(pBackground != NULL, "Cannot generate numpy array with dims");

            pHidden = PyArray_SimpleNewFromData(1, dims_hidden, NPY_UINT8,
                    hidden_values);
            err(pHidden != NULL, "Cannot generate numpy array with dims_hidden");
        }


        PyObject *pTime = Py_BuildValue("i", current_step);
        PyObject *pId = Py_BuildValue("i", current_state_id);

        err(pTime != NULL, "Cannot create argument");
        err(pId != NULL, "Cannot create argument");

        MDBG("calculate_weight input parameter:");

        // Py_INCREF(pValue);
        MPI_Fint fcomm = MPI_Comm_c2f(comm);
        PyObject *pFcomm = Py_BuildValue("i", fcomm);

        PyObject * pReturn = PyObject_CallFunctionObjArgs(pFunc, pTime, pId,
                pBackground,
                pHidden,
                pArray_assimilated_index,
                pArray_assimilated_varid,
                pFcomm,
                NULL);
        err(pReturn != NULL, "No return value");


        double weight = PyFloat_AsDouble(pReturn);

        MDBG("Back from calculate_weight: %f", weight);

        Py_DECREF(pReturn);

        return weight;
    }
};

#endif
