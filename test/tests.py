# Python3


from melissa_da_study import *
import shutil
import os
import tempfile
import socket
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
            with_fault_tolerance=True)
    diff = time.time() - start
    print("This took %.3f seconds" % diff)



tmpdir = tempfile.mkdtemp()
fifo_name = os.path.join(tmpdir, 'myfifo')

os.environ["MELISSA_DA_TEST_FIFO"] = fifo_name

running = True
class KillerGiraffe(Thread):
    def on_timing_event(self, who, what, parameter):
        print(f"got timing event from {who}: {what}({parameter})")
        return True

    def run(self):
        global running
        print('starting fifo Thread')
        try:
            os.mkfifo(fifo_name)
        except OSError as e:
            print("Failed to create FIFO: %s" % e)
        else:
            with open(fifo_name, 'r') as fifo:
            # write stuff to fifo
                while running:
                    time.sleep(0.005)
                    data = fifo.read()
                    #for line in fifo.readlines():
                    if data != "":
                        for line in data.split('\n'):
                            if line == '':
                                continue
                            args = [int(x) for x in line.split(',')]
                            assert len(args) == 3
                            running = self.on_timing_event(*args)
                            if not running:
                                break



            os.remove(fifo_name)
            os.rmdir(tmpdir)




        # time.sleep(6)  # TODO: instead of waiting 10 s check if enough simulation 1 processes are up e.g.
        # print('Crashing first runner...')
        # killing_giraffe('simulation1')
        # time.sleep(4)
        # print('Crashing second runner...')
        # killing_giraffe('simulation1')
        # time.sleep(4)
        # print('Crashing third runner...')
        # killing_giraffe('simulation1')

giraffe = KillerGiraffe()
giraffe.start()
total_steps = 200
ensemble_size = 2
procs_server = 3
procs_runner = 2
n_runners = 1
run()
running = False

# wait for giraffe to finish:
print("waiting for giraffe")
giraffe.join()
#giraffe.stop()

compare('reference-giraffe.txt')

ti = get_timing_information()
assert len(ti['iteration']) == 200



print("passed!")
print("waiting for threads?")
exit(0)

