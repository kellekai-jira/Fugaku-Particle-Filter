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

class SlurmJuwelsCluster(cluster.SlurmCluster):

    def __init__(self, account, partition=None, in_salloc=(os.getenv('SLURM_JOB_ID') is not None), reserve_jobs_outside_salloc=False, max_devel_nodes=-1):
        """
        Arguments:

        in_salloc {bool}                        True if running within salloc. This means a node list must be given to each job submission to avoid oversubscription
        account {str}                           slurm account to use
        partition {str}                         slurm partition to use if not empty
        reserve_jobs_outside_salloc {bool}      True if we permit to reserve jobs outside the own salloc allocation.
        max_devel_nodes{int}                    On juwels you can only have e.g. 8 active devel nodes. Thus if this number is > 0 only max_devel_nodes jobs are started in the devel partition. Otherwise no partition argument is set

        """

        self.devel_nodes = 0
        self.max_devel_nodes = max_devel_nodes

        super().__init__(account, partition, in_salloc, reserve_jobs_outside_salloc, max_ranks_per_node=48)


        if self.in_salloc:
            assert partition == None  # Partition must not be defined if in salloc on juwels




    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        SlurmJuwelsCluster.check_walltime(walltime)
        # TODO: use annas template engine here instead of this function!


        # --exclusive should also do the job to not oversubscribe in job steps since srun documentation

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


        partition_param = ''
        if self.partition:
            partition_param = '--partition=%s' % self.partition
            if 'devel' in partition_param:
                self.devel_nodes += n_nodes
                if self.max_devel_nodes > 0 and self.devel_nodes > self.max_devel_nodes:
                    partition_param = ''  # start as normal job if already too many devel jobs as users on slurm cannot have momre than e.g. 8 devel nodes.


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
            print("in salloc, using pid:", pid)
            for k in cores:
                self.core_occupation[k]['job_id'] = pid
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
