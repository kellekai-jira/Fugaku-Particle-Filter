###################################################################
#                            Melissa                              #
#-----------------------------------------------------------------#
#   COPYRIGHT (C) 2017  by INRIA and EDF. ALL RIGHTS RESERVED.    #
#                                                                 #
# This source is covered by the BSD 3-Clause License.             #
# Refer to the  LICENCE file for further information.             #
#                                                                 #
#-----------------------------------------------------------------#
#  Original Contributors:                                         #
#    Theophile Terraz,                                            #
#    Bruno Raffin,                                                #
#    Alejandro Ribes,                                             #
#    Bertrand Iooss,                                              #
###################################################################

import os
import socket
import time
import subprocess

from launcher import melissa

# These variables are only used in this file.
melissa_da_path = os.getenv('MELISSA_DA_PATH')
assert melissa_da_path

# Assimilator types:
ASSIMILATOR_DUMMY = 0
ASSIMILATOR_PDAF = 1
ASSIMILATOR_EMPTY = 2
ASSIMILATOR_CHECK_STATELESS = 3


def run_melissa_da_study(
        executable='simulation1',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster_name='local',
        procs_server=1,
        procs_runner=1,
        n_runners=1):

    EXECUTABLE='simulation1'
    EXECUTABLE_WITH_PATH='simulation1'

    old_cwd = os.getcwd()
    WORKDIR = old_cwd + '/STATS'

    if (not os.path.isdir(WORKDIR)):
        os.mkdir(WORKDIR)

    os.chdir(WORKDIR)

    # The launch_server function to put in USER_FUNCTIONS['launch_server'].
    # It takes a Server object as argument, and must set its job_id attribute.
    # Here, we use the PID of the subprocess.
    # The server object provides two important attributes:
    #   path: the path to melissa_server executable
    #   cmd_opt: the options set by the launcher to pass to the server.


    def cluster_launch(n_procs, n_nodes, cmd, melissa_server_master_node=''):
        # TODO: use annas template engine here instead of this function!
        assert n_nodes == 1  # TODO: for the moment
        lib_path = os.getenv('LD_LIBRARY_PATH')

        # handle "". mpiexec -x implicitly adds the new library path to the existing one.
        # "" would lead to :"path1:path2" which cannot be read.
        if lib_path == '':
            lib_path ='""'

        if melissa_server_master_node == '':
            melissa_server_master_node ='""'

        run_cmd = '%s -n %d -x LD_LIBRARY_PATH=%s -x MELISSA_SERVER_MASTER_NODE=%s %s' % (
                os.getenv('MPIEXEC'),
                n_procs,
                lib_path,
                melissa_server_master_node,
                cmd)

        print("Launching %s" % run_cmd)
        pid = subprocess.Popen(run_cmd.split()).pid
        print("Process id: %d" % pid)
        return pid

    def launch_server(server):
        precommand = 'xterm_gdb'
        precommand = ''

        # sometimes the server starts in a runner dir...

        cmd = '%s %s/bin/melissa_server %s' % (
                precommand,
                os.getenv('MELISSA_DA_PATH'),
                server.cmd_opt
                )

        # TODO: why not using return?
        server.job_id = cluster_launch(server.cores, server.nodes, cmd)




# The launch_group function to put in USER_FUNCTIONS['launch_group'].
# It is used to launch batches of simulations (or groups of simulation the case of Sobol' indices computation).
# It takes a Group object as argument, and must set the job ID of the group of simulations in the attribute job_id of the Group object.
# This object provides three important attributes:
#   simu_id:
#   rank
#   param_set
# We distinguish three kinds of groups:

