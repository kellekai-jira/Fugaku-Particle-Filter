# Python3

from melissa_da_study import *
import shutil
import os
import sys
import pandas as pd
from io import StringIO

import time
from threading import Thread

import subprocess
import signal

import random

clean_old_stats()

executable='simulation1'
total_steps=3
ensemble_size=3
assimilator_type=ASSIMILATOR_DUMMY
cluster=LocalCluster()
procs_server=1
procs_runner=1
n_runners=1

def compare(reference_file, output_file="STATS/output.txt"):
    print('Compare %s with %s...' % (output_file, reference_file))
    ret = subprocess.call(['diff', '-s', '--side-by-side', output_file, reference_file])
    if ret != 0:
        print("failed! Wrong %s generated!" % output_file)
        exit(ret)


def get_run_information():
    return pd.read_csv('STATS/server.run-information.csv')


def get_timing_information():
    return pd.read_csv('STATS/server.timing-information.csv')


def run(server_slowdown_factor_=1):
    start = time.time()
    run_melissa_da_study(
            executable,
            total_steps,
            ensemble_size,
            assimilator_type,
            LocalCluster(),
            procs_server,
            procs_runner,
            n_runners,
            False,
            False,
            server_slowdown_factor=server_slowdown_factor_,
            precommand_server='',
            with_fault_tolerance=True)
    diff = time.time() - start
    print("This took %.3f seconds" % diff)

def test_index_map(executable_):
    global executable, procs_server, procs_runner, n_runners, total_steps
    global assimilator_type

    ref_file = './reference-index-map.csv'
    if os.path.isfile(ref_file):
        os.remove(ref_file)

    executable = executable_

    total_steps = 1
    assimilator_type = ASSIMILATOR_PRINT_INDEX_MAP


    procs_server = 2
    procs_runner = 3
    n_runners = 2
    clean_old_stats()
    run()
    os.system('cat STATS/index-map-hidden.csv >> STATS/index-map.csv')
    shutil.copyfile('STATS/index-map.csv', ref_file)
    n_runners = 1
    procs_server = 3
    procs_runner = 2
    run()
    os.system('cat STATS/index-map-hidden.csv >> STATS/index-map.csv')


    compare("STATS/index-map.csv", './reference-index-map.csv')




testcase = sys.argv[1]
elif testcase == 'test-crashing-launcher':
    subprocess.call(["bash", "-c", "python3 tests.py long-run"])
    assert False # unimplemented

elif testcase == 'test-different-parallelism':
    MAX_SERVER_PROCS = 3
    MAX_SIMULATION_PROCS = 3
    MAX_RUNNERS = 3

    total_steps = 5


    print('ensemble members: %d, total_steps: %d' % (ensemble_size, total_steps))


    # fill cases...
    cases = []
    for sep in range(1, MAX_SERVER_PROCS + 1):
        for sip in range(1, MAX_SIMULATION_PROCS + 1):
            for mr in range(1, MAX_RUNNERS + 1):
                cases.append((sep, sip, mr))
    #server procs: 1, simulation procs: 3, model runners: 2 -- this produces strange FTI errors
    #cases = [(1,3,2)]
    for i, case in enumerate(cases):
        procs_server, procs_runner, n_runners = case

        print(os.getcwd())

        print("------------------------------------------------------------------------")
        print('step %d/%d' % (i+1, len(cases)))
        print('server procs: %d, simulation procs: %d, model runners: %d'
            % (procs_server, procs_runner, n_runners))


        if os.path.isfile('STATS/server.run-information.csv'):
            os.remove('STATS/server.run-information.csv')
        run()

        # runner 0 does the output.
        compare('reference.txt')


        used_runners = get_run_information()['number runners(max)'][0]
        if used_runners < n_runners:
            print("failed! Server could only see %d/%d runners. Launcher was too slow?" %
                    (used_runners, n_runners))
            exit(1)

elif testcase == 'test-check-stateless':
    assert check_stateless('simulation1')
    assert check_stateless('simulation1-stateful') == False
    assert check_stateless('simulation1-hidden')

elif testcase == 'test-index-map':
    test_index_map('simulation1-index-map')
elif testcase == 'test-hidden-index-map':
    executable = "simulation1-hidden-index-map"

    clean_old_stats()
    total_steps = 1
    assimilator_type = ASSIMILATOR_PRINT_INDEX_MAP


    procs_server = 2
    procs_runner = 3
    n_runners = 2
    run()
    os.system('cat STATS/index-map-hidden.csv >> STATS/index-map.csv')
    compare("STATS/index-map.csv", './reference-hidden-index-map.csv')
elif testcase == 'test-empty-index-map':
    test_index_map('simulation1')
    compare("STATS/index-map.csv", './reference-empty-index-map.csv')
elif testcase == 'test-empty-hidden-index-map':
    executable = "simulation1-hidden"

    total_steps = 1
    assimilator_type = ASSIMILATOR_PRINT_INDEX_MAP

    procs_server = 3
    procs_runner = 2
    n_runners = 1
    clean_old_stats()
    run()
    os.system('cat STATS/index-map-hidden.csv >> STATS/index-map.csv')
    compare("STATS/index-map.csv", './reference-empty-hidden-index-map.csv')

elif testcase == 'long-run':
    # To generate reference for KillerGiraffe tests and for crashing_launcher test
    long_run()


else:
    print('Error! does not know the testcase %s' % testcase)
    assert False

print("passed!")
exit(0)

