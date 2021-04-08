#from cluster import cluster
import cluster
import os
import subprocess
import multiprocessing
from ClusterShell.NodeSet import NodeSet

#import logging
import time
import re

EMPTY = 0
SERVER = 1
SIMULATION = 2
OCCUPIED = 3

class SlurmMn4Cluster(cluster.Cluster):

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
        self.server_executable = 'melissa_server'
        self.server_nodes = NodeSet()

        # REM: if using in_salloc and if a job quits nodes are not freed.
        # So they cannot be subscribed again. Unfortunately there is no way to figure out the status of nodes in a salloc if jobs are running on them or not (at least i did not find.)
        # see the git stash for some regex  parsing to get the node allocation...

        self.reset_nodelist()

    def reset_nodelist(self):
        if self.in_salloc:
            self.node_occupation = {}
            self.server_nodes = NodeSet()
            # alternative: compare to https://docs.ray.io/en/latest/deploying-on-slurm.html
            # to get the node names...

            out = subprocess.check_output(['scontrol', 'show', 'hostname'])
            for line in out.split(b'\n'):
                if line != b'':
                    hostname = (line.split(b'.')[0]).decode("utf-8")
                    self.node_occupation[hostname] = EMPTY


    def set_nodes_to(self, kind, n_nodes, n_procs, n_cpus_per_proc):
        to_place = n_nodes
        nodes = set()
        n_procs_per_node=n_procs/n_nodes
        for k, v in self.node_occupation.items():
            if self.check_node_status(k, n_procs_per_node, n_cpus_per_proc, kind) == EMPTY:
                self.node_occupation[k] = kind
                nodes.add(k)
                to_place -= 1
                if kind == SERVER:
                    self.server_nodes.add(k)

        # TODO implement allocation of additional nodes if not enough space
        # https://slurm.schedmd.com/faq.html -> 'Can I change my job's size after it has started running?'
            if to_place == 0:
                break
        print("start on node: ", nodes, flush=True)
        assert to_place == 0  # otherwise did not found enough nodes in reservation...

        return list(nodes)



    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        SlurmMn4Cluster.check_walltime(walltime)
        # TODO: use annas template engine here instead of this function!

        n_cpus_per_proc = 1

        for key, value in additional_env.items():
            os.environ[key] = value

        node_list_param = ''

        if self.in_salloc:
            # Generate node_list
            if is_server:
                nodes = self.set_nodes_to(SERVER, n_nodes, n_procs, 2)
            else:
                nodes = self.set_nodes_to(SIMULATION, n_nodes, n_procs, 1)

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

        run_cmd = 'srun --immediate --exclusive --verbose -N %d -n %d --cpus-per-task=%d --ntasks-per-node=%d %s --time=%s --account=%s %s %s --job-name=%s %s' % (
                n_nodes,
                n_procs,
                n_cpus_per_proc,
                n_procs//n_nodes,
                node_list_param,
                walltime,
                self.account,
                partition_param,
                output_param,
                name,
                cmd)

        print("Launching %s ..." % run_cmd, flush=True)
        time.sleep(1)
        print("[done]", flush=True)

        # if logfile == '':
        with open('log.out', 'wb') as f:
            proc = subprocess.Popen(run_cmd.split(), stdout=f, stderr=subprocess.PIPE)

        if self.in_salloc:
            print("in salloc, using pid:", proc.pid, flush=True)
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
                print('extracted jobid:', job_id, flush=True)
                return job_id
            time.sleep(0.05)

    def IsServerNode(self, node):
        #out = subprocess.check_output("sacct -j "
        #        + os.environ.get("SLURM_JOB_ID")
        #        + "  | egrep '"
        #        + self.server_executable
        #        + ".*RUNNING' | awk '{print $1}'", shell=True)

        #jobid = out.decode("utf-8").rstrip()

        #out = subprocess.check_output("sacct -j "
        #        + jobid
        #        + " --format=NodeList%3000 --noheader | xargs", shell=True)

        #nodeset = NodeSet(out.decode("utf-8").rstrip())
        return self.server_nodes.__contains__(node)

    def SetServerExecutable(self, name):
        self.server_executable = name

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
            #os.system('kill '+str(job_id))
            return

        print("scancel", job_id, flush=True)
        subprocess.call(['scancel', job_id])

    def GetLoad(self):
        """number between 0 and 1"""
        # TODO: put a reasonable value... depending on the research on some metric...
        return 0.5

    def CleanUp(self, runner_executable):
        if self.in_salloc:
            subprocess.call(['srun', '-n', '1', 'bash', '-c', 'killall %s; killall melissa_server' %
                runner_executable])
            # REM: the following is not done to gracefully end serverjobs.... (and finish e.g. their trace writing...) TODO: but do something like this on ctrl c :
        # for to_cancel in self.started_jobs:
            # subprocess.call(['scancel', str(to_cancel)])

    def check_node_status(self, node, n_procs, n_cpus_per_proc, kind):
        FNULL = open(os.devnull, 'w')
        #ncpus = self.get_ncpus_per_node()

        if kind != SERVER and self.IsServerNode(node):
            return OCCUPIED

        run_cmd = 'srun --immediate --exclusive -N 1 -n %d --cpus-per-task=%d --nodelist=%s  echo foo' % (
                n_procs,
                n_cpus_per_proc,
                node )
        print(run_cmd, flush=True)
        if subprocess.call(run_cmd.split(),stdout=FNULL,stderr=FNULL) == 0:
            print("node " + node + " available", flush=True)
            return EMPTY
        else:
            print("node " + node + " NOT available", flush=True)
            return OCCUPIED

    def get_ncpus_per_node(self):
        try:
            return int(os.environ["SLURM_JOB_CPUS_PER_NODE"])
        except (KeyError, ValueError) as e:
            return multiprocessing.cpu_count()


