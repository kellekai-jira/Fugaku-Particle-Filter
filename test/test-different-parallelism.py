from melissa_da_testing import *

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

        # if at least all runners are up wait 8 iterations and crash server
        if self.runners >= N_RUNNERS:
            if what == STOP_ITERATION:
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
    PROCS_SERVER, procs_runner, n_runners = case

    print(os.getcwd())

    print("------------------------------------------------------------------------")
    print('step %d/%d' % (i+1, len(cases)))
    print('server procs: %d, simulation procs: %d, model runners: %d'
        % (PROCS_SERVER, procs_runner, n_runners))


    if os.path.isfile('STATS/server.run-information.csv'):
        os.remove('STATS/server.run-information.csv')
    rw = RunnerWaiter()
#os.environ["MELISSA_DA_TEST_FIFO"] = fifo_name_runner
    rw.start()


# override some study parameters:
    ase = {}
    ase["MELISSA_DA_TEST_FIFO"] = rw.fifo_name_server
    N_RUNNERS = mr

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
            additional_server_env=ase,
            with_fault_tolerance=True)

# run the study in an extern process so it is easier to terminate
    study = Process(target=run)
    study.start()

    rw.join()

    print("RunnerWaiter Thread ended, now terminating study...")
    study.terminate()


    assert rw.iterations_after_runners >= 8 * PROCS_SERVER  # did not kill server too early
    assert rw.runners == N_RUNNERS


    # runner 0 does the output.
    p = os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/reference-1000.txt'
    compare_subset(p)

    print('Wait for port freeing...')
    # port freeing takes some time..
    while subprocess.call(['bash', '-c', 'netstat -tulpn | grep 5555'],
            stderr=subprocess.DEVNULL, stdout= subprocess.DEVNULL) != 1:
        time.sleep(.1)
