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

executable='simulation1'
total_steps=3
ensemble_size=3
assimilator_type=ASSIMILATOR_DUMMY
cluster_name='local'
procs_server=1
procs_runner=1
n_runners=1

def compare(reference_file):
    print('Compare with %s...' % reference_file)
    cmd = 'diff -s --side-by-side STATS/output.txt %s' % reference_file
    ret = subprocess.call(cmd.split())
    if ret != 0:
        print("failed! Wrong output.txt generated!")
        exit(ret)

def get_csv_section(filename, section_name):
    with open(filename, 'r') as f:
        in_section = False
        csv = ''
        for line in f.readlines():
            if ('End ' + section_name) in line:
                return pd.read_csv(StringIO(csv))
            if in_section:
                csv += line
            if section_name in line:
                in_section = True
        if (in_section):
            print('Error Did not find section "End %s" marker in %s' %
                    (section_name, filename))
        else:
            print('Error Did not find section "%s" marker in %s' %
                    (section_name, filename))


    assert False  # section begin or section end not found



def get_run_information():
    return get_csv_section('STATS/server.log', 'Run information')


def get_timing_information():
    return get_csv_section('STATS/server.log', 'Timing information')


def run():
    print("Running arguments: ",
          executable,
          total_steps,
          ensemble_size,
          assimilator_type,
          cluster_name,
          procs_server,
          procs_runner,
          n_runners)

    clean_old_stats()

    start = time.time()
    run_melissa_da_study(
            executable,
            total_steps,
            ensemble_size,
            assimilator_type,
            cluster_name,
            procs_server,
            procs_runner,
            n_runners,
            False,
            False)
    diff = time.time() - start
    print("This took %.3f seconds" % diff)

testcase = sys.argv[1]
if testcase == 'test-crashing-runner':
    total_steps = 200
    ensemble_size = 42
    procs_server = 3
    procs_runner = 2
    n_runners = 10

    class KillerGiraffe(Thread):
        def run(self):
            time.sleep(2)
            print('Crashing a runner...')
            killing_giraffe('simulation1')
            time.sleep(4)
            print('Crashing a runner...')
            killing_giraffe('simulation1')
            time.sleep(4)
            print('Crashing a runner...')
            killing_giraffe('simulation1')

    giraffe = KillerGiraffe()
    giraffe.start()
    run()

    # wait for giraffe to finish:
    giraffe.join()

    compare('reference-giraffe.txt')

    ti = get_timing_information()
    assert len(ti['iteration']) == 200

    was_at_max = False
    minimum = 10
    for index, row in ti.iterrows():
        if row['max_runners'] == row['min_runners'] == 10:
            was_at_max = True
        if was_at_max:
            if row['max_runners'] == row['min_runners'] and row['max_runners'] < minimum:
                minimum = row['max_runners']

    assert minimum < 10
    # had 10 runners at the beginning?
    assert was_at_max

    if ti['max_runners'][199] > minimum:
        print('Launcher even recovered some of the broken runners')

elif testcase == 'test-crashing-server':
    assert False # unimplemented
elif testcase == 'test-crashing-launcher':
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
    #server procs: 1, simulation procs: 3, model runners: 2
    cases = [(1,3,2)]
    for i, case in enumerate(cases):
        procs_server, procs_runner, n_runners = case

        print(os.getcwd())

        print("------------------------------------------------------------------------")
        print('step %d/%d' % (i+1, len(cases)))
        print('server procs: %d, simulation procs: %d, model runners: %d'
            % (procs_server, procs_runner, n_runners))


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

else:
    print('Error! does not know the testcase %s' % testcase)
    assert False

print("passed!")
exit(0)

