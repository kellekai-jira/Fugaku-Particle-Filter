import repex


import time
import random
import os
import sys
#from matplotlib import pyplot as plt
import numpy as np

from melissa_da_study import *

start_time = time.time()

IS_P2P = True
NODES_SERVER = 1 if IS_P2P else 16
NODES_RUNNER = 1
MEMBERS = 100

N_RUNNERS = 1
if IS_P2P:
    print('Doing a p2p study')
    sn = os.getenv('SLURM_NNODES')
    if sn:
        N_RUNNERS=int(sn) - 1  # subtract server node
        MEMBERS = N_RUNNERS * 11

        print(f'Doing a study, members={MEMBERS}, runners={N_RUNNERS}')

def run():
    clean_old_stats()
    run_melissa_da_study(
            runner_cmd='/gpfsscratch/rech/moy/rkop006/melissa-p2p/examples/simulation4-p2p/test-elasticity/runner.sh',
            total_steps=100000,  # 10e3 are about 5 minuts be longer for sure ;)
            ensemble_size=MEMBERS,
            n_runners=N_RUNNERS,
            # ensemble_size=102,
            # n_runners=4,
            assimilator_type=ASSIMILATOR_DUMMY,
            # not necessary to add cluster. By default an automatic selection for the cluster
            # is done. See the cluster_selector() method.
            cluster=SlurmCluster('igf@cpu'),
            procs_server=1 if IS_P2P else NODES_SERVER * 40,
            nodes_server=NODES_SERVER,
            local_ckpt_dir='/dev/shm',
            procs_runner=40,
            nodes_runner=NODES_RUNNER,
            server_timeout=500,
            runner_timeout=500 if IS_P2P else 240,
            show_server_log=False,
            show_simulation_log=False,
            additional_server_env={  # necessary for jean-zay
                'LD_LIBRARY_PATH': os.getenv('LD_LIBRARY_PATH') + ':/gpfsscratch/rech/moy/rkop006/conda_envs/lib'
                },
            walltime='00:45:00',
            additional_env={
                'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/simulation4-p2p:' + os.getenv('PYTHONPATH'),
                'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
                'SIMULATION_RANDOM_PROPAGATION_TIME': '1',
                'MELISSA_DA_TIMING_REPORT': str(time.time() + 60*16),  # write timing events after 16 mins
                },
            is_p2p=IS_P2P)

HOME = os.getenv("HOME")
en = 'p2p-vs-pfs'
if not IS_P2P:
    en = 'central-server'

repex.run(
        EXPERIMENT_NAME=en,
        INPUT_FILES=['/gpfsscratch/rech/moy/rkop006/melissa-p2p/build/CMakeCache.txt'],
        GIT_REPOS=['/gpfsscratch/rech/moy/rkop006/melissa-p2p/'],
        experiment_function=run)

