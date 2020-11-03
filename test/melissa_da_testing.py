import os
import tempfile
import subprocess
from multiprocessing import Process
import time
from threading import Thread

import pandas as pd
from melissa_da_study import *

clean_old_stats()

def compare(reference_file, output_file="STATS/output.txt"):
    print('Compare %s with %s...' % (output_file, reference_file))

    ret = subprocess.call(['diff', '-s', '--side-by-side', output_file, reference_file])
    if ret != 0:
        print("failed! Wrong %s generated!" % output_file)
        exit(ret)

def compare_subset(reference_file, output_file="STATS/output.txt"):
    print('Compare %s with %s...' % (output_file, reference_file))
    with open(output_file) as f:
        line_number = len(f.readlines())

    os.system('head -n %d %s > /tmp/cut-reference_file' % (line_number-2, reference_file))
    os.system('head -n %d %s > /tmp/cut-output_file' % (line_number-2, output_file))

    compare('/tmp/cut-output_file', '/tmp/cut-reference_file')


def get_run_information():
    return pd.read_csv('STATS/server.run-information.csv')


def get_timing_information():
    return pd.read_csv('STATS/server.timing-information.csv')

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

class FifoThread(Thread):
    def on_timing_event(self, what, parameter):
        assert False  # unimplemented
        return False

    def on_timing_event_(self, what, parameter):
        # count runners
        if what == ADD_RUNNER:
            self.runners += 1
            print('runners:', self.runners)
        if what == REMOVE_RUNNER:
            self.runners -= 1
            print('runners:', self.runners)
        # count assimilation cycles
        if what == STOP_ITERATION:
            self.iterations += 1
            if self.iterations % 100 == 0:
                print('assimilation cycle:', self.iterations)

        return self.on_timing_event(what, parameter)

    def __init__(self):
        super().__init__()
        self.tmpdir = tempfile.mkdtemp()
        self.fifo_name_server = os.path.join(self.tmpdir, 'server_fifo')
        self.running = True
        self.runners = 0
        self.iterations = 0

    def run(self):
        try:
            os.mkfifo(self.fifo_name_server)
        except OSError as e:
            print("Failed to create FIFO: %s" % e)
        else:
            while self.running:
                with open(self.fifo_name_server, 'r') as fifo:
                    for data in fifo:
                        for line in data.split('\n'):
                            if line == '':
                                continue
                            args = [int(x) for x in line.split(',')]
                            assert len(args) == 2
                            self.running = self.on_timing_event_(*args)
                            if not self.running:
                                break
                        if not self.running:
                            break

            os.remove(self.fifo_name_server)
            os.rmdir(self.tmpdir)
