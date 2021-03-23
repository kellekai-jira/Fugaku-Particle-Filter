import os

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()


run_melissa_da_study(
    server_cmd='xterm_gdb python3 -u %s/server-p2p/server.py' % os.getenv('MELISSA_DA_SOURCE_PATH'),  # Activate this line to start the weight server instead!
    runner_cmd='xterm_gdb python3 -u %s/runner.py' % os.getcwd(),
    total_steps=10,
    ensemble_size=30,
    procs_server=1,
    procs_runner=1,
    n_runners=3,
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
    additional_env={'MELISSA_DA_IS_P2P': '1'}
)
