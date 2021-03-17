from melissa_da_study import *



import time
import random
import os
import sys
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



if len(sys.argv) == 0:
    clean_old_stats()
    run_melissa_da_study(
            runner_cmd='simulation1',
            total_steps=100000,  # 10e3 are about 5 minuts be longer for sure ;)
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
            additional_server_env={  # necessary for jean-zay
                'LD_LIBRARY_PATH': '/gpfsscratch/rech/moy/rkop006/conda_envs/lib:' +
                os.getenv('LD_LIBRARY_PATH')
                },
            walltime='00:45:00')
else:
    # make a nice plot:

    updates_per_second = {}
    with open(sys.argv[1], 'r') as f:
        for line in f.readlines():
            if 'Finished updat' in line:
                second = int(line.split(" ")[4])
                if not second in updates_per_second:
                    updates_per_second[second] = 0
                updates_per_second[second] += 1

    # for now assume they start in the same...
    import numpy as np
    from matplotlib import pyplot as plt
    a = np.array(list(updates_per_second.items()))
    xs = a.T[0]
    updates = a.T[1]
    # transform xs into minutes:
    xs -= xs.min()
    xs = xs / 60
    print(xs.shape)
    print(updates.shape)
    plt.plot(xs, updates)
    plt.plot(ts, ys)
    plt.show()
    #print(updates_per_second)

