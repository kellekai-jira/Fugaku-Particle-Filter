import shutil

from melissa_da_testing import *

PROCS_SERVER = -1

class ServerTester(FifoThread):
    def __init__(self):
        super().__init__()
        self.iterations_after_runners = 0
        self.iterations_after_kills = 0
        self.remove_runners_called = False
        self.killed_server = False
        self.server_had_checkpoint = False

    def on_timing_event(self, what, parameter):
        global N_RUNNERS, PROCS_SERVER

        # if at least all runners are up wait 7 iterations and crash server
        if self.runners >= N_RUNNERS:
            if what == STOP_ITERATION:
                self.iterations_after_runners += 1
                if self.iterations_after_runners == 12 * PROCS_SERVER:
                    print('Crashing server...')
                    #killing_giraffe('melissa_server')
                    os.system('killall melissa_server')
                    self.server_had_checkpoint = (subprocess.call(['grep', "failure[ ]*=[ ]*[1-3]", 'STATS/config.fti']) == 0)
                    shutil.copyfile('STATS/output.txt', 'STATS/output.txt.0')
                    self.killed_server = True



        # Then wait still 20 iterations and that all runners are up again.
        if self.killed_server and what == STOP_ITERATION:
            #print('iteration after kills')

            self.iterations_after_kills += 1

            if self.iterations_after_kills >= 12*PROCS_SERVER:
                return False

        return True


st = ServerTester()
#os.environ["MELISSA_DA_TEST_FIFO"] = fifo_name_runner
st.start()


# override some study parameters:
N_RUNNERS = 2
PROCS_SERVER = 3
ase = {}
ase["MELISSA_DA_TEST_FIFO"] = st.fifo_name_server

def run():
    run_melissa_da_study(
        total_steps=1000,
        ensemble_size=30,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=LocalCluster(),
        procs_server=PROCS_SERVER,
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

st.join()

print("ServerTester Thread ended, now terminating study...")
study.terminate()

assert st.iterations_after_runners >= 10 * PROCS_SERVER  # did not kill server too early
assert st.iterations_after_kills >= 10 * PROCS_SERVER  # server was back up again
assert st.server_had_checkpoint


# Check_output
# join files and remove duplicate lines before compare!
shutil.copyfile('STATS/output.txt', 'STATS/output.txt.1')
subprocess.call(["bash", "-c", "cat STATS/output.txt.0 STATS/output.txt.1 | sort | uniq > STATS/output.txt"])

def get_line_number(fname):
    with open(fname) as f:
        return len(f.readlines())


assert get_line_number('STATS/output.txt.0') >= 10
assert get_line_number('STATS/output.txt.1') >= 10

# Generate reference
p = os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/reference-1000.txt'
subprocess.call(["bash", "-c", "head -n %d %s | sort > STATS/reference-crashing-server-sorted.txt" % (get_line_number('STATS/output.txt'), p)])
compare_subset('STATS/reference-crashing-server-sorted.txt')

assert get_line_number('STATS/output.txt') >= 20


compare_subset(p, 'STATS/output.txt.0')


print("passed!")
