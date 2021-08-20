from melissa_da_study import *

run_melissa_da_study(
        runner_cmd='simulation1',
        total_steps=1000,
        cluster=FugakuCluster(),
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        # not necessary to add cluster. By default an automatic selection for the cluster
        # is done. See the cluster_selector() method.
        #cluster=LocalCluster(),
        procs_server=2,
        procs_runner=3,
        n_runners=5,
        show_server_log=False,
        show_simulation_log=False)
