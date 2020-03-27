from melissa_da_study import *
import sys

executable='simulation1'
total_steps=3
ensemble_size=3
assimilator_type=ASSIMILATOR_DUMMY
cluster_name='local'
procs_server=1
procs_runner=1
n_runners=1

def run():
    run_melissa_da_study(
            executable,
            total_steps,
            ensemble_size,
            assimilator_type,
            cluster_name,
            procs_server,
            procs_runner,
            n_runners)

testcase = sys.argv[1]
if testcase == 'test-crashing-runner':
    total_steps = 200
    ensemble_size = 42
    procs_server = 3
    procs_runner = 2
    n_runners = 10
    run()

    print('TODO: validate')
    sys.exit(0)

elif testcase == 'test-crashing-server':
    assert False # unimplemented
elif testcase == 'test-crahsing-launcher':
    assert False # unimplemented
elif testcase == 'test-all.sh':
    assert False # unimplemented
else:
    print('Error! does not know the testcase %s' % testcase)
    assert False


