#!/usr/bin/python3

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


from cluster import cluster


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
        print(msg.format(pid, self.jobs.keys()), file=sys.stderr)
        print('cleaning them up now...')
        self.CleanUp(None)



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
            return cluster.STATE_STOP

        job = self.jobs[job_id]

        if job.poll() is None:
            return cluster.STATE_RUNNING
        else:
            del self.jobs[job_id]
            return cluster.STATE_STOP


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

    def CleanUp(self, executable):
        pid = os.getpid()
        for job_id in list(self.jobs):
            if job_id in self.jobs:
                try:
                    job = self.jobs[job_id]
                    if not job:
                        continue

                    if job.poll() is None:
                        msg = 'pid={:d}: LocalCluster terminating process {:d}'
                        print(msg.format(pid, job.pid))
                        job.terminate()
                        job.wait()
                except Exception:
                    print("Could not clean up this job. Maybe an uncritical race condition:")
                    traceback.print_exc()

        self.jobs = {}

    @staticmethod
    def clean_up_test():
        """ an extremely rigorous method of cleanup to fix some process kill issues in the
        debian CI"""
        pass
