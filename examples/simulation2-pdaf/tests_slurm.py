from melissa_da_study import *
import subprocess

clean_old_stats()

run_melissa_da_study(
        runner_cmd='simulation2-pdaf',
        total_steps=18,
        ensemble_size=9,
        assimilator_type=ASSIMILATOR_PDAF,
        cluster=SlurmCluster('prcoe03', 'devel'),
        procs_server=3,
        procs_runner=2,
        n_runners=3,
        show_server_log = False,
        show_simulation_log = False,
        config_fti_path='./config.fti',
        walltime='00:02:00')

exit(subprocess.call(["bash", "test.sh"]))
