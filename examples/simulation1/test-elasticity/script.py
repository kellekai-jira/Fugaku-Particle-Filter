from melissa_da_study import *


import time
import random
#from matplotlib import pyplot as plt
import numpy as np

random.seed(43)
def free_resources():
    MAX_MINUTES = 20
    MAX_RES = 16
    ts = [0, MAX_MINUTES]
    ys = [2, 0]

    for _ in range(12):
        t = random.randint(0, MAX_MINUTES*2) / 2.
        while t in ts:
            t = random.randint(0, MAX_MINUTES*2) / 2.
        y = random.randint(3, MAX_RES)
        ts.append(t)
        ys.append(y)
    data = sorted(zip(ts,ys))
    ts = np.array(data).T[0]
    ys = np.array(data).T[1]
    #plt.step(ts, ys)
    #plt.show()
    return ts, ys

ts, ys = free_resources()

print(ts, ys)

start_time = time.time()

NODES_SERVER = 2
NODES_RUNNER = 1

def runners_now():
    global ts, ys
    mins = (time.time() - start_time) / 60
    i = 0
    while ts[i] < mins:
        i += 1
    return (ys[i] - NODES_SERVER) // NODES_RUNNER





run_melissa_da_study(
        runner_cmd='simulation1',
        total_steps=10000,
        ensemble_size=30,
        assimilator_type=ASSIMILATOR_DUMMY,
        # not necessary to add cluster. By default an automatic selection for the cluster
        # is done. See the cluster_selector() method.
        cluster=SlurmCluster('igf@cpu'),
        procs_server=2,
        nodes_server=NODES_SERVER,
        procs_runner=3,
        nodes_runner=NODES_RUNNER,
        n_runners=runners_now,
        show_server_log=False,
        show_simulation_log=False,
        walltime='00:45:00')
