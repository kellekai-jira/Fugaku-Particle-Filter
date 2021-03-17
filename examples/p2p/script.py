import os

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()


pp = os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/p2p'
pp = os.getcwd()
run_melissa_da_study(
    # server_cmd='xterm_gdb python3 -u %s/server.py' % pp,
    # runner_cmd='xterm_gdb python3 -u %s/examples/p2p/r2.py' % os.getenv('MELISSA_DA_SOURCE_PATH'),
    runner_cmd='xterm_gdb python3 -u %s/r2.py' % os.getcwd(),
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
