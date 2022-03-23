import zmq
import p2p_pb2 as cm

import time
import os

def parse(buf):
    """Parse a buffer into a message object"""
    parsed = cm.Message()
    parsed.ParseFromString(buf)
    return parsed



def bind_socket(context, t, addr):
    socket = context.socket(t)
    socket.bind(addr)
    port = socket.getsockopt(zmq.LAST_ENDPOINT)
    port = port.decode().split(':')[-1]
    port = int(port)
    return socket, port



# time measuring
START_ITERATION                   =  2  # parameter = timestep
STOP_ITERATION                    =  3  # parameter = timestep
START_FILTER_UPDATE               =  4  # parameter = timestep
STOP_FILTER_UPDATE                =  5  # parameter = timestep
START_IDLE_RUNNER                 =  6  # parameter = runner_id
STOP_IDLE_RUNNER                  =  7  # parameter = runner_id
START_PROPAGATE_STATE             =  8  # parameter = state_id
STOP_PROPAGATE_STATE              =  9  # parameter = state_id,

# Server only
START_ACCEPT_WEIGHT               =  100
STOP_ACCEPT_WEIGHT                =  101
START_ACCEPT_DELETE               =  102
STOP_ACCEPT_DELETE                =  103
START_ACCEPT_RUNNER_REQUEST       =  104
STOP_ACCEPT_RUNNER_REQUEST        =  105
START_ACCEPT_PREFETCH             =  106
STOP_ACCEPT_PREFETCH              =  107
START_HANDLE_JOB_REQ              =  108
STOP_HANDLE_JOB_REQ               =  109
START_CALC_PAR_STATE_IMPORTANCE   =  111
STOP_CALC_PAR_STATE_IMPORTANCE    =  110
START_RESAMPLE                    =  112
STOP_RESAMPLE                     =  113

# Validator only
START_LOAD_STATE_VALIDATOR                  =  1000
STOP_LOAD_STATE_VALIDATOR                   =  1001
START_RECV_DICT_VALIDATOR                   =  1002
STOP_RECV_DICT_VALIDATOR                    =  1003
START_SEND_DICT_VALIDATOR                   =  1004
STOP_SEND_DICT_VALIDATOR                    =  1005
START_RECV_EVALUATE_DATAFRAME_VALIDATOR     =  1006
STOP_RECV_EVALUATE_DATAFRAME_VALIDATOR      =  1007
START_ALLREDUCE_DICT_VALIDATOR              =  1008
STOP_ALLREDUCE_DICT_VALIDATOR               =  1009
START_REDUCE_EVALUATE_DATAFRAME_VALIDATOR   =  1010
STOP_REDUCE_EVALUATE_DATAFRAME_VALIDATOR    =  1011
START_COMPUTE_VMAX_VALIDATOR   =  1012
STOP_COMPUTE_VMAX_VALIDATOR    =  1013
START_COMPUTE_VMIN_VALIDATOR   =  1014
STOP_COMPUTE_VMIN_VALIDATOR    =  1015
START_COMPUTE_XAVG_VALIDATOR   =  1016
STOP_COMPUTE_XAVG_VALIDATOR    =  1017
START_COMPUTE_PEARSON_VALIDATOR   =  1018
STOP_COMPUTE_PEARSON_VALIDATOR    =  1019
START_COMPUTE_RMSE_VALIDATOR   =  1020
STOP_COMPUTE_RMSE_VALIDATOR    =  1021
START_COMPUTE_PEMAX_VALIDATOR   =  1022
STOP_COMPUTE_PEMAX_VALIDATOR    =  1023
START_COMPUTE_ENAVG_VALIDATOR   =  1024
STOP_COMPUTE_ENAVG_VALIDATOR    =  1025
START_COMPUTE_ENSTDDEV_VALIDATOR   =  1026
STOP_COMPUTE_ENSTDDEV_VALIDATOR    =  1027
START_COMPUTE_RMSZ_VALIDATOR   =  1028
STOP_COMPUTE_RMSZ_VALIDATOR    =  1029
START_UNPACK_DICT_VALIDATOR                   =  1030
STOP_UNPACK_DICT_VALIDATOR                    =  1031
START_COMPUTE_ENCALC_VALIDATOR   =  1032
STOP_COMPUTE_ENCALC_VALIDATOR    =  1033
START_COMPUTE_ENPAR_VALIDATOR   =  1034
STOP_COMPUTE_ENPAR_VALIDATOR    =  1035
START_FLATTEN_DICT_VALIDATOR = 1036
STOP_FLATTEN_DICT_VALIDATOR = 1037
START_ENSEMBLE_WRAPPER_VALIDATOR = 1038
STOP_ENSEMBLE_WRAPPER_VALIDATOR = 1039
START_VALIDATE_VALIDATOR = 1040
STOP_VALIDATE_VALIDATOR = 1041
START_ZVAL_FULL_VALIDATOR = 1042
STOP_ZVAL_FULL_VALIDATOR = 1043
START_LOAD_STATE_FULL_VALIDATOR = 1044
STOP_LOAD_STATE_FULL_VALIDATOR = 1045
START_ZVAL_BIAS_FULL_VALIDATOR = 1046
STOP_ZVAL_BIAS_FULL_VALIDATOR = 1047

