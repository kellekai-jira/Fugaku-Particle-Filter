/*
 * PythonAssimilator.h
 *
 *  Created on: Jan 14, 2021
 *      Author: friese
 */

#ifndef PYTHONASSIMILATOR_H_
#define PYTHONASSIMILATOR_H_

#include "Assimilator.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace py {
    static PyObject *pFunc;
    static PyObject *pModule;
    static wchar_t *program;

    void init();
    void callback();  // FIXME: add parameters
    void finalize();
    void err(bool no_fail, const char * error_str);
}

class PythonAssimilator : public Assimilator
{
private:
    Field & field;
    const int total_steps;
    MpiManager & mpi;
public:
    PythonAssimilator(Field & field_, const int total_steps, MpiManager & mpi_);
    virtual int do_update_step(const int current_step);
    virtual ~PythonAssimilator();
};

#endif /* PYTHONASSIMILATOR_H_ */
