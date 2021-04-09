#from cluster import cluster
import cluster
import os
import subprocess

import logging
import time
import re
import copy

EMPTY = 0
SERVER = 1
SIMULATION = 2

class SlurmCluster(cluster.Cluster):

    def check_walltime(wt):
        p = re.compile("\d\d?:\d\d?:\d\d?");
        assert p.match(wt)  # no valid slurm walltime

    def __init__(self, account, partition=None, in_salloc=(os.getenv('SLURM_JOB_ID') is not None), reserve_jobs_outside_salloc=False, max_ranks_per_node=48):
        """
        Arguments:

        in_salloc {bool}                        True if running within salloc. This means a node list must be given to each job submission to avoid oversubscription
        account {str}                           slurm account to use
        partition {str}                         slurm partition to use if not empty
        reserve_jobs_outside_salloc {bool}      True if we permit to reserve jobs outside the own salloc allocation.
        max_ranks_per_node {int}                How many ranks can be allocated at a maximum per node. Only important if in_salloc is True

        """

        self.account = account
        self.partition = partition
        self.in_salloc = in_salloc
        self.started_jobs = []
        self.salloc_jobids = []
        self.reserve_jobs_outside_salloc = reserve_jobs_outside_salloc
        self.max_ranks_per_node = max_ranks_per_node

        # REM: if using in_salloc and if a job quits nodes are not freed.
        # So they cannot be subscribed again. Unfortunately there is no way to figure out the status of nodes in a salloc if jobs are running on them or not (at least i did not find.)
        # see the git stash for some regex  parsing to get the node allocation...

        if self.in_salloc:
            self.core_occupation = {}
            # alternative: compare to https://docs.ray.io/en/latest/deploying-on-slurm.html
            # to get the node names...
            out = subprocess.check_output(['scontrol', 'show', 'hostnames'])
            for line in out.split(b'\n'):
                if line != b'':
                    hostname = (line.split(b'.')[0]).decode("utf-8")
                    for proc in range(max_ranks_per_node):
                        self.core_occupation[(hostname, proc)] = {'kind': EMPTY, 'job_id': ""}


    def set_cores_to(self, kind, n_nodes, n_cores, wants_empty_node):
        assert n_nodes > 0
        assert n_cores > 0
        assert n_cores <= n_nodes * self.max_ranks_per_node # Prevent oversubscription
        to_place = self.max_ranks_per_node * n_nodes
        assert to_place % 1 == 0

        def empty_node(nodename):
            if wants_empty_node:
                return len(list(filter(lambda k: k[0] == nodename and self.core_occupation[k]['kind'] != EMPTY,
                    self.core_occupation))) == 0  # did not find anything else before on this node
            else:
                return True

        cores = set()
        tmp = copy.deepcopy(self.core_occupation)
        for k, v in tmp.items():
            if v['kind'] == EMPTY and empty_node(k[0]):
                tmp[k]['kind'] = kind
                cores.add(k)
                to_place -= 1

            if to_place == 0:
                break
        if to_place == 0:
            self.core_occupation = tmp
            return list(cores)
        else:  # did not find enough space in the reservation... reserver outside...

            print('trying to find a spot for cores,nodes:', n_cores, n_nodes)
            print(tmp)
            assert self.reserve_jobs_outside_salloc  # otherwise this is not permitted!
            return []




    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        SlurmCluster.check_walltime(walltime)
        # TODO: use annas template engine here instead of this function!

        for key, value in additional_env.items():
            os.environ[key] = value

        node_list_param = ''

        nodes = set()


        if self.in_salloc:
            # Generate node_list
            if is_server:
                cores = self.set_cores_to(SERVER, n_nodes, n_procs, True)
            else:
                cores = self.set_cores_to(SIMULATION, n_nodes, n_procs, False)
            nodes = set(map(lambda k: k[0], cores))

        if len(nodes) > 0:
            node_list_param = '--nodelist=%s' % ','.join(nodes)

        partition_param = ''
        if self.partition:
            partition_param = '--partition=%s' % self.partition

        output_param = ''
        if logfile != '':
            output_param = '--output=%s' % logfile


        run_cmd = 'srun --verbose -N %d -n %d --ntasks-per-node=%d %s --time=%s --account=%s %s %s --job-name=%s %s' % (
                n_nodes,
                n_procs,
                n_procs//n_nodes,
                node_list_param,
                walltime,
                self.account,
                partition_param,
                output_param,
                name,
                cmd)

        print("Launching %s" % run_cmd)

        # unset allocation id's in case we were just running this one outside...
        if self.in_salloc and len(nodes) == 0:
            slurm_allocation_id = os.getenv('SLURM_JOB_ID')
            del os.environ['SLURM_JOB_ID']
            del os.environ['SLURM_JOBID']

        proc = subprocess.Popen(run_cmd.split(), stderr=subprocess.PIPE)

        # reset allocation id's in case we were just running this one outside...
        if self.in_salloc and len(nodes) == 0:
            os.environ['SLURM_JOB_ID'] = slurm_allocation_id
            os.environ['SLURM_JOBID']  = slurm_allocation_id

        if self.in_salloc and len(nodes) > 0:
            pid = str(proc.pid)
            self.salloc_jobids.append(pid)
            for k in cores:
                self.core_occupation[k]['job_id'] = pid
            print("in salloc, using pid:", pid)
            for node in nodes:
                self.node_occupation[node]['job_id'] = pid
            return pid
        # else:
            # with open(logfile, 'wb') as f:
                # proc = subprocess.Popen(run_cmd.split(), stdout=f, stderr=subprocess.PIPE)

        #print("Process id: %d" % proc.pid)
        regex = re.compile('srun: launching ([0-9.]+) on')

        while proc.poll() is None:
            s = proc.stderr.read(128).decode()
            #print(s)
            res = regex.search(s)
            if res:
                job_id = str(res.groups()[0])
                self.started_jobs.append(job_id)
                print('extracted jobid:', job_id)
                return job_id
            time.sleep(0.05)





    def CheckJobState(self, job_id):
        state = cluster.STATE_WAITING
        if self.in_salloc and (job_id in self.salloc_jobids):
            ret_code = subprocess.call(["ps", job_id], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if ret_code == 0:
                state = cluster.STATE_RUNNING
            else:
                state = cluster.STATE_STOP
                for k, v in self.core_occupation.items():
                    if v['job_id'] == job_id:
                        self.core_occupation[k]['job_id'] = ""
                        self.core_occupation[k]['kind'] = EMPTY


            #logging.debug('Checking for job_id %d: state: %d' % (job_id, state))
            return state

        # for a strange reason this cannot handle jobsteps (jobid 1234.45)
        # Thus the killing mechanism failes at the end for such things after a successful
        # run
        proc = subprocess.Popen(["squeue", "-o %T", "--job=%s" % str(job_id)],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        out, err = proc.communicate()

        #print('Check job state output(%d): ' % job_id, out)

        def in_out(keywords):
            for kw in keywords:
                if kw in out:
                    return True
            return False


        if in_out(["PENDING"]):
            return cluster.STATE_WAITING
        elif in_out(["CONFIGURING", "RUNNING"]):
            return cluster.STATE_RUNNING
        else:
            return cluster.STATE_STOP

    def KillJob(self, job_id):
        if self.in_salloc and (job_id in self.salloc_jobids):
            os.system('kill '+job_id)
            # remove from node occupation!
            return

        print("scancel", job_id)
        subprocess.call(['scancel', job_id])

    def GetLoad(self):
        """number between 0 and 1"""
        # TODO: put a reasonable value... depending on the research on some metric...
        return 0.5


    def CleanUp(self, runner_executable):

        if self.in_salloc:
            subprocess.call(['srun', 'bash', '-c', 'killall %s; killall melissa_da_server; killall python3' %
                runner_executable])

            # REM: the following is not done to gracefully end serverjobs.... (and finish e.g. their trace writing...) TODO: but do something like this on ctrl c :
        # for to_cancel in self.started_jobs:
            # subprocess.call(['scancel', str(to_cancel)])
