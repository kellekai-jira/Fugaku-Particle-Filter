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
STOP_LOAD_STATE_VALIDATOR                   =  1010
START_RECV_DICT_VALIDATOR                   =  1020
STOP_RECV_DICT_VALIDATOR                    =  1030
START_SEND_DICT_VALIDATOR                   =  1040
STOP_SEND_DICT_VALIDATOR                    =  1050
START_RECV_EVALUATE_DATAFRAME_VALIDATOR     =  1060
STOP_RECV_EVALUATE_DATAFRAME_VALIDATOR      =  1070
START_ALLREDUCE_DICT_VALIDATOR              =  1080
STOP_ALLREDUCE_DICT_VALIDATOR               =  1090
START_REDUCE_EVALUATE_DATAFRAME_VALIDATOR   =  1100
STOP_REDUCE_EVALUATE_DATAFRAME_VALIDATOR    =  1110
START_COMPUTE_VMAX_VALIDATOR   =  1120
STOP_COMPUTE_VMAX_VALIDATOR    =  1130
START_COMPUTE_VMIN_VALIDATOR   =  1140
STOP_COMPUTE_VMIN_VALIDATOR    =  1150
START_COMPUTE_XAVG_VALIDATOR   =  1160
STOP_COMPUTE_XAVG_VALIDATOR    =  1170
START_COMPUTE_PEARSON_VALIDATOR   =  1180
STOP_COMPUTE_PEARSON_VALIDATOR    =  1190
START_COMPUTE_RMSE_VALIDATOR   =  1200
STOP_COMPUTE_RMSE_VALIDATOR    =  1210
START_COMPUTE_PEMAX_VALIDATOR   =  1220
STOP_COMPUTE_PEMAX_VALIDATOR    =  1230
START_COMPUTE_ENAVG_VALIDATOR   =  1240
STOP_COMPUTE_ENAVG_VALIDATOR    =  1250
START_COMPUTE_ENSTDDEV_VALIDATOR   =  1260
STOP_COMPUTE_ENSTDDEV_VALIDATOR    =  1270
START_COMPUTE_RMSZ_VALIDATOR   =  1280
STOP_COMPUTE_RMSZ_VALIDATOR    =  1290

def trigger(what, parameter):
    if trigger.enabled:
        now = time.time() - trigger.null_time
        trigger.events.append((now, what, parameter))

trigger.events = []
# null_time in seconds
trigger.null_time = int(os.getenv("MELISSA_TIMING_NULL")) / 1000
trigger.enabled = True


def maybe_write( is_server = True, validator_id = -1 ):  # TODO: rename this in maybe_write_timing
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


    ]

    # copied from write regions in TimingEvent

    # in seconds
    if time.time() >= maybe_write.report_time:

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
                                f.write(','.join(list(map(str, [oevt[0] * 100000, evt[0] * 100000, ett[2], oevt[2], evt[2]]))) + '\n')

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
