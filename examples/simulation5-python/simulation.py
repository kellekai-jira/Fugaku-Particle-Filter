# runner written in python that calls the melissa da api

from mpi4py import MPI

from melissa_da_api import MelissaDaAPI


# Main:
import numpy as np
import time



#model init
state = np.zeros(2000, dtype='float64')
state[0] = 41

melissa = MelissaDaAPI(len(state), 0, MPI.COMM_WORLD)



nsteps = 1
while nsteps > 0:
    MPI.COMM_WORLD.Barrier()
    # do some thing with the states
    time.sleep(0.1)
    nsteps = melissa.expose(state)

    print('Now propagating %d steps' % nsteps)

print("Finished")


