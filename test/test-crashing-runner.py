from melissa_da_testing import *

import os

PROCS_SERVER = 3
N_RUNNERS = 5

class RunnerTester(FifoThread):
    def __init__(self):
        super().__init__()
        self.iterations_after_runners = 0
        self.iterations_after_kills = 0
        self.remove_runners_called = False
        self.killed_all = False
        self.runner_max = 0

    def on_timing_event(self, what, parameter):
        global N_RUNNERS, PROCS_SERVER

        if what == Event.REMOVE_RUNNER:
            self.remove_runners_called = True

        self.runner_max = max(self.runners, self.runner_max)


        # if at least all runners are up wait 3 iterations and crash 2 runners
        if self.runners >= N_RUNNERS:
            if what == Event.STOP_ITERATION:
                self.iterations_after_runners += 1
                if self.iterations_after_runners == 3*PROCS_SERVER:
                    def perform_kills(parent):
                        print('Crashing first runner...')
                        killing_giraffe('simulation1')
                        time.sleep(.3)
                        print('Crashing second runner...')
                        killing_giraffe('simulation1')
                        time.sleep(.3)
                        parent.killed_all = True
                        print('killed all!')

                    giraffe = Thread(target=perform_kills, args=(self,))
                    # The evil giraffe strikes back again, performing 2 runner kills!
                    giraffe.start()

        # Then wait still 3 iterations and that all runners are up again.
                if self.killed_all and self.remove_runners_called:
                    self.iterations_after_kills += 1

                    if self.iterations_after_kills >= 3 * PROCS_SERVER and \
                            (self.runners == N_RUNNERS or (self.runners < self.runner_max \
                            and self.runners > N_RUNNERS)):
                        # we check that the server saw at least one of the 2 runners that
                        # crashed this improves the changes that the test passes on debian
                        # where the server sometimes does not see before the study end
                        # that 2 runners timed out. (Probably the second crashing runner
                        # is not getting any tasks that might time out)
                        print("Successfully quitting RunnerTester thread")
                        return False
        return True


rt = RunnerTester()
#os.environ["MELISSA_DA_TEST_FIFO"] = fifo_name_runner
rt.start()

def run():
    ase = {}
    ase["MELISSA_DA_TEST_FIFO"] = rt.fifo_name_server
    run_melissa_da_study(
        runner_timeout=10,  # detect tests very fast. Still this may not be too tight as the launcher uses the same timeout to detect if a runner started up. so it must be larger than the runners startup time.
        total_steps=3000,
        ensemble_size=10,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=LocalCluster(),
        procs_server=PROCS_SERVER,
        procs_runner=2,
        n_runners=N_RUNNERS,
        show_server_log=False,
        show_simulation_log=False,
        additional_server_env=ase)

# run the study in an extern process so it is easier to terminate
study = Process(target=run)
study.start()



rt.join()

print("RunnerTester Thread ended, now terminating study...")
study.terminate()

assert rt.remove_runners_called == True
# check if runners were restarted!
assert rt.runners == N_RUNNERS or (rt.runners < rt.runner_max and rt.runners > N_RUNNERS)
assert rt.iterations_after_runners >= 3*PROCS_SERVER
assert rt.iterations_after_kills >= 3*PROCS_SERVER

compare_subset(os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/reference-1000.txt')


print("passed!")
