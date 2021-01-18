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
import time
from shutil import copyfile
import signal
import sys
import socket


from cluster import *

from utils import *

start_time = int(time.time()*1000)  # in milliseconds

# These variables are only used in this file.
melissa_da_path = os.getenv('MELISSA_DA_PATH')
assert melissa_da_path
melissa_with_fti = (os.getenv('MELISSA_DA_WITH_FTI') == 'TRUE')
melissa_da_datadir = os.getenv('MELISSA_DA_DATADIR')
assert melissa_da_datadir


def cluster_selector():
    hn = socket.gethostname()
    print('hostname:', hn)
    if 'juwels' in hn or 'jwlogin' in hn:
        return SlurmJuwelsCluster(account='prcoe03')
    else:
        return LocalCluster()


def run_melissa_da_study(
        runner_cmd='simulation1',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=cluster_selector(),  # TODO: replace this by a class that contains all the necessary methods taken from annas batch spawner
        procs_server=1,
        procs_runner=1,
        n_runners=1,
        show_server_log = True,
        show_simulation_log = True,
        config_fti_path = os.path.join(melissa_da_datadir, "config.fti"),
        server_slowdown_factor=1,  # the higher this number the slower the server. 0 is minimum...
        runner_timeout=5,
        server_timeout=30,
        additional_server_env={},
        create_runner_dir=False,
        precommand_server='',
        nodes_server=1,
        nodes_runner=1,
        walltime='xxxx01:00:00',
        prepare_runner_dir=None,  # is executed within the runner dir before the runner is launched. useful to e.g. copy config files for this runner into this directory...
        additional_env={}):

    assert isinstance(cluster, Cluster)

    old_cwd = os.getcwd()
    WORKDIR = old_cwd + '/STATS'

    if (not os.path.isdir(WORKDIR)):
        os.mkdir(WORKDIR)

    if melissa_with_fti:
        copyfile(config_fti_path, WORKDIR+"/config.fti")

    os.chdir(WORKDIR)

    start_logging(WORKDIR)







    # TODO: dirty: setting global variables. Use a class variable or sth like this...

    RUNNER_CMD = runner_cmd
    # split away arguments and take only the last bit of the path.
    EXECUTABLE = runner_cmd.split(' ')[0].split('/')[-1]


    cluster.CleanUp(EXECUTABLE)





    running = True
