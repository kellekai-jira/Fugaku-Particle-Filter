/*
 * PythonAssimilator.cxx
 *
 *  Created on: Jan 14, 2021
 *      Author: friese
 */

#include "PythonAssimilator.h"
#include <algorithm>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <csignal>

#include <cstdlib>

PythonAssimilator::PythonAssimilator(Field & field_, const int total_steps_, MpiManager & mpi_) :
    field(field_), total_steps(total_steps_), mpi(mpi_)
{
    nsteps = 1;

    py::init(field);
}

void PythonAssimilator::on_init_state(const int runner_id, const
                                              Part & part, const
                                              VEC_T * values, const
                                              Part & hidden_part,
                                              const VEC_T * values_hidden)
{
    static bool is_first = true;

    assert(runner_id == 0);  // be sure to collect data only from one runner!

    // let's use this to set the init.
    // you may not have more runners than ensemble members here! Otherwise some
    // would stay uninitialized!

    // For now we copy the first received ensemble state everywhere.... I know this is a rather stupid way to init the ensemble!
    // TODO: later we should at least perturb all members a bit using the index map and so on...
    for (auto & member : field.ensemble_members) {
        member.store_background_state_part(part,
                                    values, hidden_part, values_hidden);

        assert(part.send_count + part.local_offset_server <=
               member.state_background.size());

        // copy into analysis state to send it back right again!
        std::copy(values, values + part.send_count,
                  member.state_analysis.data() +
                  part.local_offset_server);

        // copy into field's hidden state to send it back right again!
        std::copy(values_hidden, values_hidden + hidden_part.send_count,
                  member.state_hidden.data() +
                  hidden_part.local_offset_server);
    }
}

int PythonAssimilator::do_update_step(const int current_step) {
    L("Doing python update step...\n");
    MPI_Barrier(mpi.comm());

    py::callback(current_step);

    if (current_step >= total_steps)
    {
        return -1;
    }
    else
    {
        return getNSteps();
    }
}

PythonAssimilator::~PythonAssimilator() {
    py::finalize();

    // even after calling Py_DECREF on the numpy arrays the vectors's content is still in
    // memory.
}

