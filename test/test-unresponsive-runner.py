from melissa_da_testing import *


local_cluster = LocalCluster()
EXEC = 'simulation1-deadlock'
run_melissa_da_study(
    runner_cmd=EXEC,
    runner_timeout=5,  # detect tests very fast. Still this may not be too tight as the launcher uses the same timeout to detect if a runner started up. so it must be larger than the runners startup time.
    total_steps=3,
    ensemble_size=10,
    assimilator_type=ASSIMILATOR_DUMMY,
    cluster=local_cluster,
    procs_server=3,
    procs_runner=2,
    n_runners=2,
    show_server_log=False,
    show_simulation_log=False)


found1 = False
found2 = False
with open('STATS/melissa_launcher.log') as f:
    for line in f.readlines():
        if 'Gracefully ending study now' in line:
            found1 = True
            break

with open('STATS/server.log.1') as f:
    for line in f.readlines():
        if 'times in a row on' in line and 'Go fix your ensemble' in line:
            found2 = True
            break

assert found1
assert found2

# TODO: test if checkpoint file is correct!

# Since the study will crash we manually clean up:
time.sleep(1)  # give instruction pointer to the os to be able to finish ongoing app startups
os.system('ps')
local_cluster.CleanUp(EXEC)

print('passed')
