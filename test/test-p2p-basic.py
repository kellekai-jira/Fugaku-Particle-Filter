import os
import shutil
import configparser
import time
import subprocess


from melissa_da_study import *
class TestRun:
    def __init__(self, procs_runner=2, nodes_runner=1, n_runners=3, has_propagation_time=''):
        print("Running test with procs_runner=%d, nodes_runner=%d, n_runners=%d, has_propagation_time=%s" %
            (procs_runner, nodes_runner, n_runners, has_propagation_time))
        clean_old_stats()
        self.NODES_RUNNER = nodes_runner
        self.PROCS_RUNNER = procs_runner
        self.N_RUNNERS = n_runners
        self.HAS_PROPAGATION_TIME = has_propagation_time

        run_melissa_da_study(
            is_p2p = True,
            runner_cmd='simulation4-p2p',
            total_steps=10,
            ensemble_size=10,
            procs_runner=self.PROCS_RUNNER,
            nodes_runner=self.NODES_RUNNER,
            n_runners=self.N_RUNNERS,
            show_server_log=False,
            show_simulation_log=False,
            runner_timeout=60,
            server_timeout=60,
            additional_env={
                'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/p2p:' + os.getenv('PYTHONPATH'),
                'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
                'SIMULATION_RANDOM_PROPAGATION_TIME': self.HAS_PROPAGATION_TIME,
                },
        )


# set parameters
def text_in_file(text, filename):
    return subprocess.call(['grep', text, filename]) == 0

def gracefully_ended():
    assert text_in_file('Gracefully ending server now.', 'STATS/server.log.1')

    assert text_in_file('Gracefully ending study now.', 'STATS/melissa_launcher.log')

TestRun(2, 1, 3, '1')
gracefully_ended()

time.sleep(3)

TestRun(2, 1, 3, '')
gracefully_ended()
