from melissa_da_study import *

run_melissa_da_study(
        executable='simulation3-empty',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_EMPTY,
        cluster=LocalCluster(),
        procs_server=1,
        procs_runner=1,
        n_runners=1,
        show_server_log = True,
        show_simulation_log = True)
