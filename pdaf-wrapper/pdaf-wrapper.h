/*
 *
 * pdaf.h
 *
 *  Created on: Aug 15, 2019
 *      Author: friese
 */

#ifndef PDAF_WRAPPER_H_
#define PDAF_WRAPPER_H_
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

// TODO: choose better names!
// TODO: pass parameters by value better?
void cwrapper_init_user(const int * param_total_steps);
void cwrapper_init_pdaf(const int * param_dim_state, const
                        int * param_dim_state_p, const
                        int * param_ensemble_size);
void cwrapper_assimilate_pdaf();
void cwrapper_PDAF_deallocate();

// old, TODO: remove, also remove from f90 file.
int cwrapper_PDAF_get_state(int * doexit, const int * dim_state_analysis,
                            double * state_analysis[], int * status);
void cwrapper_PDAF_put_state(const int * dim_state_background, const
                             double * state_background[], int * status);

#ifdef __cplusplus
}
#endif


#endif /* PDAF_WRAPPER_H_ */
