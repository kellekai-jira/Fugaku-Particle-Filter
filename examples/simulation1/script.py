from melissa_da_study import *

run_melissa_da_study(
        runner_cmd='simulation1',
        total_steps=10,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=LocalCluster(),
        procs_server=2,
        procs_runner=3,
        n_runners=1,
        show_server_log = False,
        show_simulation_log = False)
