import os
import shutil
import configparser
import time
import sys

def prepare_runner_directory(runner_id):
    HMDIR="/2ndfs/ra000012/a04454/NICAM"
    runner_dir = os.path.abspath(os.getcwd())
    LORDIR="/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz"

    os.mkdir(f"{runner_dir}/ll")
    os.mkdir(f"{runner_dir}/msg")
    os.mkdir(f"{runner_dir}/history")
    os.mkdir(f"{runner_dir}/restart")

    os.symlink(f"{HMDIR}/nicam_20211118/NICAM/bin/nhm_driver", f"{runner_dir}/nhm_driver")
    os.symlink(f"{HMDIR}/Public/LEGACY/mnginfo/data/rl01-prc40.info", f"{runner_dir}/rl01-prc40.info")
    os.symlink(f"{HMDIR}/Public/LEGACY/vertical/vlayer/data/vgrid78.dat", f"{runner_dir}/vgrid78.dat")
    os.symlink(f"{HMDIR}/Public/LEGACY/vertical/bsstate_ANL/data/vgrid78_ref_ANL.dat", f"{runner_dir}/vgrid78_ref_ANL.dat")
    os.symlink(f"{HMDIR}/Public/NICAM_DATABASE/radpara/PARA.bnd29ch111sp", f"{runner_dir}/PARA.bnd29ch111sp")
    os.symlink(f"{HMDIR}/Public/NICAM_DATABASE/radpara/invtau.formatted", f"{runner_dir}/invtau.formatted")
    os.symlink(f"{HMDIR}/Public/NICAM_DATABASE/radpara/tautab.formatted", f"{runner_dir}/tautab.formatted")

    for rgn in range(40):
       RG = "{:08d}".format(rgn)
       os.symlink(f"{HMDIR}/Public/NETCDF/boundary/gl05rl01Az78pe40/boundary_GL05RL01Az78.rgn{RG}.nc", f"{runner_dir}/boundary_GL05RL01Az78.rgn{RG}.nc")
       os.symlink(f"{HMDIR}/Public/NETCDF/init/gl05rl01Az78pe40/init_all_GL05RL01Az78.rgn{RG}.nc", f"{runner_dir}/init_all_GL05RL01Az78.rgn{RG}.nc")
       os.symlink(f"{HMDIR}/Public/NETCDF/sst/gl05rl01pe40/oisst_2019_GL05RL01.rgn{RG}.nc", f"{runner_dir}/oisst_2019_GL05RL01.rgn{RG}.nc")

    os.symlink(f"{HMDIR}/nicam_20211118/NICAM/bin/fio_netcdf_ico2ll_mpi", f"{runner_dir}/fio_netcdf_ico2ll_mpi")

    for rgn in range(40):
       RG = "{:05d}".format(rgn)
       os.symlink(f"{HMDIR}/Public/LEGACY/gl05/rl01/grid/llmap/i180j90/data/llmap.rgn{RG}", f"{runner_dir}/llmap.rgn{RG}")

    os.symlink(f"{HMDIR}/Public/LEGACY/gl05/rl01/grid/llmap/i180j90/data/llmap.info", f"{runner_dir}/llmap.info")

os.system('killall gdb')

from melissa_da_study import *
clean_old_stats()

env = ['MELISSA_LORENZ_ITER_MAX',
        'MELISSA_LORENZ_MEMBERS',
        'MELISSA_LORENZ_RUNNER_GROUP_SIZE',
        'MELISSA_LORENZ_PROCS_RUNNERS',
        'MELISSA_LORENZ_NODES_RUNNERS',
        'MELISSA_LORENZ_NUM_RUNNERS',
        'MELISSA_LORENZ_NUM_VALIDATORS',
        'MELISSA_LORENZ_STATE_DIMENSION',
        'MELISSA_LORENZ_OBSERVATION_DIR',
        'MELISSA_LORENZ_EXPERIMENT_DIR',
        'MELISSA_LORENZ_TIMING_TRACE',
        'MELISSA_LORENZ_CPC_CONFIG']

assert all(x in os.environ for x in env)

