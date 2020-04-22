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
if testcase == 'test-crashing-runner':
    class KillerGiraffe(Thread):
        def run(self):
            time.sleep(2)
            print('Crashing first runner...')
            killing_giraffe('simulation1')
            time.sleep(4)
            print('Crashing second runner...')
            killing_giraffe('simulation1')
            time.sleep(4)
            print('Crashing third runner...')
            killing_giraffe('simulation1')

    giraffe = KillerGiraffe()
    giraffe.start()
    total_steps = 200
    ensemble_size = 42
    procs_server = 3
    procs_runner = 2
    n_runners = 10
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

elif testcase == 'test-crashing-server1':
    class KillerGiraffe(Thread):
        def run(self):
            global had_checkpoint
            time.sleep(2)
            print('Crashing a server...')
            #killing_giraffe('melissa_server')
            subprocess.call(["killall", "melissa_server"])
            had_checkpoint = (subprocess.call(['grep', "failure[ ]*=[ ]*[1-3]", 'config.fti']) == 0)
            shutil.copyfile('output.txt', 'output.txt.0')

            # from shutil import copyfile
            # copyfile('config.fti', 'config.fti.0')

    giraffe = KillerGiraffe()
    giraffe.start()

    total_steps = 200
    ensemble_size = 4
    procs_server = 1
    procs_runner = 2
    n_runners = 2
    run()

    # Check if server was restarted:
    assert os.path.isfile("STATS/server.log.0")
    assert os.path.isfile("STATS/server.log")

    # Check for FTI logs:
    assert subprocess.call(["grep", "Ckpt. ID.*taken in", "STATS/server.log.0"]) == 0
    assert subprocess.call(["grep", "This is a restart. The execution ID is", "STATS/server.log"]) == 0

    ref_size = os.path.getsize('reference-giraffe.txt')
    # Check if file sizes are good
    # Check that none of the files contains the full output
    assert ref_size > os.path.getsize('STATS/output.txt.0') > 309  # bytes
    assert ref_size > os.path.getsize('STATS/output.txt') > 1000  # bytes



    print("Had checkpoint?", had_checkpoint)
    assert had_checkpoint

    # Check_output
    # join files and remove duplicate lines before compare!
    shutil.copyfile('STATS/output.txt', 'STATS/output.txt.1')
    subprocess.call(["bash", "-c", "cat STATS/output.txt.0 STATS/output.txt.1 | sort | uniq > STATS/output.txt"])
    # Generate reference
    subprocess.call(["bash", "-c", "sort reference-giraffe.txt > STATS/reference-crashing-server-sorted.txt"])
    compare('STATS/reference-crashing-server-sorted.txt')


    # TODO: check that not one file contains all the results already...

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

