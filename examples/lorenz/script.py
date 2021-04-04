import os
import shutil
import configparser

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()


run_melissa_da_study(
    cluster=SlurmMn4Cluster('bsc93'),
    walltime='02:00:00',
    is_p2p=True,
    precommand_server='',
    runner_cmd='simulation-lorenz',
    total_steps=3,
    ensemble_size=30,
    procs_runner=48,
    nodes_runner=1,
    n_runners=2,
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
    additional_env={
        'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/lorenz:' + os.getenv('PYTHONPATH'),
        'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
        'MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE': '4',
        'MELISSA_LORENZ_ITER_MAX': '3',
        'MELISSA_LORENZ_OBSERVATION_PERCENT': '20',
        'MELISSA_LORENZ_STATE_DIMENSION': '1048576',
        'MELISSA_LORENZ_OBSERVATION_DIR': '/gpfs/projects/bsc93/bsc93655/melissaP2P/Melissa/melissa-da/examples/lorenz',
        # 'SIMULATION_RANDOM_PROPAGATION_TIME': '1',
        },

    # for is_p2p=False only:
    additional_server_env={

            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
            },
    assimilator_type=ASSIMILATOR_PYTHON,
)