def trigger(what, parameter):
    if trigger.enabled:
        now = time.time() - trigger.null_time
        trigger.events.append((now, what, parameter))

trigger.events = []
# null_time in seconds
trigger.null_time = int(os.getenv("MELISSA_TIMING_NULL")) / 1000
trigger.enabled = True


def maybe_write( is_server = True, validator_id = -1, cycle = -100 ):  # TODO: rename this in maybe_write_timing
    if not trigger.enabled:
        return False



    event_type_translations = [
            (START_ITERATION, STOP_ITERATION, 'Iteration'),
            (START_FILTER_UPDATE, STOP_FILTER_UPDATE, 'Filter Update'),
            (START_PROPAGATE_STATE, STOP_PROPAGATE_STATE, 'Propagation'),
            (START_IDLE_RUNNER, STOP_IDLE_RUNNER, 'Runner Idle'),
            (START_ACCEPT_WEIGHT               , STOP_ACCEPT_WEIGHT                , '_ACCEPT_WEIGHT'),
            (START_ACCEPT_DELETE               , STOP_ACCEPT_DELETE                , '_ACCEPT_DELETE'),
            (START_ACCEPT_RUNNER_REQUEST       , STOP_ACCEPT_RUNNER_REQUEST        , '_ACCEPT_RUNNER_REQUEST'),
            (START_ACCEPT_PREFETCH             , STOP_ACCEPT_PREFETCH              , '_ACCEPT_PREFETCH'),
            (START_HANDLE_JOB_REQ              , STOP_HANDLE_JOB_REQ               , '_HANDLE_JOB_REQ'),
            (START_CALC_PAR_STATE_IMPORTANCE   , STOP_CALC_PAR_STATE_IMPORTANCE    , '_CALC_PAR_STATE_IMPORTANCE'),
            (START_RESAMPLE                    , STOP_RESAMPLE                     , '_RESAMPLE'),
            (START_LOAD_STATE_VALIDATOR, STOP_LOAD_STATE_VALIDATOR, '_LOAD_STATE_VALIDATOR'),
            (START_RECV_DICT_VALIDATOR, STOP_RECV_DICT_VALIDATOR, '_RECV_DICT_VALIDATOR'),
            (START_UNPACK_DICT_VALIDATOR, STOP_UNPACK_DICT_VALIDATOR, '_UNPACK_DICT_VALIDATOR'),
            (START_SEND_DICT_VALIDATOR, STOP_SEND_DICT_VALIDATOR, '_SEND_DICT_VALIDATOR'),
            (START_RECV_EVALUATE_DATAFRAME_VALIDATOR, STOP_RECV_EVALUATE_DATAFRAME_VALIDATOR, '_RECV_EVALUATE_DATAFRAME_VALIDATOR'),
            (START_ALLREDUCE_DICT_VALIDATOR, STOP_ALLREDUCE_DICT_VALIDATOR, '_ALLREDUCE_DICT_VALIDATOR'),
            (START_REDUCE_EVALUATE_DATAFRAME_VALIDATOR, STOP_REDUCE_EVALUATE_DATAFRAME_VALIDATOR, '_REDUCE_EVALUATE_DATAFRAME_VALIDATOR'),
            (START_COMPUTE_VMAX_VALIDATOR, STOP_COMPUTE_VMAX_VALIDATOR, '_COMPUTE_VMAX_VALIDATOR'),
            (START_COMPUTE_VMIN_VALIDATOR, STOP_COMPUTE_VMIN_VALIDATOR, '_COMPUTE_VMIN_VALIDATOR'),
            (START_COMPUTE_XAVG_VALIDATOR, STOP_COMPUTE_XAVG_VALIDATOR, '_COMPUTE_XAVG_VALIDATOR'),
            (START_COMPUTE_PEARSON_VALIDATOR, STOP_COMPUTE_PEARSON_VALIDATOR, '_COMPUTE_PEARSON_VALIDATOR'),
            (START_COMPUTE_RMSE_VALIDATOR, STOP_COMPUTE_RMSE_VALIDATOR, '_COMPUTE_RMSE_VALIDATOR'),
            (START_COMPUTE_PEMAX_VALIDATOR, STOP_COMPUTE_PEMAX_VALIDATOR, '_COMPUTE_PEMAX_VALIDATOR'),
            (START_COMPUTE_ENAVG_VALIDATOR, STOP_COMPUTE_ENAVG_VALIDATOR, '_COMPUTE_ENAVG_VALIDATOR'),
            (START_COMPUTE_ENSTDDEV_VALIDATOR, STOP_COMPUTE_ENSTDDEV_VALIDATOR, '_COMPUTE_ENSTDDEV_VALIDATOR'),
            (START_COMPUTE_RMSZ_VALIDATOR, STOP_COMPUTE_RMSZ_VALIDATOR, '_COMPUTE_RMSZ_VALIDATOR'),
            (START_UNPACK_DICT_VALIDATOR, STOP_UNPACK_DICT_VALIDATOR, '_UNPACK_DICT_VALIDATOR'),
            (START_COMPUTE_ENCALC_VALIDATOR, STOP_COMPUTE_ENCALC_VALIDATOR, '_COMPUTE_ENCALC_VALIDATOR'),
            (START_COMPUTE_ENPAR_VALIDATOR, STOP_COMPUTE_ENPAR_VALIDATOR, '_COMPUTE_ENPAR_VALIDATOR'),
            (START_FLATTEN_DICT_VALIDATOR, STOP_FLATTEN_DICT_VALIDATOR, '_FLATTEN_DICT_VALIDATOR'),
            (START_ENSEMBLE_WRAPPER_VALIDATOR, STOP_ENSEMBLE_WRAPPER_VALIDATOR, '_ENSEMBLE_WRAPPER_VALIDATOR'),
            (START_VALIDATE_VALIDATOR, STOP_VALIDATE_VALIDATOR, '_VALIDATE_VALIDATOR'),
            (START_ZVAL_FULL_VALIDATOR, STOP_ZVAL_FULL_VALIDATOR, '_ZVAL_FULL_VALIDATOR'),
            (START_ZVAL_BIAS_FULL_VALIDATOR, STOP_ZVAL_BIAS_FULL_VALIDATOR, '_ZVAL_BIAS_FULL_VALIDATOR'),
            (START_LOAD_STATE_FULL_VALIDATOR, STOP_LOAD_STATE_FULL_VALIDATOR, '_LOAD_STATE_FULL_VALIDATOR'),

    ]

    # copied from write regions in TimingEvent

    # in seconds
    if (time.time() >= maybe_write.report_time) or (cycle == maybe_write.report_cycle):

        trigger.enabled = False

        trace_fn = 'trace.melissa_p2p_server.csv' if is_server else f'trace.melissa_p2p_validator_{validator_id}.csv'
        print('write region csv for server')
        with open(trace_fn, 'w+') as f:
            f.write("start_time,end_time,region,parameter_open,parameter_close\n")

            open_events = []

            for evt in trigger.events:
                # Opening event: push on stack
                found_anything = False
                for ett in event_type_translations:
                    if evt[1] == ett[0]:
                        #D("Pushing event");
                        open_events.append( evt );
                        found_anything = True
                        break
                    elif evt[1] == ett[1]:
                        for i, oevt in enumerate(reversed(open_events)):
                            # REM: -1 as parameter closes last... otherwise same parameter (important for idle and propagate runner events!)
                            if oevt[1] == ett[0] and (oevt[2] == evt[2] or evt[2] == -1 or ett[0] == START_ITERATION):
                                #D("Popping event and writing region");
                                f.write(','.join(list(map(str, [oevt[0] * 1000, evt[0] * 1000, ett[2], oevt[2], evt[2]]))) + '\n')

                                # remove from stack:
                                del open_events[-i-1]

                                found_anything = True
                                break
                        if found_anything:
                            break
                        else:
                            print("Did not find enter region event for %d %d at %f s" % (evt[1], evt[2], evt[0]))
                    if found_anything:
                        break
                if not found_anything:
                    print("Event %d is no enter/leave region event? Or it is the first weight/ first time the runner connects" % evt[1])
        return True

trigger.enabled = False



if os.getenv("MELISSA_DA_TIMING_REPORT"):
    r = int(float(os.getenv("MELISSA_DA_TIMING_REPORT")))
    if r < time.time():
        print("MELISSA_DA_TIMING_REPORT time was before. No report will be generated")
        trigger.enabled = False  # don't trigger anymore
    else:
        trigger.enabled = True
        maybe_write.report_time = r
        print("Will report timing information at %lu unix seconds (in %lu seconds)"
                        % (maybe_write.report_time, maybe_write.report_time - time.time()))

if os.getenv("MELISSA_LORENZ_ITER_MAX"):
    c = int(float(os.getenv("MELISSA_LORENZ_ITER_MAX"))) - 2
    trigger.enabled = True
    maybe_write.report_cycle = c
    print("Will report timing information at cycle %d" % c )


