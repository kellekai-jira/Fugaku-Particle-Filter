import os
import subprocess
import sys
from threading import Thread
import time
from melissa_da_study import *


had_checkpoint = False

def run(server_slowdown_factor_=1):
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
            show_simulation_log = False,
            config_fti_path='./config.fti',
            server_slowdown_factor=server_slowdown_factor_)

if sys.argv[1] == 'test-example-simulation2':
    run()
elif sys.argv[1] == 'test-crashing-server':
    class KillerGiraffe(Thread):
        def run(self):
            global had_checkpoint
            time.sleep(3)
            print('Crashing a server...')
            killing_giraffe('melissa_server')
            had_checkpoint = (subprocess.call(['grep', "failure[ ]*=[ ]*[1-3]", 'config.fti']) == 0)

            # from shutil import copyfile
            # copyfile('config.fti', 'config.fti.0')

    giraffe = KillerGiraffe()
    giraffe.start()
    run(10000)

    # Check if server was restarted:
    assert os.path.isfile("STATS/server.log.0")
    assert os.path.isfile("STATS/server.log")

    # Check for FTI logs:
    assert subprocess.call(["grep", "Ckpt. ID.*taken in", "STATS/server.log.0"]) == 0
    assert subprocess.call(["grep", "This is a restart. The execution ID is", "STATS/server.log"]) == 0


    print("Had checkpoint?", had_checkpoint)
    assert had_checkpoint




exit(subprocess.call(["bash", "test.sh"]))


# TODO: check against server crashes here. will it recover from the good timestamp?
