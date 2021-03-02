import os

from melissa_da_study import *
pp = os.getenv('MELISSA_DA_SOURCE_PATH') + '/server-p2p'
run_melissa_da_study(
    server_cmd='xterm_gdb python3 -u %s/server.py' % pp,
    runner_cmd='xterm_gdb python3 -u %s/runner.py' % pp,
    total_steps=10,
    ensemble_size=30,
    procs_server=1,
    procs_runner=1,
    n_runners=3,
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=60 * 60,  # 60 seconds time for debugging!
    server_timeout=60 * 60,
)
