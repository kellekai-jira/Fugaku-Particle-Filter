import os
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
            show_server_log = False,
            show_simulation_log = False)

if sys.argv[1] == 'test-example-simulation2':
    run()
elif sys.argv[1] == 'test-crashing-server':
    had_checkpoint = False
    class KillerGiraffe(Thread):
        def run(self):
            time.sleep(3)
            had_checkpoint = (subprocess.call(['grep', "failure[ ]*=[ ]*[1-3]", 'config.fti']) == 0)

            from shutil import copyfile
            copyfile('config.fti', 'config.fti.0')
            print('Crashing a server...')
            killing_giraffe('melissa_server')

    giraffe = KillerGiraffe()
    giraffe.start()
    run()

    # Check if server was restarted:
    assert os.path.isfile("STATS/server.log.0")
    assert os.path.isfile("STATS/server.log")

    # Check for FTI logs:  FIXME
    #assert subprocess.call(["grep", "", "STATS/server.log.0"])
    #assert subprocess.call(["grep", "", "STATS/server.log.0"])


    print("Had checkpoint?", had_checkpoint)
    assert had_checkpoint




exit(subprocess.call(["bash", "test.sh"]))


# TODO: check against server crashes here. will it recover from the good timestamp?
