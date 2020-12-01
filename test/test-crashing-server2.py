import shutil

from melissa_da_testing import *


PROCS_SERVER = 2
N_RUNNERS = 2
class ServerTester(FifoThread):
    def __init__(self):
        super().__init__()
        self.remove_runners_called = False
        self.killed_server = False
        self.server_had_checkpoint = False
        self.was_unfinished = False

    def on_timing_event(self, what, parameter):
        global N_RUNNERS, PROCS_SERVER

        #  wait 7 iterations and crash server
        if what == Event.STOP_ITERATION:
            print('iteration :', self.iterations)
            if self.iterations == 2 * PROCS_SERVER:
                print('Crashing server...')
                #killing_giraffe('melissa_server')
                os.system('pkill -9 melissa_server')
                self.server_had_checkpoint = (subprocess.call(['grep', "failure[ ]*=[ ]*[1-3]", 'config.fti']) == 0)
                self.was_unfinished = not os.path.isfile("state_step16_for.txt")
                shutil.copyfile('config.fti', 'config.fti.0')
                self.killed_server = True
                return False
        return True


st = ServerTester()
st.start()



def run():
    ase = {}
    ase["MELISSA_DA_TEST_FIFO"] = st.fifo_name_server
    run_melissa_da_study(
        runner_cmd='simulation2-pdaf',
        total_steps=18,
        ensemble_size=9,
        assimilator_type=ASSIMILATOR_PDAF,
        cluster=LocalCluster(),
        procs_server=3,
        procs_runner=PROCS_SERVER,
        n_runners=3,
        show_server_log=False,
        show_simulation_log=False,
        server_slowdown_factor=10000,  # slow down so it works better
        additional_server_env=ase)

run()
st.running = False


assert st.server_had_checkpoint
assert st.was_unfinished


 # Check if server was restarted:
assert os.path.isfile("STATS/server.log.1")
assert os.path.isfile("STATS/server.log.2")

# Check for FTI logs:
assert subprocess.call(["grep", "Ckpt. ID.*taken in", "STATS/server.log.1"]) == 0
assert subprocess.call(["grep", "This is a restart. The execution ID is", "STATS/server.log.2"]) == 0


exit(subprocess.call(["bash", os.environ['MELISSA_DA_SOURCE_PATH'] + "/examples/simulation2-pdaf/test.sh"]))
