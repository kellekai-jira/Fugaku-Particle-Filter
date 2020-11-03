# Python3


from melissa_da_study import *
import shutil
import os
import tempfile
import socket
import sys
import fcntl
import pandas as pd
from io import StringIO

from multiprocessing import Process

import time
from threading import Thread

import subprocess
import signal

import random

clean_old_stats()

# print('some cleanup action')
# os.system('killall simulation1; killall melissa_server; killall xterm; ')

executable='simulation1'
total_steps=100
ensemble_size=3
assimilator_type=ASSIMILATOR_DUMMY
cluster=LocalCluster()
procs_server=1
procs_runner=1
n_runners=1

def compare_subset(reference_file, output_file="STATS/output.txt"):
    print('Compare %s with %s...' % (output_file, reference_file))
    with open(output_file) as f:
        line_number = len(f.readlines())

    os.system('head -n %d %s > /tmp/cut-reference_file' % (line_number-2, reference_file))
    os.system('head -n %d %s > /tmp/cut-output_file' % (line_number-2, output_file))

    ret = subprocess.call(['diff', '-s', '--side-by-side', '/tmp/cut-output_file', '/tmp/cut-reference_file'])
    if ret != 0:
        print("failed! Wrong %s generated!" % output_file)
        exit(ret)


def get_run_information():
    return pd.read_csv('STATS/server.run-information.csv')


def get_timing_information():
    return pd.read_csv('STATS/server.timing-information.csv')


def run(server_slowdown_factor_=1):
    print('in run...')
    start = time.time()
    run_melissa_da_study(
            #'xterm_gdb ' +
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
            # precommand_server='xterm_gdb',
            additional_server_env=ase,
            with_fault_tolerance=True)
    diff = time.time() - start
    print("This took %.3f seconds" % diff)



tmpdir = tempfile.mkdtemp()
fifo_name_runner = os.path.join(tmpdir, 'runner_fifo')
fifo_name_server = os.path.join(tmpdir, 'server_fifo')

#os.environ["MELISSA_DA_TEST_FIFO"] = fifo_name_runner
ase = {}
ase["MELISSA_DA_TEST_FIFO"] = fifo_name_server

running = True

ADD_RUNNER                  = 0   # parameter = runner_id
REMOVE_RUNNER               = 1   # parameter = runner_id
START_ITERATION             = 2   # parameter = timestep
STOP_ITERATION              = 3   # parameter = timestep
START_FILTER_UPDATE         = 4   # parameter = timestep
STOP_FILTER_UPDATE          = 5   # parameter = timestep
START_IDLE_RUNNER           = 6   # parameter = runner_id
STOP_IDLE_RUNNER            = 7   # parameter = runner_id
START_PROPAGATE_STATE       = 8   # parameter = state_id
STOP_PROPAGATE_STATE        = 9   # parameter = state_id,
NSTEPS                      = 10  # parameter = nsteps, only used by runner so far
INIT                        = 11  # no parameter  // defines init ... it is not always 0 as the timing api is not called at the NULL environment variable...

runners = 0
iterations_after_runners = 0
killed_all = False
iterations_after_kills = 0
remove_runners_called = False
iterations = 0
class FifoThread(Thread):
    def on_timing_event(self, what, parameter):
        global runners, n_runners, iterations_after_kills, iterations_after_runners
        global remove_runners_called, iterations
        #print(f"got timing event from server: {what}({parameter})")

        # count runners
        if what == ADD_RUNNER:
            runners += 1
        if what == REMOVE_RUNNER:
            runners -= 1
            remove_runners_called = True
            print('remove runners')

        if what == STOP_ITERATION:
            iterations += 1
            if iterations % 100 == 0:
                print('assimilation cycle:', iterations)

        # if at least 5 runners wait 3 iterations and crash 3 runners
        if runners >= n_runners:
            if what == STOP_ITERATION:
                iterations_after_runners += 1
                if iterations_after_runners == 3:
                    class KillerGiraffe(Thread):
                        def run(self):
                            print('Crashing first runner...')
                            killing_giraffe('simulation1')
                            time.sleep(.3)
                            print('Crashing second runner...')
                            killing_giraffe('simulation1')
                            time.sleep(.3)
                            global killed_all
                            killed_all = True
                    giraffe = KillerGiraffe()
                    giraffe.start()

        # Then wait still 3 iterations and that all runners are up again.
                if killed_all:
                    iterations_after_kills += 1

                    if iterations_after_kills >= 3 and runners == n_runners:
                        return False
        return True

    def run(self):
        global running
        try:
            os.mkfifo(fifo_name_server)
        except OSError as e:
            print("Failed to create FIFO: %s" % e)
        else:
            while running:
                with open(fifo_name_server, 'r') as fifo:
                    for data in fifo:
                        for line in data.split('\n'):
                            if line == '':
                                continue
                            args = [int(x) for x in line.split(',')]
                            assert len(args) == 2
                            running = self.on_timing_event(*args)
                            if not running:
                                break
                        if not running:
                            break



            os.remove(fifo_name_server)
            os.rmdir(tmpdir)




        # time.sleep(6)  # TODO: instead of waiting 10 s check if enough simulation 1 processes are up e.g.

ft = FifoThread()
ft.start()
total_steps = 10000
ensemble_size = 2
procs_server = 3
procs_runner = 2
n_runners = 5
p = Process(target=run)
p.start()
ft.join()

print("FifoThread ended, now terminating...")
p.terminate()

# print('some cleanup action')
# os.system('killall simulation1; killall melissa_server; killall xterm; ')

assert remove_runners_called == True
assert runners == n_runners  # check if runners were restarted!
assert iterations_after_runners >= 3
assert iterations_after_kills >= 3

compare_subset(os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/reference-10000.txt')

print("passed!")

