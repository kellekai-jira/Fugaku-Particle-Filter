import os
import subprocess
import sys
from threading import Thread
import time
from melissa_da_study import *

clean_old_stats()

had_checkpoint = False
was_unfinished = False

def run(server_slowdown_factor_=1):
    run_melissa_da_study(
            runner_cmd='simulation2-pdaf',
            total_steps=18,
            ensemble_size=9,
            assimilator_type=ASSIMILATOR_PDAF,
            cluster=LocalCluster(),
            procs_server=3,
            procs_runner=2,
            n_runners=3,
            show_server_log = False,
            show_simulation_log = False,
            config_fti_path='./config.fti',
            server_slowdown_factor=server_slowdown_factor_)

if sys.argv[1] == 'test-example-simulation2':
    run()
else:
    assert False  # Testcase not implemented



exit(subprocess.call(["bash", "test.sh"]))


# TODO: check against server crashes here. will it recover from the good timestamp?
