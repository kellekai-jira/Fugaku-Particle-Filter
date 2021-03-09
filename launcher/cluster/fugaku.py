#!/usr/bin/env python3

# Copyright (c) 2020, Institut National de Recherche en Informatique et en Automatique (https://www.inria.fr/)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import os
import subprocess
import sys
import traceback
import numpy as np
import tempfile
import signal, psutil

from cluster import cluster


class FugakuCluster(cluster.Cluster):
    def __init__(self):
        self.env_variable_pattern = ' -x %s=%s '
        self.mpiexec = os.getenv('MPIEXEC')

        # parameters
        self.NODE_FREE = 0
        self.NODE_SERV = 1
        self.NODE_CLNT = 2

        proc = subprocess.Popen(['pjshowip'], stdout=subprocess.PIPE)
        proc_ips = proc.stdout.read().decode("utf-8").split()
        node_ips = set(proc_ips)
        nb_nodes = len(node_ips)

        self.nodes = [ self.NODE_FREE ] * nb_nodes

        self.jobs = {}


    def __del__(self):
        if self.jobs == {}:
            return

        pid = os.getpid()
        msg = 'pid={:d}: FugakuCluster: jobs {} not cleaned up'
        print(msg.format(pid, self.jobs.keys()), file=sys.stderr)
        print('cleaning them up now...')
        self.CleanUp(None)



    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):

        assert n_procs%n_nodes == 0

        additional_env_parameters = ''
        for key, value in additional_env.items():
            additional_env_parameters += self.env_variable_pattern % (key, value)

        # GET VCOORDS
        available_nodes = np.asarray(np.array(self.nodes) == 0).nonzero()[0]
        assert len(available_nodes) >= n_nodes
        vcoords = available_nodes[:n_nodes]

        # CREATE VCOORD FILE
        vcoordfile = self.CreateVcoordFile( vcoords, int(n_procs/n_nodes) )

        run_cmd = '%s --vcoordfile %s -n %d %s %s' % (
                self.mpiexec,
                vcoordfile,
                n_procs,
                additional_env_parameters,
                cmd)

        print("Launching %s" % run_cmd)

        if logfile == '':
            job = subprocess.Popen(run_cmd.split())
        else:
            with open(logfile, 'wb') as f:
                job = subprocess.Popen(run_cmd.split(), stdout=f)

        self.jobs[job.pid] = { 'job' : job, 'vcoordfile' : vcoordfile, 'vcoords' : vcoords }

        for i in vcoords:
            self.nodes[i] = self.NODE_SERV if is_server else self.NODE_CLNT

        print("Launched {:s} pid={:d}".format(name, job.pid))
        return job.pid


    def CheckJobState(self, job_pid):
        if not job_pid in self.jobs:
            return cluster.STATE_STOP

        job = self.jobs[job_pid]['job']

        if job.poll() is None:
            return cluster.STATE_RUNNING
        else:
            self.RemoveJob(job_pid)
            return cluster.STATE_STOP


    def KillJob(self, job_pid):
        if not job_pid in self.jobs:
            print('no job found with id {:d}'.format(job_pid), file=sys.stderr)
            return
        self.KillRecursive(job_pid)
        self.RemoveJob( job_pid )


    def GetLoad(self):
        """number between 0 and 1"""
        return 0.5

    def CleanUp(self, executable=None):
        pid = os.getpid()
        for job_pid in list(self.jobs):
            if job_pid in self.jobs:
                self.RemoveVcoordFile(self.jobs[job_pid]['vcoordfile'])
                try:
                    job = self.jobs[job_pid]['job']
                    if not job:
                        continue

                    if job.poll() is None:
                        msg = 'pid={:d}: FugakuCluster terminating process {:d}'
                        print(msg.format(pid, job.pid))
                        self.KillRecursive(job_pid)
                except Exception:
                    print("Could not clean up this job. Maybe an uncritical race condition:")
                    traceback.print_exc()

        self.jobs = {}

    @staticmethod
    def clean_up_test():
        """ an extremely rigorous method of cleanup to fix some process kill issues in the
        debian CI"""
        pass

    # HELPER FUNCTIONS

    def CreateVcoordFile(self, vcoords, procs_per_vcoord ):
        tmpname = '/tmp/' + next(tempfile._get_candidate_names()) + '.vcoord'
        with open( tmpname, "w" ) as vcoordfile:
            for coord in vcoords:
                for i in range(procs_per_vcoord):
                    vcoordfile.write("(" + str(coord) + ")\n")
        return tmpname

    def RemoveVcoordFile(self, name ):
        os.remove( name )

    def RemoveJob(self, job_pid ):
        for i in self.jobs[job_pid]['vcoords']:
            self.nodes[i] = self.NODE_FREE
        self.RemoveVcoordFile(self.jobs[job_pid]['vcoordfile'])
        del self.jobs[job_pid]

    def KillRecursive(self, pid):
        # taken from https://stackoverflow.com/a/17112379/5073895
        try:
            parent = psutil.Process(pid)
        except psutil.NoSuchProcess:
            return
        children = parent.children(recursive=True)
        for process in children:
            process.send_signal(signal.SIGKILL)
