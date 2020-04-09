from cluster import cluster
import os
import subprocess

import logging

class LocalCluster(cluster.Cluster):
    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        # TODO: use annas template engine here instead of this function!
        assert n_nodes == 1  # as we are local

        additional_env_parameters = ''
        for name, value in additional_env.items():
            additional_env_parameters += ' -x %s=%s ' % (name, value)

        run_cmd = '%s -n %d %s %s' % (
                os.getenv('MPIEXEC'),
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
        os.system('killall mpiexec')
        #os.system('killall python3')
        os.system('killall %s' % runner_executable)
