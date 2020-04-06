import subprocess
import sys
from threading import Thread
import time
from melissa_da_study import *


def run():
    clean_old_stats()
    run_melissa_da_study(
            executable='simulation2-pdaf',
            total_steps=18,
            ensemble_size=9,
            assimilator_type=ASSIMILATOR_PDAF,
            cluster_name='local',
            procs_server=3,
            procs_runner=2,
            n_runners=3,
            show_server_log = True,
            show_simulation_log = True)

if sys.argv[1] == 'test-example-simulation2':
    run()
elif sys.argv[1] == 'test-crashing-server':
    class KillerGiraffe(Thread):
        def run(self):
            time.sleep(2)
            print('Crashing a server...')
            killing_giraffe('melissa_server')

    giraffe = KillerGiraffe()
    giraffe.start()
    run()


exit(subprocess.call(["bash", "test.sh"]))


# TODO: check against server crashes here. will it recover from the good timestamp?
