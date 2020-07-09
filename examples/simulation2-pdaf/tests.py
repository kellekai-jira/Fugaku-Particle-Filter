import os
import shutil
import subprocess
from multiprocessing import Process
import sys
from threading import Thread
import time
from melissa_da_study import *

clean_old_stats()

melissa_da_path = os.getenv('MELISSA_DA_PATH')

config_fti_lib = melissa_da_path + "/share/melissa-da/config.fti"

procs_server=3

had_checkpoint = False
was_unfinished = False

def run(server_slowdown_factor_=1, config_fti=config_fti_lib):
    run_melissa_da_study(
            runner_cmd='simulation2-pdaf',
            total_steps=18,
            ensemble_size=9,
            assimilator_type=ASSIMILATOR_PDAF,
            cluster=LocalCluster(),
            procs_server=procs_server,
            procs_runner=2,
            n_runners=3,
            show_server_log = False,
            show_simulation_log = False,
            server_slowdown_factor=server_slowdown_factor_,
            config_fti_path=config_fti)

if sys.argv[1] == 'test-example-simulation2':
    procs_server = 3
    run()
elif sys.argv[1] == 'test-crashing-server2':
    class KillerGiraffe(Thread):
        def run(self):
            global had_checkpoint, was_unfinished
            time.sleep(10)
            print('Crashing a server...')
            #killing_giraffe('melissa_server')
            subprocess.call(["killall", "melissa_server"])
            had_checkpoint = (subprocess.call(["grep", "Variate Processor Recovery File", "server.log"]) == 0)
            subprocess.call(["bash","../set_val.sh","failure","3","config.fti"])
            subprocess.call(["bash","../set_val.sh","h5_single_file_dir",os.getcwd()+"/Global","config.fti"])
            was_unfinished = not os.path.isfile("state_step16_for.txt")

            from shutil import copyfile
            copyfile('config.fti', 'config.fti.0')

    procs_server = 3

    giraffe = KillerGiraffe()
    giraffe.start()
    run(10000)

    # Check if server was restarted:
    assert os.path.isfile("STATS/server.log.0")
    assert os.path.isfile("STATS/server.log")

    # Check for FTI logs:
    assert subprocess.call(["grep", "Ckpt. ID.*taken in", "STATS/server.log.0"]) == 0
    assert subprocess.call(["grep", "VPR recovery successfull", "STATS/server.log"]) == 0


    print("Had checkpoint?", had_checkpoint)
    assert had_checkpoint

    print("Was unfinished?", was_unfinished)
    assert was_unfinished


elif sys.argv[1] == 'test-crashing-server2-elastic':

    config_fti_tmp = os.getcwd() + "/config.fti"
    shutil.copyfile(config_fti_lib, config_fti_tmp)

    procs_server = 3

    keywords = {'config_fti': config_fti_tmp, 'server_slowdown_factor_': 10000}
    pFail = Process(target=run, kwargs=keywords)
    pFail.start()

    time.sleep(10)
    print('Crashing a server...')
    pFail.terminate()

    had_checkpoint = (subprocess.call(["grep", "Variate Processor Recovery File", "STATS/server.log"]) == 0)
    was_unfinished = not os.path.isfile("state_step16_for.txt")

    subprocess.call(["bash","set_val.sh","failure","3",config_fti_tmp])
    subprocess.call(["bash","set_val.sh","h5_single_file_dir",os.getcwd()+"/STATS/Global",config_fti_tmp])

    procs_server = 1

    keywords = {'config_fti': config_fti_tmp, 'server_slowdown_factor_': 10000}
    pFinalize = Process(target=run, kwargs=keywords)
    pFinalize.start()
    pFinalize.join()

    os.remove(config_fti_tmp)

    # Check for FTI logs:
    assert subprocess.call(["grep", "Ckpt. ID.*taken in", "STATS/server.log"]) == 0
    assert subprocess.call(["grep", "VPR recovery successfull", "STATS/server.log"]) == 0

    print("Had checkpoint?", had_checkpoint)
    assert had_checkpoint

    print("Was unfinished?", was_unfinished)
    assert was_unfinished


exit(subprocess.call(["bash", "test.sh"]))


# TODO: check against server crashes here. will it recover from the good timestamp?
