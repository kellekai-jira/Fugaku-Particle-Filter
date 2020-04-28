from melissa_da_study import *

run_melissa_da_study(
        runner_cmd='simulation2-pdaf',
        total_steps=18,
        ensemble_size=9,
        assimilator_type=ASSIMILATOR_PDAF,
        cluster=LocalCluster(),
        procs_server=3,
        procs_runner=2,
        n_runners=3,
        show_server_log = True,
        show_simulation_log = True,
        additional_server_env={"PDAF_FILTER_NAME": "EnKF"})
