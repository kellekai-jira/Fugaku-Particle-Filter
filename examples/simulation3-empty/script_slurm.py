from melissa_da_study import *

clean_old_stats()

run_melissa_da_study(
        runner_cmd='simulation3-empty',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_EMPTY,
        cluster=SlurmCluster('prcoe03', 'devel'),
        procs_server=1,
        procs_runner=1,
        n_runners=1,
        show_server_log = True,
        show_simulation_log = True,
        walltime='00:02:00')
