import os, sys

import mpi4py
mpi4py.rc(initialize=False, finalize=False)
from mpi4py import MPI


import numpy as np

TOTAL_STEPS = 3
ENSEMBLE_SIZE = 3
PROCS_SERVER = 2

def callback(t, ensemble_list_background, ensemble_list_analysis,
        ensemble_list_hidden_inout):

    assert(ENSEMBLE_SIZE == len(ensemble_list_analysis) == len(ensemble_list_background) == len(ensemble_list_hidden_inout))
    assert t in range(1, TOTAL_STEPS+1)
    for a, b in zip(ensemble_list_background, ensemble_list_analysis):
        assert(a.shape == b.shape)


# FIXME: make use of index statemap... implement functions to access!

    rank = MPI.COMM_WORLD.rank
    assert rank in range(PROCS_SERVER)
    print('my rank:', rank)

    print("in the callback function")
    print("now doing DA update for t=%d..." % t)
    print("lens:", len(ensemble_list_background), len(ensemble_list_analysis))

    ii = ensemble_list_background[0]
    oo = ensemble_list_analysis[1]
    hh = ensemble_list_hidden_inout[2]
    # load observation orresponding to time
    # somehow compare them with ensemble_list_background to generate ensemble_list_analysis
    print(np.array(ii).shape)
    print('input:', ii)
    oo = ii + 1
    print('output:', oo)
    print('refcount:', sys.getrefcount(ii), sys.getrefcount(oo), sys.getrefcount(hh))
    assert(sys.getrefcount(ii) < 5)
    assert(sys.getrefcount(oo) < 5)
    assert(sys.getrefcount(hh) < 5)

    hh = hh - 1
    print('hidden:', ensemble_list_hidden_inout)


    if t == TOTAL_STEPS:
        if rank == 1:
            print('backgound')
            print(ensemble_list_background)
            print('analysis')
            print(ensemble_list_analysis)
            print('hidden')
            print(ensemble_list_hidden_inout)

            from numpy import array
            background = array([array([26., 27., 28., 29., 30., 31., 32., 33., 34., 35., 36., 37., 38.,
                   39., 40., 41., 42., 43., 44., 45.]), array([26., 27., 28., 29., 30., 31., 32., 33., 34., 35., 36., 37., 38.,
                   39., 40., 41., 42., 43., 44., 45.]), array([26., 27., 28., 29., 30., 31., 32., 33., 34., 35., 36., 37., 38.,
                   39., 40., 41., 42., 43., 44., 45.])])
            analysis = array([array([0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                   0., 0., 0.]), array([0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                   0., 0., 0.]), array([0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                   0., 0., 0.])])
            hidden = array([array([0., 3., 3., 0.]), array([0., 3., 3., 0.]), array([0., 3., 3., 0.])])
            # hiddne makes sense: last half of the hidden state is 0330 as first half is
            # 33033, each runner rank has 330 in its hidden state at the end.


            assert (background == array(ensemble_list_background)).all()
            assert (analysis == array(ensemble_list_analysis)).all()
            assert (hidden == array(ensemble_list_hidden_inout)).all()


            # tell test script about success!
            fn = os.getcwd() + '/test-successful'
            print('Writing test-successful file to %s' % fn)
            with open(fn, 'w+') as f:
                f.write('Success\n')

    # return ensemble return nothing, performs inplace changement


if __name__ == '__main__':
    from melissa_da_study import *
    clean_old_stats()
    run_melissa_da_study(
        runner_cmd='simulation1-hidden-index-map',
        total_steps=TOTAL_STEPS,
        ensemble_size=ENSEMBLE_SIZE,
        assimilator_type=ASSIMILATOR_PYTHON,
        # cluster is now auto selected
        procs_server=PROCS_SERVER,
        procs_runner=3,
        n_runners=1,
        show_server_log=False,
        show_simulation_log=False,
        additional_server_env={
            'PYTHONPATH': os.getcwd() + ':' + os.getenv('PYTHONPATH'),
            'MELISSA_DA_PYTHON_ASSIMILATOR_MODULE': 'test_python_assimilator'
            },
        #precommand_server='xterm_gdb',
        server_timeout=10000,
        runner_timeout=10000,
        walltime='00:05:00'
        )

    assert os.path.isfile('STATS/test-successful')
    print('Success!')
