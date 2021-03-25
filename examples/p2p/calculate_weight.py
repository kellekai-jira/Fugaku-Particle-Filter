import traceback
import numpy as np
import mpi4py
mpi4py.rc(initialize=False, finalize=False)
from mpi4py import MPI

def calculate_weight(cycle, background, hidden, assimilated_index, assimilated_varid):
    try:
        print("Calculating weight for:", cycle)
        state = np.zeros(40, dtype='float64')
        state[3] = 42.



        background_d = np.frombuffer(background, dtype='float64',
                             count=len(background) // 8)

        assert (background_d == state).all()

        # TODO: get the correct comm here!

        return 0.42
    except Exception as e:
        print('Python Error!')
        print(e)
        traceback.print_stack()
        traceback.print_exc()

    return -1.
