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
        n_runners=1,
        show_server_log = False,
        show_simulation_log = False,
        additional_server_env = {
          "SCOREP_ENABLE_TRACING": "TRUE",
          "SCOREP_EXPERIMENT_DIRECTORY": "scorep-melissa-server",
          "SCOREP_FILTERING_FILE": "/p/home/jusers/friedemann1/juwels/workspace/melissa-da/profiling/filter_scorep_update_step_pdaf_enkf"
          },
        walltime='00:02:00')

