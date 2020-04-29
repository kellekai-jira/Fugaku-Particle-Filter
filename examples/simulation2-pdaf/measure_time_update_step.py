from melissa_da_study import *
import shutil
import os
import time
str_t = time.strftime("%Y-%m-%d_%H:%M:%S")

max_runner = 3
nx = 100
ny = 10


folder = 'experiments/measurements_'+str_t
os.makedirs(folder)

if os.path.islink('last'):
    os.remove('last')
os.symlink(folder, 'last')


for i in range (1,10):
    members = 2**i
    print('=============================================')
    print ("Running for %d ensemble members" % members)
    clean_old_stats()
    run_melissa_da_study(
            runner_cmd='simulation2-pdaf',
            total_steps=35,
            ensemble_size=members,
            assimilator_type=ASSIMILATOR_PDAF,
            #cluster=LocalCluster(),
            cluster=SlurmCluster(account="prcoe03", partition="devel"),
            procs_server=2,
            procs_runner=2,
            n_runners=max_runner,
            show_server_log = False,
            show_simulation_log = False,
            additional_server_env={
                "PDAF_FILTER_NAME": "EnKF",
                "INIT_TYPE": "RANDOM"
                },
            additional_env = {
                "NX": "%d" % nx,
                "NY": "%d" % ny
                },
            walltime="00:10:00")

    shutil.copyfile('STATS/server.log', '%s/server.log.%d.members' % (folder, members))
    for runner in range(max_runner):
        try:
            shutil.copyfile('STATS/runner-%03d.log' % runner, '%s/runner-%03d.log.%d.members' % (folder, runner, members))
        except:
            print("Could not find runner%d's log. Probably it was not started before the study ended" % runner)

