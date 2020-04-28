from melissa_da_study import *


debug = True
if debug:
    precommand = 'xterm_gdb'

run_melissa_da_study(
        runner_cmd='%s simulation2-pdaf' % precommand,
        total_steps=18,
        ensemble_size=9,
        assimilator_type=ASSIMILATOR_PDAF,
        cluster=LocalCluster(),
        procs_server=3,
        procs_runner=2,
        n_runners=3,
        show_server_log = True,
        show_simulation_log = True,
        additional_server_env={
            "PDAF_FILTER_NAME": "EnKF",
            "INIT_TYPE": "RANDOM"
            },
        additional_env = {
            "NX": "10",
            "NY": "10"
            },
        precommand_server = precommand)
