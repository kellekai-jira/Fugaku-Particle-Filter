import traceback
import numpy as np
import mpi4py
mpi4py.rc(initialize=False, finalize=False)
from mpi4py import MPI

import random

def calculate_weight(cycle, pid, background, hidden, assimilated_index, assimilated_varid, fcomm):
    try:
        comm = MPI.COMM_WORLD.f2py(fcomm)
        print("rank %d t=%d, Calculating weight for particle with id=%d" % (comm.rank, cycle, pid))
        state = np.zeros(40, dtype='float64')
        state[3] = 42.



        background_d = np.frombuffer(background, dtype='float64',
                             count=len(background) // 8)

        #assert (background_d == state).all()  only works for simulation.py

        # TODO: get the correct comm here!

        return 0.42
        return random.random()
    except Exception as e:
        print('Python Error!')
        print(e)
        traceback.print_stack()
        traceback.print_exc()

    return -1.
