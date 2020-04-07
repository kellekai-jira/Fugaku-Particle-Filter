class Cluster:
    """This class is Abstract. Usable Cluster configurations must derive from it."""

    def __init__(self):
        self.procs_per_node = -1


    def ScheduleJob(self, name, walltime, n_procs, n_nodes, cmd,
            melissa_server_master_node='', logfile=''):
        """Puts job into batch scheduler. Must return a job id (integer, > 0) that than is reused in
        CheckJobState, KillJob and so on to identify the job. Often this will just be the
        JobID returned by the Batch Scheduler.

        Arguments:

        name {str}                         name of the job
        walltime {str}                     walltime for the job as the cluster wants it to be formatted. Normally  like hh:mm:ss
        n_procs {int}                      amount of processors over all nodes
        n_nodes {int}                      amount of nodes to run this job on
        cmd {str}                          command to run
        melissa_server_master_node {str}   the content of this must be put in the job's environment variable MELISSA_SERVER_MASTER_NODE
        logfile {str}                      absolute path of where the logfile (mixed stdout AND stderr) should be stored. This is impmortant for some tests.
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
