from melissa_da_testing import *

class RunnerTester(FifoThread):
    def __init__(self):
        super().__init__()
        self.runners = 0
        self.iterations_after_runners = 0
        self.iterations_after_kills = 0
        self.remove_runners_called = False
        self.iterations = 0
        self.killed_all = False

    def on_timing_event(self, what, parameter):
        global N_RUNNERS

        # count runners
        if what == ADD_RUNNER:
            self.runners += 1
            print('runners:', self.runners)
        if what == REMOVE_RUNNER:
            self.runners -= 1
            self.remove_runners_called = True
            print('runners:', self.runners)

        if what == STOP_ITERATION:
            self.iterations += 1
            if self.iterations % 100 == 0:
                print('assimilation cycle:', self.iterations)

        # if at least all runners are up wait 3 iterations and crash 3 runners
        if self.runners >= N_RUNNERS:
            killed_all = False
            if what == STOP_ITERATION:
                self.iterations_after_runners += 1
                if self.iterations_after_runners == 3:
                    def perform_kills(parent):
                        print('Crashing first runner...')
                        killing_giraffe('simulation1')
                        time.sleep(.3)
                        print('Crashing second runner...')
                        killing_giraffe('simulation1')
                        time.sleep(.3)
                        parent.killed_all = True
                        print('killed_all!!!')

                    giraffe = Thread(target=perform_kills, args=(self,))
                    # The evil giraffe strikes back again, performing 3 runner kills!
                    giraffe.start()

        # Then wait still 3 iterations and that all runners are up again.
                if self.killed_all:
                    self.iterations_after_kills += 1

                    if self.iterations_after_kills >= 3 and self.runners == N_RUNNERS:
                        return False
        return True


rt = RunnerTester()
#os.environ["MELISSA_DA_TEST_FIFO"] = fifo_name_runner
rt.start()


# override some study parameters:
N_RUNNERS = 5
ase = {}
ase["MELISSA_DA_TEST_FIFO"] = rt.fifo_name_server

def run():
    run_melissa_da_study(
        total_steps=10000,
        ensemble_size=2,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=LocalCluster(),
        procs_server=3,
        procs_runner=2,
        n_runners=N_RUNNERS,
        show_server_log=False,
        show_simulation_log=False,
        #precommand_server='xterm_gdb',
        additional_server_env=ase,
        with_fault_tolerance=True)

# run the study in an extern process so it is easier to terminate
study = Process(target=run)
study.start()



rt.join()

print("RunnerTester Thread ended, now terminating study...")
study.terminate()

assert rt.remove_runners_called == True
assert rt.runners == N_RUNNERS  # check if runners were restarted!
assert rt.iterations_after_runners >= 3
assert rt.iterations_after_kills >= 3

compare_subset(os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/reference-10000.txt')

print("passed!")
