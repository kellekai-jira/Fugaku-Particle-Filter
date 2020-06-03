from cluster import cluster
import os
import subprocess

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

    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        # TODO: use annas template engine here instead of this function!
        assert n_nodes == 1  # as we are local


        additional_env_parameters = ''
        for name, value in additional_env.items():
            additional_env_parameters += self.env_variable_pattern % (name, value)

        run_cmd = '%s -n %d %s %s' % (
                self.mpiexec,
                n_procs,
                additional_env_parameters,
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

    def CheckJobState(self, job_id):
        state = 0
        ret_code = subprocess.call(["ps", str(job_id)], stdout=subprocess.DEVNULL)
        if ret_code == 0:
            state = 1
        else:
            state = 2
        #logging.debug('Checking for job_id %d: state: %d' % (job_id, state))
        return state

    def KillJob(self, job_id):
        os.system('kill '+str(job_id))

    def GetLoad(self):
        """number between 0 and 1"""
        return 0.5

    def CleanUp(self, runner_executable):
        os.system('killall melissa_server')
        os.system('killall gdb')
        os.system('killall xterm')
        os.system('killall %s' % self.mpiexec)
        #os.system('killall python3')
        os.system('killall %s' % runner_executable)
