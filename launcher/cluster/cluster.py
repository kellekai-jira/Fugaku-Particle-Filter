class Cluster:
    """This class is Abstract. Usable Cluster configurations must derive from it."""

    def __init__(self):
        self.procs_per_node = -1


    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            additional_env={}, logfile='', is_server=False):
        """Puts job into batch scheduler. Must return a job id (integer, > 0) that than is reused in
        CheckJobState, KillJob and so on to identify the job. Often this will just be the
        JobID returned by the Batch Scheduler.

        Arguments:

        name {str}                         name of the job
        walltime {str}                     walltime for the job as the cluster wants it to be formatted. Normally  like hh:mm:ss
        n_procs {int}                      amount of processors over all nodes
        n_nodes {int}                      amount of nodes to run this job on
        cmd {str}                          command to run
        additional_env {dict()}            dictionary with environment variables and their values as they should be set before the start.
        logfile {str}                      absolute path of where the logfile (mixed stdout AND stderr) should be stored. This is impmortant for some tests.
        is_server {bool}                   True if this job is a server job. False for simulation jobs.
        """
        raise NotImplementedError

    def CheckJobState(self, job_id):
        """
        Check the job state:
        0: not runing  TODO: use macros!  TODO: what's the difference between not running and not running anymore?
        1: running
        2: not running anymore (finished or crashed)
        """
        raise NotImplementedError

    def KillJob(self, job_id):
        raise NotImplementedError

    def GetLoad(self):
        """ return number between 0 and 1. 0: cluster empty. 1: cluster full"""
        raise NotImplementedError

    def CleanUp(self, runner_executable):
        """
        must clean up the environment. Is called before a new study is started to clean
        old processes that might be listening for some ports and so on

        Arguments:

        runner_executable {str}              name of the runner executable as it might be passed to the killall command
        """
        raise NotImplementedError
