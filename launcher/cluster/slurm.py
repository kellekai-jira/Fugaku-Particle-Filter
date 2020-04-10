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

        # REM: if using in_salloc and if a job quits nodes are not freed.
        # So they cannot be subscribed again. Unfortunately there is no way to figure out the status of nodes in a salloc if jobs are running on them or not (at least i did not find.)
        # see the git stash for some regex  parsing to get the node allocation...

        if self.in_salloc:
            self.node_occupation = {}
            out = subprocess.check_output(['srun', 'hostname'])
            for line in out.readlines():
                hostname = line.split('.')[0]
                self.node_occupation[hostname] = EMPTY


    def set_nodes_to(self, kind, n_nodes):
        to_place = n_nodes
        nodes = set()
        for k, v in self.node_occupation.items():
            if v == EMPTY:
                self.node_occupation[k] = kind
                nodes.add(k)

            if to_place == 0:
                break
        assert to_place == 0

        return ','.join(nodes)



    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        # TODO: use annas template engine here instead of this function!

        for name, value in additional_env.items():
            os.environ[name] = value

        node_list_param = ''

        if self.in_salloc:
            # Generate node_list
            if is_server:
                nodes = self.set_nodes_to(SERVER, n_nodes)
            else:
                nodes = self.set_nodes_to(SERVER, n_nodes)

            n = []
            for node in nodes:
                n.append([node]*(n_procs//n_nodes))

            node_list_param = '--nodelist=%s' % (','.join(n))



        partition_param = ''
        if self.partition:
            partition_param = '--partition=%s' % self.partition

        run_cmd = 'srun -N %d -n %d --ntasks-per-node=%d %s --time=%s --account=%s %s --job-name=%s %s' % (
                n_nodes,
                n_procs,
                n_procs//n_nodes,
                node_list_param,
                walltime,
                self.account,
                partition_param,
                name,
                cmd)

        print("Launching %s" % run_cmd)

        if logfile == '':
            proc = subprocess.Popen(run_cmd.split(), stderr=subprocess.PIPE)
        else:
            with open(logfile, 'wb') as f:
                proc = subprocess.Popen(run_cmd.split(), stdout=f, stderr=subprocess.PIPE)

        print("Process id: %d" % proc.pid)
        regex = re.compile('job ([0-9]+) queued')
        while proc.poll() is None:
            s = proc.stderr.read(64).decode()
            res = regex.search(s)
            if res:
                return int(res.groups()[0])
            time.sleep(0.05)





    def CheckJobState(self, job_id):
        state = 0
        proc = subprocess.Popen(["squeue", "-o %T", "--job=%d" % job_id],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              universal_newlines=True)
        out, err = proc.communicate()

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
            print("job seems to be not pending/running!")
            return 2

    def KillJob(self, job_id):
        subprocess.call(['scancel', str(job_id)])

    def GetLoad(self):
        """number between 0 and 1"""
        # TODO: put a reasonable value... depending on the research on some metric...
        return 0.5

    def CleanUp(self, runner_executable):
        if self.in_salloc:
            subprocess.call(['srun', 'bash', '-c', 'killall %s; killall melissa_server' %
                runner_executable])
