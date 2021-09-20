import os
import shutil
import configparser
import time
import sys

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()

local_dir = os.environ['PJM_LOCALTMP']
print("local directory: " + local_dir)

__env_write_trace = os.environ['MELISSA_LORENZ_TIMING_TRACE']
__env_state_size_elem = os.environ['MELISSA_LORENZ_STATE_DIMENSION']
__env_obs_dir = os.environ['MELISSA_LORENZ_OBSERVATION_DIR']

run_melissa_da_study(
    cluster=FugakuCluster(),
    walltime='02:00:00',
    is_p2p=True,
    precommand_server='',
    runner_cmd='simulation-lorenz',
    total_steps=10,
    ensemble_size=20,
    procs_runner=48,
    nodes_runner=1,
    n_runners=5,
    local_ckpt_dir=local_dir + '/melissa_cache',
    global_ckpt_dir='/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/STATS/Global',
    meta_ckpt_dir='/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/STATS/Meta',
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
    additional_env={
        'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/lorenz:' + os.getenv('PYTHONPATH'),
        'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
        'MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE': '1024',
        'MELISSA_LORENZ_ITER_MAX': '10',
        'MELISSA_LORENZ_OBSERVATION_PERCENT': '20',
        'MELISSA_LORENZ_STATE_DIMENSION': __env_state_size_elem,
        'MELISSA_LORENZ_OBSERVATION_DIR': __env_obs_dir,
        'MELISSA_DA_TEST_FIFO': '/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/timing.trace',
        'MELISSA_DA_TIMING_REPORT': str(time.time() + float(__env_write_trace))
        },

    # for is_p2p=False only:
    additional_server_env={
            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
            },
    assimilator_type=ASSIMILATOR_PYTHON,
)
