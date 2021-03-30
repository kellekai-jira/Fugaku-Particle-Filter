import os
import shutil
import configparser

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()

PROCS_RUNNER = 3
NODES_RUNNER = 1

assert PROCS_RUNNER >= 2
assert NODES_RUNNER >= 1
assert PROCS_RUNNER % NODES_RUNNER == 0

def prepare_runner_dir():
    print('Preparing runner directory (%s)' % os.getcwd())
    shutil.copy('../../config-runner.fti', './config.fti')

    config = configparser.ConfigParser()
    config.read('config.fti')
    config['basic']['node_size'] = str(PROCS_RUNNER//NODES_RUNNER)
    with open('config.fti', 'w') as f:
        config.write(f)


run_melissa_da_study(
    server_cmd='python3.6m -u %s/server-p2p/server.py' % os.getenv('MELISSA_DA_SOURCE_PATH'),  # Activate this line to start the weight server instead!
    # server_cmd='coverage run %s/server-p2p/server.py' % os.getenv('MELISSA_DA_SOURCE_PATH'),  # Activate this line to start the weight server instead!
    #runner_cmd='xterm_gdb python3 -u %s/simulation.py' % os.getcwd(),
    runner_cmd='xterm_gdb simulation4-p2p',
    total_steps=10,
    ensemble_size=30,
    procs_runner=PROCS_RUNNER,
    nodes_runner=NODES_RUNNER,
    n_runners=3,
    create_runner_dir=True,
    prepare_runner_dir=prepare_runner_dir,
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
    additional_env={
        'MELISSA_DA_IS_P2P': '1',
        'PYTHONPATH': os.getcwd() + ':' + os.getenv('PYTHONPATH'),
        'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
        },
)
