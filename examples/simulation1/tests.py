# Python3

from melissa_da_study import *
import shutil
import os
import sys
import pandas as pd
from io import StringIO

import time

executable='simulation1'
total_steps=3
ensemble_size=3
assimilator_type=ASSIMILATOR_DUMMY
cluster_name='local'
procs_server=1
procs_runner=1
n_runners=1


def get_run_information():
    marker = 0
    csv = ''
    with open('STATS/server.log', 'r') as f:
        marker = 0
        for line in f.readlines():
            marker -= 1
            if marker > 0:
                csv += line
            if marker == 1:
                return pd.read_csv(StringIO(csv))
            if '------------------- Run information(csv): -------------------' in line:
                marker = 3


    assert False  # Should be never reached. Wrong log file? Compiled without timing?


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

    run()

    print('TODO: validate')
    sys.exit(0)

elif testcase == 'test-crashing-server':
    assert False # unimplemented
elif testcase == 'test-crashing-launcher':
    assert False # unimplemented
elif testcase == 'test-different-paralellism':
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

    for i, case in enumerate(cases):
        procs_server, procs_runner, n_runners = case

        print(os.getcwd())
        if os.path.isdir("STATS"):
            shutil.rmtree("STATS")

        print("------------------------------------------------------------------------")
        print('step %d/%d' % (i+1, len(cases)))
        print('server procs: %d, simulation procs: %d, model runners: %d'
            % (procs_server, procs_runner, n_runners))


        run()

        import subprocess
        # runner 0 does the output.
        ret = subprocess.call('diff -s --side-by-side STATS/output.txt reference.txt'.split())
        if ret != 0:
            print("failed! Wrong output.txt generated!")
            exit(ret)


        used_runners = get_run_information()['number runners(max)'][0]
        if used_runners < n_runners:
            print("failed! Server could only see %d/%d runners. Launcher was too slow?" %
                    (used_runners, n_runners))
            exit(1)

        print("passed!")

    exit(0)

else:
    print('Error! does not know the testcase %s' % testcase)
    assert False


