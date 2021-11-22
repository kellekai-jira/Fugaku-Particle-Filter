import os
import shutil
import configparser
import time
import sys

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()

env = ['MELISSA_LORENZ_ITER_MAX',
        'MELISSA_LORENZ_MEMBERS',
        'MELISSA_LORENZ_RUNNER_GROUP_SIZE',
        'MELISSA_LORENZ_PROCS_RUNNERS',
        'MELISSA_LORENZ_NUM_RUNNERS',
        'MELISSA_LORENZ_STATE_DIMENSION',
        'MELISSA_LORENZ_OBSERVATION_DIR',
        'MELISSA_LORENZ_EXPERIMENT_DIR',
        'MELISSA_LORENZ_TIMING_TRACE']

assert all(x in os.environ for x in env)

if int(os.environ['PJM_LLIO_LOCALTMP_SIZE']) > 0:
    local_dir = os.environ['PJM_LOCALTMP']
else:
    local_dir = '/dev/shm'

#if int(os.environ['PJM_LLIO_SHAREDTMP_SIZE']) > 0:
#    global_dir = os.environ['PJM_SHAREDTMP']
#else:
#    global_dir = os.environ['MELISSA_LORENZ_EXPERIMENT_DIR']
global_dir = os.environ['MELISSA_LORENZ_EXPERIMENT_DIR']

print("local directory: " + local_dir)
print("global directory: " + global_dir)

__env_steps = int(os.environ['MELISSA_LORENZ_ITER_MAX'])
__env_members = int(os.environ['MELISSA_LORENZ_MEMBERS'])
__env_procs_runners = int(os.environ['MELISSA_LORENZ_PROCS_RUNNERS'])
__env_nodes_runners = int(os.environ['MELISSA_LORENZ_NODES_RUNNERS'])
__env_num_runners = int(os.environ['MELISSA_LORENZ_NUM_RUNNERS'])
__env_state_size_elem = os.environ['MELISSA_LORENZ_STATE_DIMENSION']
__env_obs_dir = os.environ['MELISSA_LORENZ_OBSERVATION_DIR']
__env_write_trace = float(os.environ['MELISSA_LORENZ_TIMING_TRACE'])
__env_runner_group_size = int(os.environ['MELISSA_LORENZ_RUNNER_GROUP_SIZE'])

__precommand_runner = ''
if "MELISSA_PRECOMMAND_RUNNER" in os.environ:
    __precommand_runner = os.environ["MELISSA_PRECOMMAND_RUNNER"]

run_melissa_da_study(
    cluster=FugakuCluster(),
    walltime='02:00:00',
    is_p2p=True,
    precommand_server='',
    precommand_runner=__precommand_runner,
    runner_cmd='simulation-lorenz',
    total_steps=__env_steps,
    ensemble_size=__env_members,
    procs_runner=__env_procs_runners,
    nodes_runner=__env_nodes_runners,
    runner_group_size = __env_runner_group_size,
    n_runners=__env_num_runners,
    local_ckpt_dir=local_dir + '/melissa_cache',
    global_ckpt_dir=global_dir + '/Global',
    meta_ckpt_dir=global_dir + '/Meta',
#    global_ckpt_dir='/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/STATS/Global',
#    meta_ckpt_dir='/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/STATS/Meta',
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
        'MELISSA_DA_TEST_FIFO': global_dir + '/timing.trace',
        'MELISSA_DA_TIMING_REPORT': str(time.time() + __env_write_trace)
        },

    # for is_p2p=False only:
    additional_server_env={
            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
            },
    assimilator_type=ASSIMILATOR_PYTHON,
)
