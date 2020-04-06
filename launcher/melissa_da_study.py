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
import sys
import socket
import time
import subprocess
from shutil import copyfile

from launcher import melissa

from utils import *

import logging

# These variables are only used in this file.
melissa_da_path = os.getenv('MELISSA_DA_PATH')
assert melissa_da_path
melissa_with_fti = (os.getenv('MELISSA_DA_WITH_FTI') == 'TRUE')

# Assimilator types:
ASSIMILATOR_DUMMY = 0
ASSIMILATOR_PDAF = 1
ASSIMILATOR_EMPTY = 2
ASSIMILATOR_CHECK_STATELESS = 3

started_runners = 0  # as Python seems to not support closurs this has to be global.

def run_melissa_da_study(
        executable='simulation1',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster_name='local',  # TODO: replace this by a class that contains all the necessary methods taken from annas batch spawner
        procs_server=1,
        procs_runner=1,
        n_runners=1,
        show_server_log = True,
        show_simulation_log = True,
        config_fti_path = melissa_da_path + "/share/melissa-da/config.fti"):

    global started_runners
    started_runners = 0

    old_cwd = os.getcwd()
    WORKDIR = old_cwd + '/STATS'

    if (not os.path.isdir(WORKDIR)):
        os.mkdir(WORKDIR)

    if melissa_with_fti:
        copyfile(config_fti_path, WORKDIR+"/config.fti")

    os.chdir(WORKDIR)


    # The launch_server function to put in USER_FUNCTIONS['launch_server'].
    # It takes a Server object as argument, and must set its job_id attribute.
    # Here, we use the PID of the subprocess.
    # The server object provides two important attributes:
    #   path: the path to melissa_server executable
    #   cmd_opt: the options set by the launcher to pass to the server.


    def cluster_launch(n_procs, n_nodes, cmd, melissa_server_master_node='', logfile=''):
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
        pid = 0

        if logfile == '':
            pid = subprocess.Popen(run_cmd.split()).pid
        else:
            with open(logfile, 'wb') as f:
                pid = subprocess.Popen(run_cmd.split(), stdout=f).pid

        print("Process id: %d" % pid)
        return pid

    def launch_server(server):
        #precommand = 'xterm_gdb'
        precommand = ''

        # sometimes the server starts in a runner dir...

        cmd = '%s %s/bin/melissa_server %s' % (
                precommand,
                os.getenv('MELISSA_DA_PATH'),
                server.cmd_opt
                )

        # TODO: why not using return?
        logfile = '' if show_server_log else 'server.log'
        server.job_id = cluster_launch(server.cores, server.nodes, cmd, '', logfile)

    def restart_server(server):
        if (not show_server_log) and os.path.isfile('server.log'):
            # find new free server logfile name:
            i = 0
            def fn(i):
                return 'server.log.%d'%i
            while os.path.isfile(fn(i)):
                i += 1
            os.rename('server.log', fn(i))



        if melissa_with_fti:
            launch_server(server)
        else:
            # FIXME: gracefully shut down all runners!
            logging.info("Server cannot be recovered as melissa-da was not compiled using WITH_FTI")
            from launcher.simulation import FINISHED
            with server.lock:
                server.status = FINISHED
                server.want_stop = True
            logging.debug("Ending server now")




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

        cmd = '%s %s' % (
                precommand,
                EXECUTABLE_WITH_PATH
                )

        melissa_server_master_node = 'tcp://%s:4000' % group.server_node_name

        print('Starting runner! REM: the simulation group id != runner id!')
        logfile = '' if show_simulation_log else 'simulation-%03d.log' % group.group_id
        group.job_id = cluster_launch(group.cores, group.nodes, cmd, melissa_server_master_node, logfile)

        os.chdir(WORKDIR)

        global started_runners
        started_runners += 1

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
        #print('Checking for job_id %d: state: %d' % (job.job_id, state))
        job.job_status = state

    def check_load():
        global started_runners
        return started_runners < MAX_RUNNERS


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
    melissa_study.server.restart(restart_server)
    melissa_study.check_scheduler_load(check_load)
    melissa_study.cancel_job(kill_job)

    melissa_study.run()


    os.chdir(old_cwd)

    # flush print output to the console
    sys.stdout.flush()


def check_stateless(simulation_executable):  # TODO: do those guys without FTI maybe?
    clean_old_stats()
    run_melissa_da_study(
        executable=simulation_executable,
        total_steps=3,
        ensemble_size=1,
        assimilator_type=ASSIMILATOR_CHECK_STATELESS,
        cluster_name='local',
        procs_server=1,
        procs_runner=1,
        n_runners=1,
        show_server_log = False,
        show_simulation_log = False)

    with open('STATS/server.log', 'r') as f:
        for line in f.readlines():
            if '**** Check Successful' in line:
                print('Simulation %s seems stateless'
                        % simulation_executable)
                return True

    print('Simulation %s is stateful and thus cannot be used with melissa-da')
    return False

# exporting for import * :
__all__ = ['run_melissa_da_study', 'check_stateless', 'ASSIMILATOR_PDAF',
           'killing_giraffe', 'clean_old_stats',  # utils
           'ASSIMILATOR_CHECK_STATELESS',
           'ASSIMILATOR_DUMMY',
           'ASSIMILATOR_EMPTY',
           'ASSIMILATOR_DUMMY']
