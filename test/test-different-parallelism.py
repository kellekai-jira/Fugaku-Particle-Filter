from melissa_da_testing import *

import socket

MAX_SERVER_PROCS = 3
MAX_SIMULATION_PROCS = 3
MAX_RUNNERS = 3

N_RUNNERS = -1
PROCS_SERVER = -1


class RunnerWaiter(FifoThread):
    def __init__(self):
        super().__init__()
        self.iterations_after_runners = 0

    def on_timing_event(self, what, parameter):
        global N_RUNNERS, PROCS_SERVER

        # if all runners are up wait 8 iterations and crash server
        if self.runners == N_RUNNERS:
            if what == Event.STOP_ITERATION:
                self.iterations_after_runners += 1
                if self.iterations_after_runners == 8*PROCS_SERVER:  # every server proc is sending the stop iteration event...
                    return False

        return True



# fill cases...
cases = []
for sep in range(1, MAX_SERVER_PROCS + 1):
    for sip in range(1, MAX_SIMULATION_PROCS + 1):
        for mr in range(1, MAX_RUNNERS + 1):
            cases.append((sep, sip, mr))

#server procs: 1, simulation procs: 3, model runners: 2 -- this produces strange FTI errors
#cases = [(1,3,2)]

# speed up test, only randomized choose some configs.
import random
random.shuffle(cases)
cases = cases[:3]

for i, case in enumerate(cases):
    procs_server, procs_runner, n_runners = case

    # export variables from the local scope
    PROCS_SERVER = procs_server
    N_RUNNERS = n_runners

    print(os.getcwd())

    print("------------------------------------------------------------------------")
    print('step %d/%d' % (i+1, len(cases)))
    print('server procs: %d, simulation procs: %d, model runners: %d'
        % (PROCS_SERVER, procs_runner, n_runners))


    if os.path.isfile('STATS/server.run-information.csv'):
        os.remove('STATS/server.run-information.csv')
    rw = RunnerWaiter()
    rw.start()


    # override some study parameters:
    ase = {}
    ase["MELISSA_DA_TEST_FIFO"] = rw.fifo_name_server

    def run():
        run_melissa_da_study(
            total_steps=3000,
            ensemble_size=10,
            assimilator_type=ASSIMILATOR_DUMMY,
            cluster=LocalCluster(),
            procs_server=PROCS_SERVER,
            procs_runner=procs_runner,
            n_runners=N_RUNNERS,
            show_server_log=False,
            show_simulation_log=False,
            #precommand_server='xterm_gdb',
            additional_server_env=ase)

# run the study in an extern process so it is easier to terminate
    study = Process(target=run)
    study.start()

    rw.join()

    print("RunnerWaiter Thread ended, now terminating study...")
    study.terminate()


    # did not kill server too early
    assert rw.iterations_after_runners >= 8 * PROCS_SERVER

    # check that there was no other runner connecting for some strange reasons later on
    assert rw.runners == N_RUNNERS


    # runner 0 does the output.
    p = os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/reference-1000.txt'
    compare_subset(p)

    print('Wait for port freeing...')
    # port freeing takes some time..
    while True:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        result = s.connect_ex(('127.0.0.1', 5555))

        s.close()
        if result != 0:
            print('socket is closed')
            break

        time.sleep(.1)
