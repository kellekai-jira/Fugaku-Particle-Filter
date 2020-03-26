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

# These variables are only used in this file.
melissa_da_path = os.getenv('MELISSA_DA_PATH')
assert melissa_da_path

EXECUTABLE='simulation1'
WORKDIR = os.getcwd() + '/STATS'

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
    total_steps = 10
    ensemble_size = 5
    assimilator_type = 0  # ASSIMILATOR_DUMMY
    max_runner_timeout = 5  # seconds
    n_procs_server = 1
    n_nodes_server = 1

    cmd = '%s %s/bin/melissa_server %d %d %d %d %s' % (
            precommand,
            os.getenv('MELISSA_DA_PATH'),
            total_steps,
            ensemble_size,
            assimilator_type,
            max_runner_timeout,
            socket.gethostname()
            )

    # TODO: why not using return?
    server.pid = cluster_launch(n_procs_server, n_nodes_server, cmd)




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
    n_nodes_simulation = 1
    n_procs_simulation = 1
    precommand = 'xterm_gdb'
    precommand = ''

    runner_dir = '%s/runner%03d' % (WORKDIR, group.group_id)
    if (not os.path.isdir(runner_dir)):
        os.mkdir(runner_dir)
    os.chdir(runner_dir)

    cmd = '%s %s/bin/%s' % (
            precommand,
            melissa_da_path,
            EXECUTABLE
            )

    melissa_server_master_node = 'tcp://%s:4000' % group.server_node_name
    group.job_id = cluster_launch(n_procs_simulation, n_procs_simulation, cmd, melissa_server_master_node)

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

def check_load():
    # We only run one group at a time
    #time.sleep(1)
    time.sleep(1)
    try:
        out = str(subprocess.check_output(["pidof", EXECUTABLE])).split()
    except:
        return True
    if len(out) > 3:
        return False
    else:
        time.sleep(2)
        return True

def kill_job(job):
    os.system('kill '+str(job.job_id))
