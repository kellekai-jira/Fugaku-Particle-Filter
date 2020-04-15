from melissa_da_study import *

import os
import subprocess
import time

class LocalClusterTracing(LocalCluster):
    def __init__(self):
        self.old_mpiexec = os.getenv('MPIEXEC')

    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env, logfile, is_server):
        # TODO: use annas template engine here instead of this function!
        assert n_nodes == 1  # as we are local


        if not is_server:
            additional_env['SCOREP_ENABLE_PROFILING']='false'

        #if is_server:
        if False:
            splitted = ['scalasca', '-analyze', os.getenv('MPIEXEC'),
                   '-n %d'% n_procs]
            splitted += ['-x %s=%s' % (name, value) for name, value in additional_env.items()]
            splitted += cmd.split()
        else:
            additional_env_parameters = ''
            for name, value in additional_env.items():
                additional_env_parameters += ' -x %s=%s ' % (name, value)
            run_cmd = '%s -n %d %s %s' % (
                    os.getenv('MPIEXEC'),
                    n_procs,
                    additional_env_parameters,
                    cmd)
            splitted = run_cmd.split()

            print("Launching %s" % run_cmd)
        print(splitted)
        pid = 0

        if logfile == '':
            pid = subprocess.Popen(splitted).pid
        else:
            with open(logfile, 'wb') as f:
                pid = subprocess.Popen(splitted, stdout=f).pid


        if is_server:
            self.server_pid = pid
        print("Process id: %d" % pid)
        return pid

    # def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            # additional_env, logfile, is_server):
        # pid = super().ScheduleJob(name, walltime, n_procs, n_nodes, cmd,
            # additional_env, logfile, is_server)
        # if is_server:  # don't kill server job, so the traces are finalized!
            # self.server_pid = pid
        # return pid


    def KillJob(self, job_id):
        time.sleep(4)
        # don't kill server job at the end to write full scorep files...
        #os.system('kill '+str(job_id))
        if not self.server_pid or job_id != self.server_pid:
            os.system('kill '+str(job_id))
        else:
            print("never killing server")
            # give the server some time to shut down itself before the launcher does the
            # exit() that will kill all subprocesses...
            time.sleep(1)


clean_old_stats()

run_melissa_da_study(
        runner_cmd='simulation1-stateful',
        total_steps=3,
        ensemble_size=3,
        assimilator_type=ASSIMILATOR_DUMMY,
        cluster=LocalClusterTracing(),
        #cluster=LocalCluster(),
        procs_server=2,
        procs_runner=3,
        n_runners=1,
        show_server_log = False,
        show_simulation_log = False,
        additional_server_env={
            "SCOREP_ENABLE_PROFILING": "true",  # switch on scorep
            #"SCOREP_TOTAL_MEMORY": "42M",
            "SCOREP_ENABLE_TRACING": "true",  # switch on tracing!
            "SCAN_ANALYZE_OPTS": "--time-correct",

            #"SCOREP_ENABLE_UNWINDING":"true",  # otherwise it will not get user functions

            #"SCOREP_SAMPLING_EVENTS":"perf_cycles@2000000",

            "SCOREP_EXPERIMENT_DIRECTORY": "melissa_server_scorep",

            "SCOREP_FILTERING_FILE": "%s/workspace/melissa-da/profiling/filter_scorep" % os.getenv('HOME'),
            #"SCOREP_TRACING_CONVERT_CALLING_CONTEXT_EVENTS" : "true"  # .. nice enter, leave events, even after unwinding
#
            },
            with_fault_tolerance = False
        )
