from melissa_da_testing import *

run_melissa_da_study(
    runner_cmd='simulation1-deadlock',
    runner_timeout=5,  # detect tests very fast. Still this may not be too tight as the launcher uses the same timeout to detect if a runner started up. so it must be larger than the runners startup time.
    total_steps=3,
    ensemble_size=10,
    assimilator_type=ASSIMILATOR_DUMMY,
    cluster=LocalCluster(),
    procs_server=3,
    procs_runner=2,
    n_runners=2,
    show_server_log=False,
    show_simulation_log=False)


found1 = False
found2 = False
with open('STATS/melissa_launcher.log') as f:
    for line in f.readlines():
        if 'All runners crashed. Stopping study now.' in line:
            found1 = True
        if 'times, remove simulation':
            found2 = True

assert found1
assert found2
