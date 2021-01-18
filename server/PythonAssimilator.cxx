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

    // otherwise release mode will make problems!
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        // analysis state is enough:
        double * as_double = reinterpret_cast<double*>(ens_it->state_analysis.data());
        size_t len_double = ens_it->state_analysis.size()/sizeof(double);
        std::fill(as_double, as_double + len_double, 0.0);
    }

    py::init(field);

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
}

void py::init(Field & field) {
    PyObject *pName = NULL;








    py::program = Py_DecodeLocale("melissa_server", NULL);
    Py_Initialize();
    _import_array();  // init numpy

    if (NPY_VERSION != PyArray_GetNDArrayCVersion()) {
        L("Error! Numpy version conflict that might lead to undefined behavior. Recompile numpy!");
        exit(EXIT_FAILURE);
    }

    //PyRun_SimpleString("import sys");
    //PyRun_SimpleString("sys.path.append(\"/home/friese/tmp/test-c-mpi4py\")");

    char *module_name = getenv("MELISSA_DA_PYTHON_ASSIMILATOR_MODULE");
    if (!module_name) {
        L("MELISSA_DA_PYTHON_ASSIMILATOR_MODULE not set! exiting now");
        exit(EXIT_FAILURE);
    }

    pName = PyUnicode_DecodeFSDefault(module_name);
    /* Error checking of pName left out */ // =FIXME!

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    py::err(pModule != NULL, "Cannot find the module file. Is its path in PYTHONPATH?");  // Could not find module
    pFunc = PyObject_GetAttrString(pModule, "callback");

    py::err(pFunc && PyCallable_Check(pFunc), "could not find callable callback function");



    if (field.local_vect_size >= LONG_MAX) {
        L("Error! too large vectsize for python assimilator");
        exit(EXIT_FAILURE);
    }

    // init list:
    py::pEnsemble_list_background = PyList_New(field.ensemble_members.size());
    py::pEnsemble_list_analysis = PyList_New(field.ensemble_members.size());
    npy_intp dims[1] = { static_cast<npy_intp>(field.local_vect_size) };
    int i = 0;
    for (auto &member : field.ensemble_members) {
        PyObject *pBackground = PyArray_SimpleNewFromData(1, dims, NPY_FLOAT64,
                member.state_background.data());
        PyList_SetItem(pEnsemble_list_background, i, pBackground);

        PyObject *pAnalysis = PyArray_SimpleNewFromData(1, dims, NPY_FLOAT64,
                member.state_analysis.data());
        PyList_SetItem(pEnsemble_list_analysis, i, pAnalysis);

        // Refcount on pAnalysis and pBackground is 2 now. Thus even if the objects does
        // not exist anymore in the python context, they will stay in memory.
        // FIXME: is this what we want? Partly: we do not want python to delete the
        // vectors but it shall destroy the other stuff that is stored with the
        // pyobjects... See how we handle this.

        ++i;
    }
}

void py::callback(const int current_step) {
    PyObject *pTime = Py_BuildValue("i", current_step);

    py::err(pTime != NULL, "Error: Cannot create argument");

    D("callback input parameter:");

    //Py_INCREF(pValue);
    PyObject * pReturn = PyObject_CallFunctionObjArgs(pFunc, pTime,
            pEnsemble_list_background, pEnsemble_list_analysis, NULL);
    py::err(pReturn != NULL, "Error: no return value");

    D("Back from callback:");

    Py_DECREF(pReturn);
}

void py::finalize() {
    D("h1");
    Py_XDECREF(pFunc);
    D("h2");
    Py_DECREF(pModule);
    D("h3");
    // FIXME decref list??
    PyMem_RawFree(program);
    D("h4");
}

void py::err(bool no_fail, const char * error_str) {
    if (!no_fail) {
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
        L("Error! %s", error_str);
        assert(false);
        exit(1);
    }
}