if int(os.environ['PJM_LLIO_LOCALTMP_SIZE']) > 0:
    local_dir = os.environ['PJM_LOCALTMP']
else:
    local_dir = '/dev/shm'

#if int(os.environ['PJM_LLIO_SHAREDTMP_SIZE']) > 0:
#    global_dir = os.environ['PJM_SHAREDTMP']
#else:
#    global_dir = os.environ['MELISSA_LORENZ_EXPERIMENT_DIR']
global_dir = os.environ['MELISSA_LORENZ_EXPERIMENT_DIR']

print("local directory: " + local_dir)
print("global directory: " + global_dir)

__env_steps = int(os.environ['MELISSA_LORENZ_ITER_MAX'])
__env_members = int(os.environ['MELISSA_LORENZ_MEMBERS'])
__env_runner_group_size = int(os.environ['MELISSA_LORENZ_RUNNER_GROUP_SIZE'])
__env_procs_runners = int(os.environ['MELISSA_LORENZ_PROCS_RUNNERS'])
__env_nodes_runners = int(os.environ['MELISSA_LORENZ_NODES_RUNNERS'])
__env_num_runners = int(os.environ['MELISSA_LORENZ_NUM_RUNNERS'])
__env_num_validators = int(os.environ['MELISSA_LORENZ_NUM_VALIDATORS'])
__env_state_size_elem = os.environ['MELISSA_LORENZ_STATE_DIMENSION']
__env_obs_dir = os.environ['MELISSA_LORENZ_OBSERVATION_DIR']
__env_write_trace = float(os.environ['MELISSA_LORENZ_TIMING_TRACE'])
__env_zip_json_config = os.environ['MELISSA_LORENZ_CPC_CONFIG']

__precommand_runner = ''
if "MELISSA_PRECOMMAND_RUNNER" in os.environ:
    __precommand_runner = os.environ["MELISSA_PRECOMMAND_RUNNER"]

run_melissa_da_study(
    cluster=FugakuCluster(),
    walltime='02:00:00',
    is_p2p=True,
    precommand_server='',
    precommand_runner=__precommand_runner,
    runner_cmd='simulation-lorenz',
    total_steps=__env_steps,
    ensemble_size=__env_members,
    procs_runner=__env_procs_runners,
    nodes_runner=__env_nodes_runners,
    runner_group_size = __env_runner_group_size,
    n_runners=__env_num_runners,
    n_validator=__env_num_validators,
    validator_config=__env_zip_json_config,
    local_ckpt_dir=local_dir + '/melissa_cache',
    global_ckpt_dir=global_dir + '/Global',
    meta_ckpt_dir=global_dir + '/Meta',
#    global_ckpt_dir='/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/STATS/Global',
#    meta_ckpt_dir='/home/ra000012/a04454/LAB/Melissa/melissa-da-particle-filter/examples/lorenz/STATS/Meta',
    show_server_log=False,
    show_simulation_log=False,
    runner_timeout=1000000,  # no timeout for now
    server_timeout=1000000,  # no timeout for now
    prepare_runner_dir=prepare_runner_directory,
    additional_env={
        'PYTHONPATH': os.getenv('MELISSA_DA_SOURCE_PATH') + '/examples/lorenz:' + os.getenv('PYTHONPATH'),
        'MELISSA_DA_PYTHON_CALCULATE_WEIGHT_MODULE': 'calculate_weight',
        'MELISSA_LORENZ_OBSERVATION_BLOCK_SIZE': '1024',
        'MELISSA_LORENZ_ITER_MAX': '10',
        'MELISSA_LORENZ_OBSERVATION_PERCENT': '20',
        'MELISSA_LORENZ_STATE_DIMENSION': __env_state_size_elem,
        'MELISSA_LORENZ_OBSERVATION_DIR': __env_obs_dir,
        'MELISSA_DA_TEST_FIFO': global_dir + '/timing.trace',
        'MELISSA_DA_TIMING_REPORT': str(time.time() + __env_write_trace)
        },

    # for is_p2p=False only:
    additional_server_env={
            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'script_assimilate_python'
            },
    assimilator_type=ASSIMILATOR_PYTHON,
)