# dirty but convenient to kill stuff...

    def signal_handler(sig, frame):
        debug("Received Signal %d, Cleaning up now!" % sig)
        nonlocal running
        running = False

        if sig == signal.SIGTERM:
            debug("SIGTERM: trying to join state_refresher...")
            state_refresher.join()
            debug("State refresher joined")


        cluster.CleanUp(EXECUTABLE)

        sys.exit(1)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

   # TODO: do we have to do sth to replace this?    melissa_study.set_simulation_timeout(runner_timeout)        # simulations have runner_timeout seconds to start up


    class Job:
        def __init__(self, job_id):
            self.job_id = job_id
            self.state = STATE_WAITING
            self.cstate = STATE_WAITING
            Job.jobs.append(self)

        def check_state(self):
            return self.cstate

        def __del__(self):
            if hasattr(self, 'job_id'):
                debug("Killing Job job_id=%s" % str(self.job_id))
                cluster.KillJob(self.job_id)

    Job.jobs = []

    def refresh_states():
        while running:
            for job in Job.jobs:
                job.cstate = cluster.CheckJobState(job.job_id)

            time.sleep(.1)
    state_refresher = defer(refresh_states)


    MAX_SERVER_STARTS = 3
    class Server(Job):
        def __init__(self):
            self.node_name = ''
            self.last_msg_to = time.time()
            self.last_msg_from = time.time()
            Server.starts += 1
            assert Server.starts < 2 or melissa_with_fti  # can't restart server without fti!

            if Server.starts > MAX_SERVER_STARTS:
                raise Exception("Too many server restarts!")



            options = [total_steps, ensemble_size, assimilator_type,
                               runner_timeout, server_slowdown_factor]

            node_name = get_node_name()
            options.append(node_name)

            debug('filling: %s' % str(options))
            cmd_opt = '%d %d %d %d %d %s' % tuple(options)

            cmd = '%s melissa_server %s' % (
                    precommand_server,
                    cmd_opt
                    )

            envs = additional_env.copy()
            envs['MELISSA_TIMING_NULL'] = str(start_time)
            join_dicts(envs, additional_server_env)

            if not 'LD_LIBRARY_PATH' in envs:
                lib_path = os.getenv('LD_LIBRARY_PATH')
                if lib_path != '':
                    envs['LD_LIBRARY_PATH'] = lib_path


            logfile = '' if show_server_log else '%s/server.log.%d' % (WORKDIR, Server.starts)

            job_id = cluster.ScheduleJob('melissa_server',
                    walltime,  procs_server, nodes_server, cmd, envs, logfile, is_server=True)
            Job.__init__(self, job_id)

    Server.starts = 0

    class Runner(Job):
        def __init__(self, runner_id, server_node_name):
            #self.runner_id = runner_id

            precommand = 'xterm_gdb'
            precommand = ''

            cmd = '%s %s' % (
                    precommand,
                    RUNNER_CMD
                    )

            melissa_server_master_node = 'tcp://%s:4000' % server_node_name

            logfile = ''
            if not show_simulation_log:
                logfile = '%s/runner-%03d.log' % (WORKDIR, runner_id)
                if create_runner_dir:
                    runner_dir = '%s/runner-%03d' % (WORKDIR, runner_id)
                    os.mkdir(runner_dir)
                    os.chdir(runner_dir)

                if prepare_runner_dir is not None:
                    prepare_runner_dir()

            additional_runner_env = {
                    "MELISSA_SERVER_MASTER_NODE": melissa_server_master_node,
                    "MELISSA_TIMING_NULL": str(start_time),
                    "MELISSA_DA_RUNNER_ID": str(runner_id)
                    }
            lib_path = os.getenv('LD_LIBRARY_PATH')
            if lib_path != '':
                additional_runner_env['LD_LIBRARY_PATH'] = lib_path

            envs = additional_env.copy()
            join_dicts(envs, additional_runner_env)

            job_id = cluster.ScheduleJob(EXECUTABLE, walltime, procs_runner, nodes_runner, cmd, envs, logfile, is_server=False)

            os.chdir(WORKDIR)


            self.start_running_time = -1
            self.server_knows_it = False

            Job.__init__(self, job_id)


    init_sockets()


    runners = {}  # running runners
    server = None
    next_runner_id = 0
    while running:
        time.sleep(0.1)  # chill down processor...

        if not server:  # No server. Start one!
            server = Server()
        if server.state == STATE_WAITING:
            if server.check_state() == STATE_RUNNING:
                server.state = STATE_RUNNING
                debug('Server running now!')
                server.last_msg_from = time.time()
        if server.state == STATE_RUNNING:
            if server.node_name != '' and len(runners) < n_runners:  # TODO depend on check load here!
                runner_id = next_runner_id
                next_runner_id += 1
                runners[runner_id] = Runner(runner_id, server.node_name)

            # Check if the server did not timeout!
            if time.time() - server.last_msg_from > server_timeout or \
                server.check_state() != STATE_RUNNING:
                if server.check_state() != STATE_RUNNING:
                    error('Server Job not up anymore!')
                else:
                    error('Server timed out!')
                runners.clear()
                # clear is sometimes not enough to kill all zombies so we call cleanup
                del server
                server = None
                cluster.CleanUp(EXECUTABLE)
                continue

            # Check if we need to give a live sign to the server?
            if (time.time() - server.last_msg_to) > 10:
                melissa_comm4py.send_hello()
                # send nonblocking? use defer()... but actually zmq_send only queues the message... so even if it cannot be send directly this should not pose any problems. This might fix random bugs when it deadlocks because the server tries to send a message and the launcher tries to send a message to the server at the same time?
                debug('ping to server after %d s' % (time.time() - server.last_msg_to))
                server.last_msg_to = time.time()

            # Check if some runners are running now, timed out while registration or if
            # they were killed for some strange reasons...
            for runner_id in list(runners.keys()):
                runner = runners[runner_id]
                if runner.state == STATE_WAITING:
                    if runner.check_state() == STATE_RUNNING:
                        runner.state = STATE_RUNNING
                        debug('Runner %d running now!' % runner_id)
                        runner.start_running_time = time.time()
                if runner.state == STATE_RUNNING:
                    if not runner.server_knows_it and \
                            time.time() - runner.start_running_time > runner_timeout:
                        error(('Runner %d is killed as it did not register at the server'
                              + ' within %d seconds') % (runner_id, runner_timeout))
                        del runners[runner_id]
                    if runner.check_state() != STATE_RUNNING:
                        error('Runner %d is killed as its job is not up anymore' %
                                runner_id)
                        del runners[runner_id]


            # Check messages from server
            server_msgs = get_server_messages()
            if len(server_msgs) > 0:
                server.last_msg_from = time.time()
            for msg in server_msgs:
                if 'runner_id' in msg and not msg['runner_id'] in runners:
                    debug('omitting message concerning already dead runner %d' %
                            msg['runner_id'])
                    continue
                elif msg['type'] == MSG_SERVER_NODE_NAME:
                    log('Registering server')
                    server.node_name = msg['node_name']
                elif msg['type'] == MSG_TIMEOUT:
                    error('Server wants me to crash runner %d' % msg['runner_id'])
                    del runners[msg['runner_id']]
                elif msg['type'] == MSG_REGISTERED:
                    runners[msg['runner_id']].server_knows_it = True
                elif msg['type'] == MSG_PING:
                    debug('got server ping')
                elif msg['type'] == MSG_STOP:
                    running = False
                    log('Gracefully ending study now.')
                    runners.clear()
                    del server
                    # cluster.CleanUp(EXECUTABLE)
                    break


    finalize_sockets()
    state_refresher.join()
    os.chdir(old_cwd)

    # flush print output to the console
    cluster.CleanUp(EXECUTABLE)
    sys.stdout.flush()







