import os
import shutil
import configparser

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()


run_melissa_da_study(
    is_p2p=True,  # FIXME: make it possible to run as normal central server particle filter study too by implementing python assimilator's api calls
    precommand_server='xterm_gdb',
    runner_cmd='xterm_gdb simulation4-p2p',
    total_steps=10,
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
        },
)
