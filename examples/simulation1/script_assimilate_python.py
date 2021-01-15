import os, sys


import  mpi4py
mpi4py.rc(initialize=False, finalize=False)
from mpi4py import MPI


import numpy as np

def callback(ensemble):

    rank = MPI.COMM_WORLD.rank
    print('my rank:', rank)

    print("in the callback function")
    print("now doing DA update...")

    #print(np.array(ensemble).shape)
    print('input:', ensemble)
    ensemble += 1
    print('output:', ensemble)
    print('refcount:', sys.getrefcount(ensemble))

    # return ensemble return nothing, performs inplace changement


if __name__ == '__main__':
    from melissa_da_study import *
    clean_old_stats()
    run_melissa_da_study(
            runner_cmd='simulation1',
            total_steps=3,
            ensemble_size=3,
            assimilator_type=ASSIMILATOR_PYTHON,
            cluster=LocalCluster(),
            procs_server=2,
            procs_runner=3,
            n_runners=1,
            show_server_log=False,
            show_simulation_log=False,
            additional_server_env={
                'PYTHONPATH': os.getcwd() + ':' + os.getenv('PYTHONPATH'),
                'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
                },
            precommand_server='xterm_gdb',
            server_timeout=10000,
            runner_timeout=10000
            )
