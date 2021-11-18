import traceback
import numpy as np
import mpi4py
mpi4py.rc(initialize=False, finalize=False)
from mpi4py import MPI
import math
import ctypes
from ctypes import *
import os
import time

def calculate_weight(cycle, pid, background, hidden, assimilated_index, assimilated_varid, fcomm):
    try:
        comm = MPI.COMM_WORLD.f2py(fcomm)
        cwlogfile_path = os.environ.get('MELISSA_LORENZ_EXPERIMENT_DIR') + "/calculate_weight_rank-%d.txt" % (comm.rank)
        cwlogfile = open(cwlogfile_path,"w")
        cwlogfile.write("rank %d t=%d, Calculating weight for particle with id=%d" % (comm.rank, cycle, pid))
        cwlogfile.flush()

        assert 'MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE' in os.environ.keys()
        assert 'MELISSA_LORENZ_OBSERVATION_PERCENT' in os.environ.keys()
        assert 'MELISSA_LORENZ_OBSERVATION_DIR' in os.environ.keys()

        share = float(os.environ.get('MELISSA_LORENZ_OBSERVATION_PERCENT'))*0.01
        blk_size = int(os.environ.get('MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE'))
        obs_dir = os.environ.get('MELISSA_LORENZ_OBSERVATION_DIR')

        t0 = time.time()
        background_d = np.frombuffer(background, dtype='float64',
                             count=len(background) // 8)
        t_background_d = time.time() - t0


        t0 = time.time()
        NG = comm.allreduce(len(background_d), MPI.SUM)

        print("STATE DIMENSION: ", NG)
        print("COMM SIZE: ", comm.size)

        NG = comm.allreduce(len(background_d), MPI.SUM)

        print("STATE DIMENSION: ", NG)
        print("COMM SIZE: ", comm.size)

        nl_all = np.full(comm.size, math.floor(NG / comm.size))

        nl_mod = NG%comm.size;
        while nl_mod > 0:
            for i in range(comm.size):
                if (nl_mod > 1):
                    nl_all[i] = nl_all[i] + 1
                    nl_mod = nl_mod - 1
                else:
                    nl_all[i] = nl_all[i] + nl_mod
                    nl_mod = 0
                    break

        nl = nl_all[comm.rank]

        nl_off = 0;
        for i in range(comm.rank):
            nl_off = nl_off + nl_all[i]

        state_min_p = nl_off
        state_max_p = nl_off + nl - 1

        # compute total number of observations
        dim_obs = math.floor(share * NG)
        if dim_obs == 0:
            dim_obs = 1

        # compute number of regions
        num_reg = math.floor(dim_obs / blk_size)
        if dim_obs%blk_size != 0:
            num_reg = num_reg + 1

        # compute stride for regions
        stride = math.floor(NG / num_reg)

        # determine number of obs in pe
        obs_idx = []
        dim_obs_p = 0
        cnt_obs = 0
        for i in range(num_reg):
            offset = i * stride
            for j in range(blk_size):
                index_tmp = offset + j
                if (index_tmp >= state_min_p) and (index_tmp <= state_max_p):
                    dim_obs_p = dim_obs_p + 1
                    obs_idx.append(index_tmp - state_min_p)
                cnt_obs = cnt_obs + 1
                if (cnt_obs == dim_obs): break
                if (index_tmp == state_max_p): break
            if (cnt_obs == dim_obs): break
            if (index_tmp == state_max_p): break

        dim_obs_loc = np.full(1, dim_obs_p, dtype='int32')
        dim_obs_all = np.empty(comm.size, dtype='int32')
        comm.Allgather([dim_obs_loc, MPI.INT], [dim_obs_all, MPI.INT])

        t_indices_d = time.time() - t0

        t0 = time.time()

        amode = MPI.MODE_RDONLY
        fh = MPI.File.Open(comm, obs_dir + "/iter-"+str(cycle)+".obs", amode)
        print("CYCLE", cycle)
        if comm.rank == 0:
            displ = 0
        else:
            displ = ctypes.sizeof(c_double)*sum(dim_obs_all[:comm.rank])
        fh.Set_view(displ)
        observation = np.empty(dim_obs_p, dtype='float64')
        fh.Read(observation)
        fh.Close()

        t_readfile_d = time.time() - t0

        t0 = time.time()

        sum_err = 0
        for i in range(dim_obs_p):
            sum_err = sum_err + (background_d[obs_idx[i]] - observation[i]) ** 2

        #print("errors: ", background_d[obs_idx[:3]] - observation[:3], flush=True)
        #print("background: ", background_d[obs_idx[:3]], flush=True)
        #print("observation: ", observation[:3], flush=True)
        #print("indeces: ", obs_idx[:3], flush=True)
        cwlogfile.write("dim_obs_p: %d" % (dim_obs_p))
        cwlogfile.flush()

        sum_err_all = comm.allreduce(sum_err, MPI.SUM)
        sum_err_all = np.exp(-1*sum_err_all)

        t_compute_d = time.time() - t0

        cwlogfile.write("convert background: %d, compute indices: %d, read file: %d, compute weight: %d" % ( t_background_d, t_indices_d, t_readfile_d, t_compute_d ))
        cwlogfile.flush()

        cwlogfile.close()

        return sum_err_all

    except Exception as e:
        print('Python Error!')
        print(e)
        traceback.print_stack()
        traceback.print_exc()

    return -1.
