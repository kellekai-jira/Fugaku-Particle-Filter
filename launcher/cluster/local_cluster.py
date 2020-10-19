from cluster import cluster
import os
import subprocess
import sys

import logging

class LocalCluster(cluster.Cluster):
    def __init__(self):
        # figure out some stuff on mpiexec....
        # REM: this is executed twice. First to generate the standard argument and then
        # again if it is used. Maybe it would be a good idea to check for the command
        # line argument of environment variables on install instead?
        self.env_variable_pattern = ' -x %s=%s '
        self.mpiexec = os.getenv('MPIEXEC')

        cmd = "%s -n 1 %s echo Welcome" % (self.mpiexec, self.env_variable_pattern % ("A", "42"))
        r = subprocess.run(cmd.split())
        if r.returncode != 0:
            print("Executing %s returned not 0. Assuming MPICH launcher."  % cmd)
            # assume mpich:
            self.env_variable_pattern = ' -genv %s %s '

        self.jobs = {}


    def __del__(self):
        if self.jobs == {}:
            return

        pid = os.getpid()
        msg = 'pid={:d}: LocalCluster: jobs {} not cleaned up'
        print(msg.format(pid, jobs.keys()), file=sys.stderr)


    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        # TODO: use annas template engine here instead of this function!
        assert n_nodes == 1  # as we are local


        additional_env_parameters = ''
        for key, value in additional_env.items():
            additional_env_parameters += self.env_variable_pattern % (key, value)

        run_cmd = '%s -n %d %s %s' % (
                self.mpiexec,
                n_procs,
                additional_env_parameters,
                cmd)

        print("Launching %s" % run_cmd)

        if logfile == '':
            job = subprocess.Popen(run_cmd.split())
        else:
            with open(logfile, 'wb') as f:
                job = subprocess.Popen(run_cmd.split(), stdout=f)

        self.jobs[job.pid] = job

        print("Launched {:s} pid={:d}".format(name, job.pid))
        return job.pid


    def CheckJobState(self, job_id):
        if not job_id in self.jobs:
            return 2

        job = self.jobs[job_id]

        if job.poll() is None:
            return 1
        else:
            del self.jobs[job_id]
            return 2


    def KillJob(self, job_id):
        if not job_id in self.jobs:
            print('no job found with id {:d}'.format(job_id), file=sys.stderr)
            return

        job = self.jobs[job_id]
        job.terminate()
        job.wait()
        del self.jobs[job_id]


    def GetLoad(self):
        """number between 0 and 1"""
        return 0.5

    def CleanUp(self, _):
        return
        pid = os.getpid()
        for job_id in self.jobs:
            j = self.jobs[job_id]

            if j.poll() is None:
                msg = 'pid={:d}: LocalCluster terminating process {:d}'
                print(fmt.format(pid, j.pid))
                j.terminate()
                j.wait()

        self.jobs = {}
