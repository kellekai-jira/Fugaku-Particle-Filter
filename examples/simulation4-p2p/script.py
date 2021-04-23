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
    runner_cmd='xterm_gdb simulation4-p2p',
    #runner_cmd='simulation4-p2p',
    total_steps=5,
    ensemble_size=5,
    procs_runner=2,
    nodes_runner=1,
    n_runners=2,
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
    additional_env={
        'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/simulation4-p2p:' + os.getenv('PYTHONPATH'),
        'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
        'SIMULATION_RANDOM_PROPAGATION_TIME': '1',
        'MELISSA_DA_TIMING_REPORT': str(time.time() + 120),  # write timing events after 60 secons!
        },

    # for is_p2p=False only:
    additional_server_env={

            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
            },
    assimilator_type=ASSIMILATOR_PYTHON,
    walltime='10:10:10',
)
