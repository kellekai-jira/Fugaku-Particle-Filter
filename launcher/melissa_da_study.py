
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
import inspect
import configparser
import psutil

from cluster import *

from utils import *

start_time = int(time.time()*1000)  # in milliseconds

# These variables are only used in this file.
melissa_da_path = os.getenv('MELISSA_DA_PATH')
assert melissa_da_path
server_may_restart = (os.getenv('WITH_FTI_CHECKOINT_DA_SERVER') == 'TRUE')
melissa_da_datadir = os.getenv('MELISSA_DA_DATADIR')
assert melissa_da_datadir


def cluster_selector():
    return FugakuCluster()
    #hn = socket.gethostname()
    #print('hostname:', hn)
    #if 'juwels' in hn or 'jwlogin' in hn:
    #    return SlurmJuwelsCluster(account='prcoe03')
    #else:
    #    return LocalCluster()


def run_melissa_da_study(
        runner_cmd='simulation1',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=cluster_selector(),  # TODO: replace this by a class that contains all the necessary methods taken from annas batch spawner
        procs_server=1,
        procs_runner=1,
        n_runners=1,  # may be a function if the allowed runner amount may change over time
        runner_group_size=1,
        local_ckpt_dir='../Local',
        global_ckpt_dir='../Global',
        meta_ckpt_dir='../Meta',
        show_server_log=True,
        show_simulation_log=True,
        config_fti_path=os.path.join(melissa_da_datadir, "config.fti"),
        server_slowdown_factor=1,  # the higher this number the slower the server. 0 is minimum...
        runner_timeout=10,
        server_timeout=30,
        additional_server_env={},
        create_runner_dir=False,
        precommand_server='',
        nodes_server=1,
        nodes_runner=1,
        walltime='xxxx01:00:00',
        prepare_runner_dir=None,  # is executed within the runner dir before the runner is launched. useful to e.g. copy config files for this runner into this directory...
        additional_env={},
        is_p2p=False,
        server_cmd=''):

    global server_may_restart

    assert isinstance(cluster, Cluster)

    # Test that procs can be nicely partitioned on different nodes
    assert (procs_runner / nodes_runner ) % 1 == 0
    assert (procs_server / nodes_server ) % 1 == 0

    old_cwd = os.getcwd()
    WORKDIR = old_cwd + '/STATS'

    if (not os.path.isdir(WORKDIR)):
        os.mkdir(WORKDIR)

    if server_may_restart:
        copyfile(config_fti_path, WORKDIR+"/config.fti")

    os.chdir(WORKDIR)

    start_logging(WORKDIR)

    def log_study_args(frame, l):
        args, _, _, _ = inspect.getargvalues(frame)
        tmp = {}
        for a in args:
            tmp[a] = l[a]
        log("Study - options: %s" % str(tmp))
    log_study_args(inspect.currentframe(), locals())


    if is_p2p:
        server_may_restart = True  # p2p uses pickle for restart and thus the server may always restart
        additional_env['MELISSA_DA_IS_P2P'] = '1'
        # FIXME: install server.py and call via python -m
        # FIXME: use cmake's found python!
        if server_cmd == '':
            server_cmd = 'python3 -u %s/server-p2p/server.py' % os.getenv('MELISSA_DA_SOURCE_PATH')

        assert 'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE' in additional_env

        assert procs_runner >= 2
        procs_server = 1
        nodes_server = 1
        create_runner_dir = True
        print("Running in p2p mode. Ignoring given procs_server, nodes_server, assimilator_type, config_fti_path, create_runner_dir, server_slowdown_factor arguments!")
        assert procs_runner / nodes_runner == int(procs_runner / nodes_runner)
    else:
        assert not 'MELISSA_DA_IS_P2P' in additional_env
        if server_cmd == '':
            server_cmd = 'melissa_da_server'

    if callable(n_runners):
        max_runners = n_runners
    else:
        max_runners = lambda : n_runners

    assert max_runners() >= 1


    # TODO: dirty: setting global variables. Use a class variable or sth like this...

    RUNNER_CMD = runner_cmd
    # split away arguments and take only the last bit of the path.
    EXECUTABLE = runner_cmd.split(' ')[0].split('/')[-1]

    running = True

    def signal_handler(sig, _):
        debug("Received Signal %d, Cleaning up now!" % sig)
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

   # TODO: do we have to do sth to replace this?    melissa_study.set_simulation_timeout(runner_timeout)        # simulations have runner_timeout seconds to start up


    class Job:
        jobs = []

        def __init__(self, job_id):
            self.job_id = job_id
            self.state = STATE_WAITING
            self.cstate = STATE_WAITING

            jobs = [j for j in Job.jobs if j.cstate != STATE_STOP]
            jobs.append(self)
            Job.jobs = jobs

        def check_state(self):
            return self.cstate

        def remove(self):
            if self in Job.jobs:
                Job.jobs.remove(self)
                if hasattr(self, 'job_id'):
                    debug("Killing Job job_id=%s" % str(self.job_id))
                    cluster.KillJob(self.job_id)


        def __del__(self):
            if hasattr(self, 'job_id'):
                debug("Killing Job job_id=%s" % str(self.job_id))
                cluster.KillJob(self.job_id)

    Job.jobs = []

    def refresh_states():
        while running:
            jobs = [j for j in Job.jobs if j.cstate != STATE_STOP]
            for j in jobs:
                j.cstate = cluster.CheckJobState(j.job_id)

            time.sleep(.1)
    state_refresher = defer(refresh_states)

    def num_launched_runners():
        return sum(list(map(lambda x: len(runners[x].runner_ids), runners.keys()))) if runners else 0

    def launched_runners():
        return [x for runner in runners.keys() for x in runners[runner].runner_ids]

    def runner_group_of( runner_id ):
        key_idx = np.where(list(map(lambda x: runner_id in runners[x].runner_ids, runners.keys())))[0]
        if key_idx.size == 0:
            return None
        else:
            keys = list(runners.keys())
            group_id = keys[key_idx[0]]
            if removed_runner:
                debug('in runner_group_of')
                debug(' -> key idx: %s' % ( key_idx[0] ))
                debug(' -> group id: %s' % ( group_id ))
                debug(' -> runner id: %s' % ( runner_id))
                debug(' -> runners: %s' % ( runners))
                for key in runners.keys():
                    debug('   [%s] runner_ids: %s' % (key, runners[key].runner_ids))
            assert len(key_idx) == 1
            return group_id


    MAX_SERVER_STARTS = 3
    class Server(Job):
        def __init__(self):
            self.node_name = ''
            self.last_msg_to = time.time()
            self.last_msg_from = time.time()
            Server.starts += 1
            assert Server.starts < 2 or server_may_restart  # can't restart server without fti! TODO: cleanly kill everything in this case!

            if Server.starts > MAX_SERVER_STARTS:
                print('Tail of server logs for diagnostics:')
                os.system('tail server.log.1')
                raise Exception("Too many server restarts!")



            options = [total_steps, ensemble_size, assimilator_type,
                               runner_timeout, server_slowdown_factor]

            launcher_node_name = get_node_name()
            options.append(launcher_node_name)

            debug('filling: %s' % str(options))
            cmd_opt = '%d %d %d %d %d %s' % tuple(options)

            cmd = '%s %s %s' % (
                    precommand_server,
                    server_cmd,
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

            job_id = cluster.ScheduleJob('melissa_da_server',
                    walltime, procs_server, nodes_server, cmd, envs, logfile, is_server=True)
            Job.__init__(self, job_id)

    Server.starts = 0

    class Runner(Job):
        def __init__(self, group_id, runner_ids, server_node_name):
            self.runner_ids = runner_ids
            self.__group_size = len(runner_ids)

            precommand = 'xterm_gdb'
            precommand = ''

            cmd = '%s %s' % (
                    precommand,
                    RUNNER_CMD
                    )

            melissa_server_master_node = 'tcp://%s:4000' % server_node_name
            melissa_server_master_gp_node = 'tcp://%s:4001' % server_node_name

            logfile = ''
            if not show_simulation_log:
                logfile = '%s/runner-group-%03d.log' % (WORKDIR, group_id)

            print(logfile)
            additional_runner_env = {}
            coll_procs_runner = 0
            coll_nodes_runner = 0

            if create_runner_dir:
                runner_dir = '%s/runner-group-%03d' % (WORKDIR, group_id)
                os.mkdir(runner_dir)
                os.chdir(runner_dir)

            for runner_id in runner_ids:

                coll_procs_runner += procs_runner
                coll_nodes_runner += nodes_runner

                if is_p2p:
                    # Setup FTI config for runner
                    runner_config = 'config-%03d.fti' % (runner_id)
                    shutil.copy(os.path.join(melissa_da_datadir, 'config-p2p-runner.fti'), runner_config)
                    config = configparser.ConfigParser()
                    config.read(runner_config)
                    if nodes_runner >= 1:
                        config['basic']['node_size'] = str(procs_runner//nodes_runner)
                    else:
                        config['basic']['node_size'] = str(procs_runner)
                    config['basic']['ckpt_dir'] = local_ckpt_dir
                    config['basic']['glbl_dir'] = global_ckpt_dir
                    config['basic']['meta_dir'] = meta_ckpt_dir
                    config['advanced']['local_test'] = '1' if issubclass(type(cluster), LocalCluster) else '0'
                    with open(runner_config, 'w') as f:
                        config.write(f)

                if prepare_runner_dir is not None:
                    if (len(inspect.getargspec(prepare_runner_dir).args) == 1):
                        prepare_runner_dir(runner_id)
                    else:
                        # support old api:
                        prepare_runner_dir()
                if not additional_runner_env:
                    additional_runner_env = {
                        "MELISSA_SERVER_MASTER_NODE": melissa_server_master_node,
                        "MELISSA_SERVER_MASTER_GP_NODE":
                            melissa_server_master_gp_node,
                        "MELISSA_TIMING_NULL": str(start_time),
                        "MELISSA_DA_RUNNER_ID": str(runner_id)
                        }
                    lib_path = os.getenv('LD_LIBRARY_PATH')
                    if lib_path != '':
                        additional_runner_env['LD_LIBRARY_PATH'] = lib_path
                else:
                    content = additional_runner_env["MELISSA_DA_RUNNER_ID"]
                    additional_runner_env["MELISSA_DA_RUNNER_ID"] = content + "," + str(runner_id)

            envs = additional_env.copy()
            join_dicts(envs, additional_runner_env)

            job_id = cluster.ScheduleJob(EXECUTABLE, walltime, coll_procs_runner, coll_nodes_runner, cmd, envs, logfile, is_server=False)

            os.chdir(WORKDIR)

            self.start_running_time = dict( zip( runner_ids, [-1] * self.__group_size ) )
            self.server_knows_it = dict( zip( runner_ids, [False] * self.__group_size ) )

            Job.__init__(self, job_id)

    init_sockets()


    removed_runner = False


    runners = {}  # running runners
    server = None
    next_runner_id = 0
    next_group_id = 0
    while running:
        time.sleep(0.1)  # chill down processor...

        if not server:  # No server. Start one!
            server = Server()
        if server.state == STATE_WAITING:
            if server.check_state() == STATE_RUNNING:
                server.state = STATE_RUNNING
                debug('Server running now!')
                server.last_msg_from = time.time()
                cluster.UpdateJob(server.job_id)
        if server.state == STATE_RUNNING:
            if server.node_name != '':
                mr = max_runners()
                active_runners = num_launched_runners()
                if active_runners < mr:  # TODO depend on check load here!
                    slots = mr - active_runners
                    group_size = runner_group_size if runner_group_size <= slots else slots
                    runner_ids = list( range(next_runner_id, next_runner_id + group_size) )
                    next_runner_id += group_size
                    group_id = next_group_id
                    next_group_id += 1
                    runners[group_id] = Runner(group_id, runner_ids, server.node_name)
                    debug('Starting runner(s) %s' % runner_ids)
                else:
                    while num_launched_runners() > mr:
                        to_remove = random.choice(list(runners))
                        log("killing a runner-group (with id=%d) as too many runners are up" % to_remove)
                        runners[to_remove].remove()
                        del runners[to_remove]
                        # TODO: notify server!


            # Check if the server did not timeout!
            if time.time() - server.last_msg_from > server_timeout or \
                server.check_state() != STATE_RUNNING:
                if server.check_state() != STATE_RUNNING:
                    error('Server Job not up anymore!')
                else:
                    error('Server timed out!')
                [runners[k].remove() for k in runners]
                runners.clear()
                # clear is sometimes not enough to kill all zombies so we call cleanup
                server.remove()
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
            for runner_id in launched_runners():
                group_id = runner_group_of(runner_id)
                if removed_runner:
                    debug('next iteration after runner was removed!')
                    debug(' -> group id: %s' % ( group_id))
                    debug(' -> runner id: %s' % ( runner_id))
                    debug(' -> runners: %s' % ( runners))
                    for key in runners.keys():
                        debug('   [%s] runner_ids: %s' % (key, runners[key].runner_ids))
                    removed_runner = False
                # runner_group was removed
                if group_id == None:
                    continue
                runner = runners[group_id]
                if runner.state == STATE_WAITING:
                    if runner.check_state() == STATE_RUNNING:
                        runner.state = STATE_RUNNING
                        debug('Runner(s) %s running now!' % runners[group_id].runner_ids)
                        runner.start_running_time = dict.fromkeys(runner.start_running_time, time.time())
                if runner.state == STATE_RUNNING:
                    if not runner.server_knows_it[runner_id] and \
                            time.time() - runner.start_running_time[runner_id] > runner_timeout:
                        error(('Runner group %d is killed as it did not register at the server'
                              + ' within %d seconds') % (group_id, runner_timeout))
                        runners[group_id].remove()
                        del runners[group_id]
                        removed_runner = True
                        debug('Runner was removed!')
                        debug(' -> group id: %s' % (group_id))
                        debug(' -> runner id: %s' % (runner_id))
                        debug(' -> runners: %s' % (runners))
                        for key in runners.keys():
                            debug('   [%s] runner_ids: %s' % (key, runners[key].runner_ids))
                    if runner.check_state() != STATE_RUNNING:
                        error('Runner group %d is killed as its job is not up anymore' %
                                group_id)
                        # TODO: notify server!
                        runners[group_id].remove()
                        del runners[group_id]
                        removed_runner = True
                        debug('Runner was removed!')
                        debug(' -> group id: %s' % (group_id))
                        debug(' -> runner id: %s' % (runner_id))
                        debug(' -> runners: %s' % (runners))
                        for key in runners.keys():
                            debug('   [%s] runner_ids: %s' % (key, runners[key].runner_ids))



            # Check messages from server
            server_msgs = get_server_messages()
            if len(server_msgs) > 0:
                server.last_msg_from = time.time()
            for msg in server_msgs:
                if 'runner_id' in msg and runner_group_of(msg['runner_id']) == None:
                    debug('omitting message concerning already dead runner %d' %
                            msg['runner_id'])
                    continue
                elif msg['type'] == MSG_SERVER_NODE_NAME:
                    log('Registering server')
                    server.node_name = msg['node_name']
                elif msg['type'] == MSG_TIMEOUT:
                    group_id = runner_group_of(msg['runner_id'])
                    error('Server wants me to crash runner-group %d' % group_id)
                    runners[group_id].remove()
                    del runners[group_id]
                elif msg['type'] == MSG_REGISTERED:
                    group_id = runner_group_of(msg['runner_id'])
                    runners[group_id].server_knows_it[msg['runner_id']] = True
                    cluster.UpdateJob(runners[group_id].job_id)
                elif msg['type'] == MSG_PING:
                    debug('got server ping')
                elif msg['type'] == MSG_STOP:
                    running = False
                    # TODO get error if python error occured!
                    log('Gracefully ending study now.')
                    [runners[k].remove() for k in runners]
                    runners.clear()
                    server.remove()
                    del server
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
        show_server_log=False,
        show_simulation_log=False)

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
           'SlurmCluster', 'LocalCluster', 'SlurmJuwelsCluster', 'FugakuCluster', 'SlurmMn4Cluster', # cluster
           'ASSIMILATOR_PDAF',
           'ASSIMILATOR_CHECK_STATELESS',
           'ASSIMILATOR_EMPTY',
           'ASSIMILATOR_DUMMY',
           'ASSIMILATOR_PRINT_INDEX_MAP',
           'ASSIMILATOR_WRF',
           'ASSIMILATOR_PYTHON']
