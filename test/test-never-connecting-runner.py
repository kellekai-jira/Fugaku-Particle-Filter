import tempfile
from melissa_da_testing import *


cmd = os.environ['MELISSA_DA_SOURCE_PATH'] + '/test/never_connecting_runner.sh'
# TempFile:
fd, tmp_file_name = tempfile.mkstemp()

ae = {
        'BE_NEVER_CONNECTING': tmp_file_name
}


def run_study_and_wait_for(on_event, study_options):
    tmpdir = tempfile.mkdtemp()
    fifo_name_server = os.path.join(tmpdir, 'server_fifo')
    runners = 0
    iterations = 0

    def run():
        run_melissa_da_study(**study_options)

    procs_server = study_options["procs_server"]

    os.mkfifo(fifo_name_server)

    if not 'additioinal_server_env' in study_options:
        study_options['additional_server_env'] = {}
    study_options['additional_server_env']['MELISSA_DA_TEST_FIFO'] = fifo_name_server
    study = Process(target=run)
    study.start()
    print('Started Study')

    running = True
    while running:
        with open(fifo_name_server, 'r') as fifo:
            for data in fifo:
                for line in data.split('\n'):
                    if line == '':
                        continue
                    what, parameter = map(int, line.split(','))

                    # count runners
                    if what == Event.ADD_RUNNER:
                        runners += 1
                        print('runners:', runners)
                    if what == Event.REMOVE_RUNNER:
                        runners -= 1
                        print('runners:', runners)

                    # count assimilation cycles
                    if what == Event.STOP_ITERATION:
                        iterations += 1
                        if (iterations//procs_server) % 100 == 0:
                            print('iteration count:', iterations//procs_server)

                    # callback
                    running = on_event(what, parameter, iterations//procs_server, runners)

                    if not running:
                        break
                if not running:
                    break

    print('Stopping study now')
    study.terminate()
    os.remove(fifo_name_server)
    os.rmdir(tmpdir)




study_options = {
        "runner_cmd": cmd,
        "total_steps": 3000,
        "ensemble_size": 10,
        "assimilator_type": ASSIMILATOR_DUMMY,
        "cluster": LocalCluster(),
        "procs_server": 2,
        "procs_runner": 1,
        "n_runners": 2,
        "show_server_log": False,
        "show_simulation_log": False,
        "additional_env": ae,
        "with_fault_tolerance": True,
        #"precommand_server": "xterm_gdb",
        "runner_timeout": 5
        }

def on_event(what, parameter, iterations, runners):
    if what == Event.ADD_RUNNER:
        on_event.runners.add(parameter)
        print(f'adding runner({parameter})', on_event.runners)
    elif what == Event.REMOVE_RUNNER:
        on_event.runners.remove(parameter)
        print(f'removing runner({parameter})', on_event.runners)

    if on_event.good_iterations == -1 and len(on_event.runners) == 2:
        on_event.good_iterations = iterations

    return not (on_event.good_iterations != -1 and iterations > on_event.good_iterations + 3)

on_event.runners = set()
on_event.good_iterations = -1

# wait for 2 runners to connect! (the first will be never seen by the server
run_study_and_wait_for(on_event, study_options=study_options)

found = 0
with open('STATS/melissa_launcher.log') as f:
    for line in f:
        if 'resubmit group' in line and '(timeout detected by launcher)' in line:
            found += 1

assert found == 1

found = 0
for i in [0, 1]:
    with open(f'STATS/runner-00{i}.log') as f:
        for line in f:
            if 'never connecting runner' in line:
                found += 1

assert found == 0  # must be found exactly once!

with open('STATS/never_connecting_runner') as f:
    for line in f:
        if 'never connecting runner' in line:
            found += 1
assert found == 1

print('passed')