# Once we have set the job IDs of our jobs, we can use it to define the fault tolerance functions. In our case, we will use the same function for the server and the simulations. It takes a `Job` object as argument, and sets its `status` attribute to 0 if it is waiting to be scheduled, 1 if it is currently running, or 2 if it is not running anymore. In your local machine, a job will never be have a 0 status, because it is launched immediately when `USER_FUNCTIONS['launch_group']` is called.

    def launch_runner(group):
        precommand = 'xterm_gdb'
        precommand = ''

        runner_dir = '%s/runner%03d' % (WORKDIR, group.group_id)
        if (not os.path.isdir(runner_dir)):
            os.mkdir(runner_dir)
        os.chdir(runner_dir)

        cmd = '%s %s' % (
                precommand,
                EXECUTABLE_WITH_PATH
                )

        melissa_server_master_node = 'tcp://%s:4000' % group.server_node_name
        group.job_id = cluster_launch(group.cores, group.nodes, cmd, melissa_server_master_node)

        os.chdir(WORKDIR)

    def check_job(job):
        # Check the job state:
        # 0: not runing  TODO: use macros!  TODO: what's the difference between not running and not running anymore?
        # 1: running
        # 2: not running anymore (finished or crashed)
        state = 0
        try:
            subprocess.check_output(["ps", str(job.job_id)])
            state = 1
        except:
            state = 2
        # we set the job_status attribute of the Job object. Group and Server objects inherite of Job.
        print('Checking for job_id %d: state: %d' % (job.job_id, state))
        job.job_status = state

    MAX_RUNNERS=1
    PROCS_RUNNER=1
    def check_load():
        #time.sleep(1)
        # TODO: use max runners here!
        try:
            out = str(subprocess.check_output(["pidof", EXECUTABLE])).split()
        except:
            return True

        print('========= len out:%d/%d' % (len(out), MAX_RUNNERS* PROCS_RUNNER))
        if len(out) >= MAX_RUNNERS * PROCS_RUNNER:
        #if len(out) > 1:
            return False
        else:
            time.sleep(2)
            return True

    def kill_job(job):
        os.system('kill '+str(job.job_id))




    assert(cluster_name == 'local')  # cannot handle others atm!

    # TODO: dirty: setting global variables. Use a class variable or sth like this...

    EXECUTABLE_WITH_PATH = executable
    EXECUTABLE = executable.split('/')[-1]

    MAX_RUNNERS = n_runners
    PROCS_RUNNER = procs_runner

    def cleanup():
        os.system('killall melissa_server')
        os.system('killall gdb')
        os.system('killall xterm')
        os.system('killall mpiexec')
        os.system('killall %s' % EXECUTABLE)
    cleanup()





# dirty but convenient to kill stuff...
    import signal
    import sys

    def signal_handler(sig, frame):
        cleanup()

        sys.exit(1)

    signal.signal(signal.SIGINT, signal_handler)

    melissa_study = melissa.Study()
    melissa_study.set_working_directory(WORKDIR)

    melissa_study.set_simulation_timeout(400)      # simulations are restarted if no life sign for 400 seconds
    melissa_study.set_checkpoint_interval(300)     # server checkpoints every 300 seconds
    melissa_study.set_verbosity(3)                 # verbosity: 0: only errors, 1: errors + warnings, 2: usefull infos (default), 3: debug info

    # each runner is started seperately:
    melissa_study.set_batch_size(1)
    melissa_study.set_sampling_size(n_runners)  #  == n_runnners.

    melissa_study.set_assimilation(True)

# some secret options...:
    melissa_study.set_option('assimilation_total_steps', total_steps)
    melissa_study.set_option('assimilation_ensemble_size', ensemble_size)
    melissa_study.set_option('assimilation_assimilator_type', assimilator_type)  # ASSIMILATOR_DUMMY
    melissa_study.set_option('assimilation_max_runner_timeout', 5)  # seconds, timeout checked frin tge server sude,

    melissa_study.set_option('server_cores', procs_server)  # overall cores for the server
    melissa_study.set_option('server_nodes', 1)  # using that many nodes  ... on  a well defined cluster the other can be guessed probably. TODO: make changeable. best in dependence of cluster cores per node constant...

    melissa_study.set_option('simulation_cores', procs_runner)  # cores of one runner
    melissa_study.set_option('simulation_nodes', 1)  # using that many nodes

    melissa_study.simulation.launch(launch_runner)
    melissa_study.server.launch(launch_server)
    melissa_study.check_job(check_job)
    melissa_study.simulation.check_job(check_job)
    melissa_study.server.restart(launch_server)
    melissa_study.check_scheduler_load(check_load)
    melissa_study.cancel_job(kill_job)

    melissa_study.run()


    os.chdir(old_cwd)


# exporting for import * :
__all__ = ['run_melissa_da_study', 'ASSIMILATOR_PDAF', 'ASSIMILATOR_CHECK_STATELESS',
           'ASSIMILATOR_DUMMY', 'ASSIMILATOR_EMPTY', 'ASSIMILATOR_DUMMY']
