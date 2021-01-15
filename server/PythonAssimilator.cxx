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

    py::init();

}

int PythonAssimilator::do_update_step(const int current_step) {
    L("Doing python update step...\n");
    py::callback();
    MPI_Barrier(mpi.comm());
    int state_id = 0;
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        assert(ens_it->state_analysis.size() ==
               ens_it->state_background.size());


        double * as_double = reinterpret_cast<double*>(ens_it->state_analysis.data());
        size_t len_double = ens_it->state_analysis.size()/sizeof(double);

        double * as_double_bg = reinterpret_cast<double*>(ens_it->state_background.data());

        double * as_double_bg_neighbor;
        if (ens_it == field.ensemble_members.begin())
        {
            as_double_bg_neighbor = reinterpret_cast<double*>((field.ensemble_members.end()-1)->state_background.data());
        }
        else
        {
            as_double_bg_neighbor = reinterpret_cast<double*>((ens_it-1)->state_background.data());
        }


        for (size_t i = 0; i < len_double; i++)
        {
            // pretend to do some da...
            as_double[i] = as_double_bg[i] + state_id;
            as_double[i] += as_double_bg_neighbor[i];
        }
        state_id++;
    }

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

void py::init() {
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
}

void py::callback() {
    PyObject *pValue;


    std::vector<double> vector({1.0 , 2.0});
    npy_intp dims[1] = { 2 };


    //pValue = PyArray_SimpleNew(1, dims, NPY_FLOAT );
    pValue = PyArray_SimpleNewFromData(1, dims, NPY_FLOAT64, vector.data());

    if (!pValue) {
        Py_DECREF(pModule);
        L("Error: Cannot create argument");
        exit(EXIT_FAILURE);
    }


    D("callback input parameter:");
    print_vector(vector);

    //Py_INCREF(pValue);
    PyObject * pReturn = PyObject_CallFunctionObjArgs(pFunc, pValue, NULL);
    if (pReturn != NULL) {
        D("Back from callback:");
        print_vector(vector);
        Py_DECREF(pReturn);
    }
    else {
        //Py_DECREF(pValue);
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        PyErr_Print();
        L("Error! Call failed");
        exit(EXIT_FAILURE);
    }

}

void py::finalize() {
    D("h1");
    Py_XDECREF(pFunc);
    D("h2");
    Py_DECREF(pModule);
    D("h3");
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
