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

namespace py
{
extern PyObject *pFunc;
extern PyObject *pModule;
extern wchar_t *program;
extern PyObject *pEnsemble_list_background;
extern PyObject *pEnsemble_list_analysis;
extern PyObject *pEnsemble_list_hidden_inout;

extern PyObject *pArray_assimilated_index;
extern PyObject *pArray_assimilated_varid;

void init(Field &field);
int callback(const int current_step);
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

    virtual void on_init_state(const int runner_id, const
                               Part & part, const
                               VEC_T * values, const
                               Part & hidden_part,
                               const VEC_T * values_hidden);

    virtual int do_update_step(const int current_step);
    virtual ~PythonAssimilator();
};

#endif /* PYTHONASSIMILATOR_H_ */
