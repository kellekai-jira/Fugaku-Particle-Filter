import shutil

from melissa_da_study import *

run_melissa_da_study(
        runner_cmd='simulation1',
        total_steps=3000,
        ensemble_size=10,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=LocalCluster(),
        procs_server=1,
        procs_runner=1,
        n_runners=10,
        show_server_log = False,
        show_simulation_log = False)


shutil.copyfile("STATS/output.txt", "./reference-1000.txt")
