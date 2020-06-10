#from cluster import cluster
import cluster
import os
import subprocess

import logging
import time
import re

EMPTY = 0
SERVER = 1
SIMULATION = 2

class SlurmCluster(cluster.Cluster):

    def check_walltime(wt):
        p = re.compile("\d\d?:\d\d?:\d\d?");
        assert p.match(wt)  # no valid slurm walltime

    def __init__(self, account, partition=None, in_salloc=(os.getenv('SLURM_JOB_ID') is not None)):
        """
        Arguments:

        in_salloc {bool}              True if running within salloc. This means a node list must be given to each job submission to avoid oversubscription
        account {str}                 slurm account to use
        partition {str}               slurm partition to use if not empty

        """

        self.account = account
        self.partition = partition
        self.in_salloc = in_salloc
        self.started_jobs = []

        # REM: if using in_salloc and if a job quits nodes are not freed.
        # So they cannot be subscribed again. Unfortunately there is no way to figure out the status of nodes in a salloc if jobs are running on them or not (at least i did not find.)
        # see the git stash for some regex  parsing to get the node allocation...

        if self.in_salloc:
            self.node_occupation = {}
            # alternative: compare to https://docs.ray.io/en/latest/deploying-on-slurm.html
            # to get the node names...

            out = subprocess.check_output(['srun', 'hostname'])
            for line in out.split(b'\n'):
                if line != b'':
                    hostname = (line.split(b'.')[0]).decode("utf-8")
                    self.node_occupation[hostname] = EMPTY


    def set_nodes_to(self, kind, n_nodes):
        to_place = n_nodes
        nodes = set()
        for k, v in self.node_occupation.items():
            if v == EMPTY:
                self.node_occupation[k] = kind
                nodes.add(k)
                to_place -= 1

            if to_place == 0:
                break
        assert to_place == 0  # otherwise did not found enough nodes in reservation...

        return list(nodes)



    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        SlurmCluster.check_walltime(walltime)
        # TODO: use annas template engine here instead of this function!

        for key, value in additional_env.items():
            os.environ[key] = value

        node_list_param = ''

        if self.in_salloc:
            # Generate node_list
            if is_server:
                nodes = self.set_nodes_to(SERVER, n_nodes)
            else:
                nodes = self.set_nodes_to(SIMULATION, n_nodes)

            #n = []
            # for node in nodes:
                # n += [node]*(n_procs//n_nodes)

            #node_list_param = '--nodelist=%s' % (','.join(n))
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

        # if logfile == '':
        proc = subprocess.Popen(run_cmd.split(), stderr=subprocess.PIPE)

        if self.in_salloc:
            print("in salloc, using pid:", proc.pid)
            return proc.pid
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
                job_id = res.groups()[0]
                self.started_jobs.append(job_id)
                print('extracted jobid:', job_id)
                return job_id
            time.sleep(0.05)





    def CheckJobState(self, job_id):
        state = 0
        if self.in_salloc:
            ret_code = subprocess.call(["ps", str(job_id)], stdout=subprocess.DEVNULL)
            if ret_code == 0:
                state = 1
            else:
                state = 2
            #logging.debug('Checking for job_id %d: state: %d' % (job_id, state))
            return state

        # for a strange reason this cannot handle jobsteps (jobid 1234.45)
        # Thus the killing mechanism failes at the end for such things after a successful
        # run
        proc = subprocess.Popen(["squeue", "-o %T", "--job=%s" % job_id],
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
            return 0
        elif in_out(["CONFIGURING", "RUNNING"]):
            return 1
        else:
            return 2

    def KillJob(self, job_id):
        if self.in_salloc:
            os.system('kill '+str(job_id))
            return

        print("scancel", job_id)
        subprocess.call(['scancel', job_id])

    def GetLoad(self):
        """number between 0 and 1"""
        # TODO: put a reasonable value... depending on the research on some metric...
        return 0.5

    def CleanUp(self, runner_executable):
        if self.in_salloc:
            subprocess.call(['srun', 'bash', '-c', 'killall %s; killall melissa_server' %
                runner_executable])
            # REM: the following is not done to gracefully end serverjobs.... (and finish e.g. their trace writing...) TODO: but do something like this on ctrl c :
        # for to_cancel in self.started_jobs:
            # subprocess.call(['scancel', str(to_cancel)])
