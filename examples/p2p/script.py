import os
import shutil
import configparser
import time

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()


run_melissa_da_study(
    is_p2p=True,
    precommand_server='xterm_gdb',
    runner_cmd='simulation4-p2p',
    total_steps=30,
    ensemble_size=30,
    procs_runner=3,
    nodes_runner=1,
    n_runners=3,
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
    additional_env={
        'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/p2p:' + os.getenv('PYTHONPATH'),
        'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
        # 'SIMULATION_RANDOM_PROPAGATION_TIME': '1',
        'MELISSA_DA_TIMING_REPORT': time.time() + 30,  # write timing events after 30 secons!
        },

    # for is_p2p=False only:
    additional_server_env={

            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
            },
    assimilator_type=ASSIMILATOR_PYTHON,
)