void py::init(Field & field) {
    PyObject *pName = NULL;

    program = Py_DecodeLocale("melissa_server", NULL);
    Py_SetProgramName(program);

// This works only in python 3.8 and higher:
//    PyConfig config;
//    PyConfig_InitPythonConfig(&config);

//    config.buffered_stdio = 0;  // equals to python -u

//    PyStatus status = Py_InitializeFromConfig(&config);
//    err(!PyStatus_Exception(status), "Could not init python");
//    So we do the workaround: sys.stdout.reconfigure(line_buffering=True)

    Py_Initialize();

    _import_array();  // init numpy

    if (NPY_VERSION != PyArray_GetNDArrayCVersion()) {
        E("Error! Numpy version conflict that might lead to undefined behavior. Recompile numpy!");
    }

    // workaround for unbuffered stdout/ stderr (working also with > python3.7 ... < python3.8):
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("if sys.version_info < (3,7):\n    print('Please use a newer Python version (>3.7) for more verbose error log')\nelse:\n    sys.stdout.reconfigure(line_buffering=True)\n    sys.stderr.reconfigure(line_buffering=True)");

    char *module_name = getenv("MELISSA_DA_PYTHON_ASSIMILATOR_MODULE");
    if (!module_name) {
        L("MELISSA_DA_PYTHON_ASSIMILATOR_MODULE not set! exiting now");
        exit(EXIT_FAILURE);
    }

    pName = PyUnicode_DecodeFSDefault(module_name);
    err(pName != NULL, "Invalid module name");

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    err(pModule != NULL, "Cannot find the module file. Is its path in PYTHONPATH?");  // Could not find module
    pFunc = PyObject_GetAttrString(pModule, "callback");

    err(pFunc && PyCallable_Check(pFunc), "Could not find callable callback function");

    if (field.local_vect_size >= LONG_MAX) {
        E("too large vectsize for python assimilator");
    }

    // init list:
    L("Creating Python object for background states");

    pEnsemble_list_background = PyList_New(field.ensemble_members.size());
    err(pEnsemble_list_background != NULL, "Cannot create background state list");

    pEnsemble_list_analysis = PyList_New(field.ensemble_members.size());
    err(pEnsemble_list_analysis != NULL, "Cannot create analysis state list");

    pEnsemble_list_hidden_inout = PyList_New(field.ensemble_members.size());
    err(pEnsemble_list_hidden_inout != NULL, "Cannot create analysis state list");

    npy_intp dims[1] = { static_cast<npy_intp>(field.local_vect_size) };
    npy_intp dims_hidden[1] = { static_cast<npy_intp>(field.local_vect_size_hidden) };

    int i = 0;
    for (auto &member : field.ensemble_members) {
        PyObject *pBackground = PyArray_SimpleNewFromData(1, dims, NPY_UINT8,
                member.state_background.data());
        err(pBackground != NULL, "Cannot generate numpy array with dims");
        PyList_SetItem(pEnsemble_list_background, i, pBackground);

        PyObject *pAnalysis = PyArray_SimpleNewFromData(1, dims, NPY_UINT8,
                member.state_analysis.data());
        err(pAnalysis != NULL, "Cannot generate numpy array with dims");
        PyList_SetItem(pEnsemble_list_analysis, i, pAnalysis);

        PyObject *pHidden = PyArray_SimpleNewFromData(1, dims_hidden, NPY_UINT8,
                member.state_hidden.data());
        err(pHidden != NULL, "Cannot generate numpy array with dims_hidden");
        PyList_SetItem(pEnsemble_list_hidden_inout, i, pHidden);

        // PyList_SetItem steals the reference so no need to call Py_DECREF

        ++i;
    }

    npy_intp dims_index_map[1] = {static_cast<npy_intp>(field.local_index_map.size())};
    std::cout << "dims_index_map=" << dims_index_map[0] << std::endl;
    // jump over second int. See https://stackoverflow.com/questions/53097952/how-to-understand-numpy-strides-for-layman
    npy_intp strides[1] = {static_cast<npy_intp>(2*sizeof(int))};

    //assert(PyArray_CheckStrides(sizeof(int), 1, 2*sizeof(int), dims_index_map, strides));

    // FIXME: assuming memory is aligned linear for index_map_s struct....
    pArray_assimilated_index =
        PyArray_New(&PyArray_Type, 1, dims_index_map, NPY_INT32, strides,
                field.local_index_map.data(), 0, 0, NULL);
    pArray_assimilated_varid =
        PyArray_New(&PyArray_Type, 1, dims_index_map, NPY_INT32, strides,
                reinterpret_cast<char*>(field.local_index_map.data()) +
                sizeof(int), 0, 0, NULL);
    assert(sizeof(index_map_t) == 2*sizeof(int));
}

void py::callback(const int current_step) {
    PyObject *pTime = Py_BuildValue("i", current_step);

    err(pTime != NULL, "Cannot create argument");

    D("callback input parameter:");

    //Py_INCREF(pValue);
    PyObject * pReturn = PyObject_CallFunctionObjArgs(pFunc, pTime,
            pEnsemble_list_background, pEnsemble_list_analysis,
            pEnsemble_list_hidden_inout,
            pArray_assimilated_index, pArray_assimilated_varid, NULL);
    err(pReturn != NULL, "No return value");

    D("Back from callback:");

    Py_DECREF(pReturn);
}

void py::finalize() {
    Py_DECREF(pFunc);
    Py_DECREF(pModule);

    // this will also decref list members.
    Py_DECREF(pEnsemble_list_background);
    Py_DECREF(pEnsemble_list_analysis);
    Py_DECREF(pEnsemble_list_hidden_inout);

    PyMem_RawFree(program);

    Py_Finalize();
    D("Freed python context.");
}

void py::err(bool no_fail, const char * error_str) {
    if (!no_fail || PyErr_Occurred()) {
        L("Error! %s", error_str);
        PyErr_Print();
        std::raise(SIGINT);
        exit(1);
    }
}
