import ctypes
import os

class MelissaDaAPI:
    MPI_Fint = ctypes.c_int

# void melissa_init_f(const char *field_name,
                    # const int *local_doubles_count,
                    # const int *local_hidden_doubles_count,
                    # MPI_Fint   *comm_fortran);
    def __init__(self, local_doubles_count, local_hidden_doubles_count, comm):
        self.c_lib = ctypes.CDLL(os.getenv("MELISSA_DA_PATH")+"/lib/libmelissa_da_api.so")
        self.field_name = "state1".encode('utf-8')
        fcomm = comm.py2f()
        self.c_lib.melissa_init_f(
                ctypes.c_char_p(self.field_name),
                ctypes.byref(ctypes.c_int(local_doubles_count)),
                ctypes.byref(ctypes.c_int(local_hidden_doubles_count)),
                ctypes.byref(MelissaDaAPI.MPI_Fint(fcomm))
                )


    def expose(self, values, hidden=None):
        h = None
        if hidden is not None:
            h = hidden.ctypes.data_as(ctypes.POINTER(ctypes.c_double))
        return self.c_lib.melissa_expose_d(
            ctypes.c_char_p(self.field_name),
            values.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
            h,
                )

