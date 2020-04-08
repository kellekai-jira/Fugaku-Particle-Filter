from cluster import cluster
import os
import subprocess

import logging

class LocalCluster(cluster.Cluster):

    def __init__(self):
        self.procs_per_node = -1


    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            melissa_server_master_node='', logfile=''):
        # TODO: use annas template engine here instead of this function!
        assert n_nodes == 1  # as we are local
        lib_path = os.getenv('LD_LIBRARY_PATH')

        # handle "". mpiexec -x implicitly adds the new library path to the existing one.
        # "" would lead to :"path1:path2" which cannot be read.
        if lib_path == '':
            lib_path ='""'

        if melissa_server_master_node == '':
            melissa_server_master_node ='""'

        run_cmd = '%s -n %d -x LD_LIBRARY_PATH=%s -x MELISSA_SERVER_MASTER_NODE=%s %s' % (
                os.getenv('MPIEXEC'),
                n_procs,
                lib_path,
                melissa_server_master_node,
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
