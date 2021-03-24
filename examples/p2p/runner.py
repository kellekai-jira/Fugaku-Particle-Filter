# runner written in python that calls the melissa da api


import ctypes
import os

from mpi4py import MPI

class MelissaAPI:
    MPI_Fint = ctypes.c_int

# void melissa_init_f(const char *field_name,
                    # const int *local_doubles_count,
                    # const int *local_hidden_doubles_count,
                    # MPI_Fint   *comm_fortran);
    def __init__(self):
        self.c_lib = ctypes.CDLL(os.getenv("MELISSA_DA_PATH")+"/lib/libmelissa_da_api.so")


    def init(self, local_doubles_count, local_hidden_doubles_count, comm):
        self.field_name = "state1".encode('utf-8')


        fcomm = comm.py2f()  # since we only can easily acces the fortran communicator,
        # we use the fortran api for init.


        self.c_lib.melissa_init_f(
                ctypes.c_char_p(self.field_name),
                ctypes.byref(ctypes.c_int(local_doubles_count)),
                ctypes.byref(ctypes.c_int(local_hidden_doubles_count)),
                ctypes.byref(MelissaAPI.MPI_Fint(fcomm))
                )


    def expose(self, values):
        return self.c_lib.melissa_expose_d(
            ctypes.c_char_p(self.field_name),
            values.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
            None,
                )

    def get_comm(self):
        self.c_lib.melissa_get_comm_f.restype = ctypes.c_int
        res = self.c_lib.melissa_get_comm_f()
        print ('in python get comm:', res)
        return res

    def io_init(self, comm):
        fcomm = comm.py2f()
        print('in python io init', fcomm)
        self.c_lib.melissa_io_init_f(ctypes.byref(MelissaAPI.MPI_Fint(fcomm)))





# Main:
import numpy as np
import time

melissa = MelissaAPI()

print('Old  comm size:', MPI.COMM_WORLD.size)

melissa.io_init(MPI.COMM_WORLD)
# Melissa init
comm = MPI.COMM_WORLD.f2py(melissa.get_comm())
print('New  comm size:', comm.size)

#model init
state = np.zeros(2000, dtype='float64')
state[0] = 41

melissa.init(len(state), 0, comm)



nsteps = 1
while nsteps > 0:
    comm.Barrier()
    # do some thing with the states
    time.sleep(0.1)
    nsteps = melissa.expose(state)

    print('Now propagating %d steps' % nsteps)

print("Finished")


