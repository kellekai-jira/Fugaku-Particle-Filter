/*
 *
 * pdaf.h
 *
 *  Created on: Aug 15, 2019
 *      Author: friese
 */

#ifndef PDAF_H_
#define PDAF_H_
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

void cwrapper_init_pdaf(const int * param_dim_state, const int * param_ensemble_size, const int * param_total_steps);
void cwrapper_PDAF_deallocate();
void cwrapper_PDAF_get_state(int * doexit, const int * dim_state_analysis, double * state_analysis[], int * status);
void cwrapper_PDFA_put_state(const int * dim_state_background, const double * state_background[], int * status);

#ifdef __cplusplus
}
#endif


#endif /* PDAF_H_ */
