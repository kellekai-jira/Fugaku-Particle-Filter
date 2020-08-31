from melissa_da_study import *

import os
import repex
import resource


def run():
    global PARFLOW_DIR, preload_var, preload_path
    clean_old_stats()


    print('preparing run')
    # Open  ulimits...
    # see ulimit -a

    tmp = (resource.RLIM_INFINITY, resource.RLIM_INFINITY)
    resource.setrlimit(resource.RLIMIT_STACK, tmp)
    resource.setrlimit(resource.RLIMIT_DATA, tmp)

    # check:
    #os.system("ulimit -a")




    precommand = ''


    def prepare_runner_dir():
        """
        function that gets called from within the recently created runner dir before the
        runner is launched
        """
        pass


    RUNNERNODES = 5
    SERVERNODES = int(os.getenv('SLURM_JOB_NUM_NODES')) - RUNNERNODES  # ... start from 2
    print("Starting with %d runners and %d server nodes" % (RUNNERNODES, SERVERNODES))
    run_melissa_da_study(
            runner_cmd='simulation2-pdaf',  # will be executed in stats/runnner00x/. As pf writes output in the directory where the input file lies we need to copy it to our current directory
            total_steps=35,
            ensemble_size=1024,
            assimilator_type=ASSIMILATOR_PDAF,
            cluster=SlurmJuwelsCluster('prcoe03'),
            procs_server=48*SERVERNODES,
            nodes_server=SERVERNODES,
            procs_runner=24,
            n_runners=RUNNERNODES,
            show_server_log = False,
            show_simulation_log = False,
            runner_timeout = 800,
            create_runner_dir = False,
            precommand_server=precommand,
            prepare_runner_dir=prepare_runner_dir,
            walltime='00:30:00',
            additional_env = {
              "NX": "4032",
              "NY": "1000"
              },
            additional_server_env = {
              "PDAF_FILTER_NAME": "EnKF",
              "INIT_TYPE": "RANDOM"
              }
            )

# Run with repex:
HOME = os.getenv("HOME")
en = 'profiling-server-update-step'
repex.run(
        EXPERIMENT_NAME=en,
        INPUT_FILES=[HOME+'workspace/melissa-da/build/CMakeCache.txt', HOME+'workspace/melissa-da/melissa/build/CMakeCache.txt'],
        GIT_REPOS=[HOME+'/workspace/melissa-da'],
        experiment_function=run)
