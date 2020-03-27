###################################################################
#                            Melissa                              #
#-----------------------------------------------------------------#
#   COPYRIGHT (C) 2017  by INRIA and EDF. ALL RIGHTS RESERVED.    #
#                                                                 #
# This source is covered by the BSD 3-Clause License.             #
# Refer to the  LICENCE file for further information.             #
#                                                                 #
#-----------------------------------------------------------------#
#  Original Contributors:                                         #
#    Theophile Terraz,                                            #
#    Bruno Raffin,                                                #
#    Alejandro Ribes,                                             #
#    Bertrand Iooss,                                              #
###################################################################


"""
    User defined options module
"""

import getpass
from launcher import melissa
from functions import *

melissa_study = melissa.Study()
melissa_study.set_working_directory(WORKDIR)

melissa_study.set_sampling_size(20)            # ensemble size for assimilation
melissa_study.set_nb_timesteps(100)            # number of timesteps, from Melissa point of view

melissa_study.set_simulation_timeout(400)      # simulations are restarted if no life sign for 400 seconds
melissa_study.set_checkpoint_interval(300)     # server checkpoints every 300 seconds
melissa_study.set_verbosity(3)                 # verbosity: 0: only errors, 1: errors + warnings, 2: usefull infos (default), 3: debug info
melissa_study.set_batch_size(2)
melissa_study.set_assimilation(True)

# some secret options...:
melissa_study.set_option('assimilation_total_steps', 10)
melissa_study.set_option('assimilation_ensemble_size', 5)
melissa_study.set_option('assimilation_assimilator_type', 0)  # ASSIMILATOR_DUMMY
melissa_study.set_option('assimilation_max_runner_timeout', 5)  # seconds, timeout checked frin tge server sude,

melissa_study.set_option('server_cores', 1)  # overall cores for the server
melissa_study.set_option('server_nodes', 1)  # using that many nodes  ... on  a well defined cluster the other can be guessed probably.

melissa_study.set_option('simulation_cores', 1)  # cores of one runner
melissa_study.set_option('simulation_nodes', 1)  # using that many nodes

melissa_study.simulation.launch(launch_runner)
melissa_study.server.launch(launch_server)
melissa_study.check_job(check_job)
melissa_study.simulation.check_job(check_job)
melissa_study.server.restart(launch_server)
melissa_study.check_scheduler_load(check_load)
melissa_study.cancel_job(kill_job)

print('working_directory:   ' + str(melissa_study.get_working_directory()))
print('ensemble_size:       ' + str(melissa_study.get_sampling_size()))
print('nb_timesteps:        ' + str(melissa_study.get_nb_timesteps()))
print('simulation_timeout:  ' + str(melissa_study.get_simulation_timeout()))
print('checkpoint_interval: ' + str(melissa_study.get_checkpoint_interval()))
print('verbosity:           ' + str(melissa_study.get_verbosity()))

melissa_study.run()