def check_stateless(runner_cmd):  # TODO: do those guys without FTI maybe?
    clean_old_stats()
    run_melissa_da_study(
        runner_cmd=runner_cmd,
        total_steps=3,
        ensemble_size=1,
        assimilator_type=ASSIMILATOR_CHECK_STATELESS,
        cluster=LocalCluster(),
        procs_server=1,
        procs_runner=1,
        n_runners=1,
        show_server_log = False,
        show_simulation_log = False)

    with open('STATS/server.log.1', 'r') as f:
        for line in f.readlines():
            if '**** Check Successful' in line:
                log('Simulation %s seems stateless'
                        % runner_cmd)
                return True

    error('Simulation %s is stateful and thus cannot be used with melissa-da' % runner_cmd)
    return False


# exporting for import * :
__all__ = ['run_melissa_da_study', 'check_stateless', 'cluster_selector',
           'killing_giraffe', 'clean_old_stats',  # utils
           'SlurmCluster', 'LocalCluster', 'SlurmJuwelsCluster',  # cluster
           'ASSIMILATOR_PDAF',
           'ASSIMILATOR_CHECK_STATELESS',
           'ASSIMILATOR_EMPTY',
           'ASSIMILATOR_DUMMY',
           'ASSIMILATOR_PRINT_INDEX_MAP',
           'ASSIMILATOR_WRF',
           'ASSIMILATOR_PYTHON']
