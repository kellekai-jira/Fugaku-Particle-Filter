# runner written in python that calls the melissa da api


import ctypes
import os

from mpi4py import MPI

class MelissaAPI:

# void melissa_init_f(const char *field_name,
                    # const int *local_doubles_count,
                    # const int *local_hidden_doubles_count,
                    # MPI_Fint   *comm_fortran);
    def __init__(self, local_doubles_count, local_hidden_doubles_count, comm):

        self.c_lib = ctypes.CDLL(os.getenv("MELISSA_DA_PATH")+"/lib/libmelissa_da_api.so")

        self.field_name = "state1".encode('utf-8')


        fcomm = comm.py2f()  # since we only can easily acces the fortran communicator,
        # we use the fortran api for init.

        MPI_Fint = ctypes.c_int

        self.c_lib.melissa_init_f(
                ctypes.c_char_p(self.field_name),
                ctypes.byref(ctypes.c_int(local_doubles_count)),
                ctypes.byref(ctypes.c_int(local_hidden_doubles_count)),
                ctypes.byref(MPI_Fint(fcomm))
                )


    def expose(self, values, hidden_values):
        return self.c_lib.melissa_expose_d(
            ctypes.c_char_p(self.field_name),
            values.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
            hidden_values.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                )

    def refresh_comm(self, inout_comm):
        fcomm = inout_comm.py2f()  # since we only can easily acces the fortran communicator,
        self.c_lib.melissa_refresh_comm_f(fcomm)
        inout_comm.MPI.comm.f2py(fcomm)



# Main:
import numpy as np
import time
state = np.zeros(2000, dtype='float64')
hidden = np.zeros(4000, dtype='float64')
melissa = MelissaAPI(len(state), len(hidden), MPI.COMM_WORLD)

FTI_COMM_WORLD = melissa.get_comm()

nsteps = 1
while nsteps > 0:
    FTI_COMM_WORLD.Barrier()
    # do some thing with the states
    time.sleep(0.1)
    nsteps = melissa.expose(state, hidden)

    print('Now propagating %d steps' % nsteps)

print("Finished")


