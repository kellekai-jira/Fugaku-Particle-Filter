import struct
import time
from multiprocessing import Pool
import numpy as np
import configparser
import p2p_pb2 as cm
import io
import fpzip
from functools import partial
import array
import json
import os
import zmq
import pandas as pd
import glob
import re
import netCDF4
from common import *
import sys
from functools import reduce as py_reduce
import pandas_zmq
import gc
import zfpy

'''
(from Baker et al, 2014)

* CHARACTERIZING THE ORIGINAL DATA

CR(F) compressed size/original size                                                     <- rate
e_max   = max( |compressed_i - original_i| )                                            <- pointwise maximum error 
RMSE    = sqrt( 1/N * sum( sq(e_i) ) ), e_i = compressed_i - original_i                 <- root mean square error
PSNR    = 20 log10( max( original_i ) / RMSE )                                          <- peak signal to noise error
rho     = ( sum( (orig_i - mean(orig)) * (compr_i - mean(compr)) ) ) \  # => cov(orig,compr)/(s_orig*s_compr) !> 0.99999
        / sqrt( sum( sq(orig_i - mean(orig)) ) * sum( sq(compr_i - mean(compr)) ) )     <- pearson correlation coefficient
z-score (see Baker et al, 2014)
'''


FTI_CPC_MODE_NONE   = 0
FTI_CPC_FPZIP       = 1
FTI_CPC_ZFP         = 2
FTI_CPC_SINGLE      = 3
FTI_CPC_HALF        = 4
FTI_CPC_STRIP       = 5
FTI_CPC_TYPE_NONE   = 0
FTI_CPC_ACCURACY    = 1
FTI_CPC_PRECISION   = 2

from utils import get_node_name
from common import bind_socket, parse

assert (os.environ.get('MELISSA_DA_WORKER_ID') is not None)
validator_id = int(os.getenv('MELISSA_DA_WORKER_ID'))

print(f"My ID is: {validator_id}")

state_buffer = {}

# connect sockets
context = zmq.Context()
context.setsockopt(zmq.LINGER, 0)

# server socket
addr = "tcp://*:4000"
server_socket, port_socket = \
    bind_socket(context, zmq.REQ, addr)

validator_socket = None

# validator socket
def connect_validator_sockets():
    global validator_socket

    # validator master
    if validator_id == 0:
        if validator_socket is None:
            validator_socket = {}
        pattern = os.getcwd() + '/worker-*-ip.dat'
        worker_ip_files = glob.glob(pattern)
        p = re.compile("worker-(.*)-ip.dat")
        for fn in worker_ip_files:
            id = int(p.search(os.path.basename(fn)).group(1))
            if id != 0 and id not in validator_socket:
                with open(fn, 'r') as file:
                    ip = file.read().rstrip()
                addr = "tcp://" + ip + ":4001"
                so = context.socket(zmq.REQ)
                so.connect(addr)
                validator_socket[id] = so
                print(f"Validator master connected to validator: {id} at ip: {addr}")

    # validator slaves
    else:
        if validator_socket is None:
            addr = "tcp://*:4001"
            validator_socket, port_socket = \
                bind_socket(context, zmq.REP, addr)

# set paths
experimentPath = os.getcwd() + '/'
checkpointPath = os.path.dirname(os.getcwd()) + '/Global/'

def get_parameter_info( cpc ):
    if  cpc.mode == FTI_CPC_MODE_NONE:
        return 'uncompressed'

    if cpc.mode == FTI_CPC_SINGLE:
        return f"method: single-precision (32 bit float)"

    if cpc.mode == FTI_CPC_HALF:
        return f"method: half-precision (16 bit float)"

    elif cpc.mode == FTI_CPC_FPZIP:
        return f"method: FPZIP, precision: {cpc.parameter}"

    elif cpc.mode == FTI_CPC_ZFP:
        if cpc.type == FTI_CPC_PRECISION:
            return f"method: ZFP, type: PRECISION, precision: {cpc.parameter}"
        if cpc.type == FTI_CPC_ACCURACY:
            return f"method: ZFP, type: ACCURACY, tolerance: 1e-{cpc.parameter}"
        else:
            return f"method:ZFP <unknown compression type>"

    else:
        return '<no info available>'


def get_proc_data_ckpt(proc, sid, name, meta):

    item = meta[sid][proc][name]
    ckpt_file = item['ckpt_file']
    ckpt = open(ckpt_file, 'rb')

    if item['mode'] == FTI_CPC_MODE_NONE:
        ckpt.seek(item['offset'])
        out = np.fromfile(ckpt, dtype=np.float64, count=item['count'])

    if item['mode'] == FTI_CPC_SINGLE:
        ckpt.seek(item['offset'])
        out = np.fromfile(ckpt, dtype=np.float32, count=item['count']).astype('float64')

    if item['mode'] == FTI_CPC_HALF:
        ckpt.seek(item['offset'])
        out = np.fromfile(ckpt, dtype=np.float16, count=item['count']).astype('float64')

    elif item['mode'] == FTI_CPC_FPZIP:
        data = []
        size = item['size']
        ckpt.seek(item['offset'])
        load = 0
        while load < size:
            bytes = ckpt.read(8)
            load += len(bytes)
            bs = int.from_bytes(bytes, byteorder='little')
            bytes = ckpt.read(bs)
            load += len(bytes)
            block = fpzip.decompress(bytes, order='C')[0, 0, 0]
            data = [*data, *block]
        out = np.array(data)

    elif item['mode'] == FTI_CPC_ZFP:
        count = item['count']
        size = item['size']
        ckpt.seek(item['offset'])
        byte_data = ckpt.read(size)
        if item['type'] == FTI_CPC_ACCURACY:
            if item['parameter'] == 0:
                tolerance = 0
            else:
                tolerance = 10 ** (-1 * item['parameter'])
            precision = -1
        elif item['type'] == FTI_CPC_PRECISION:
            tolerance = -1
            precision = item['parameter']
        else:
            print("ERROR - unknown compression type")
            exit(-1)
        out = zfpy._decompress(byte_data, zfpy.type_double, (count,), tolerance=tolerance, precision=precision)

    ckpt.close()

    return out



def load_ckpt_data(meta, sid, nranks, name):

    global state_buffer

    assert(sid not in state_buffer)

    state_buffer[sid] = {}

    trigger(START_LOAD_STATE_VALIDATOR, 0)
    for proc in range(nranks):
        state_buffer[sid][proc] = get_proc_data_ckpt(proc, sid, name, meta)
    trigger(STOP_LOAD_STATE_VALIDATOR, 0)

    #with Pool() as pool:
    #    res = pool.map(partial(get_proc_data_ckpt, sid=sid, name=name, meta=meta), range(nranks))

    #for proc, data in enumerate(res):
    #    state_buffer[sid][proc] = data

def write_lorenz(average, stddev, cycle, num_procs, state_dims):
    ncfiles = {}
    for rank in range(num_procs):
        fn = experimentPath + f"lorenz_analysis_c{cycle}p{rank}.nc"
        ncfiles[rank] = netCDF4.Dataset(fn, mode='w', format='NETCDF4')
        ncfiles[rank].createDimension('x', state_dims[rank])
        ncfiles[rank].title = 'lorenz best state estimate'
        x_dim = ncfiles[rank].createVariable('x', np.int64, ('x',))
        x_dim.units = 'cm'
        x_dim.long_name = 'position x in cm'
        x_dim[:] = range(state_dims[rank])
    for name in average:
        for rank in average[name]:
            avg_var = ncfiles[rank].createVariable( name, np.float64, ('x',))
            avg_var.units = 'm/s'
            avg_var.standard_name = 'lorenz velocity'
            avg_var[:] = average[name][rank]
            stddev_var = ncfiles[rank].createVariable( name + " stddev", np.float64, ('x',))
            stddev_var.units = 'm/s'
            stddev_var.standard_name = 'lorenz velocity standard deviation'
            stddev_var[:] = stddev[name][rank]
    for name in average:
        for rank in average[name]:
            ncfiles[rank].close()


def send_message(socket, data):
    msg = data.SerializeToString()
    socket.send(msg)


def encode_state_id( t, id, mode ):
    hash        = mode
    hash        = (hash << 24)  | t
    hash        = (hash << 32) | id
    return hash


def decode_state_id( hash ):
    mask_mode   = 0xFF
    mask_t      = 0xFFFFFF
    mask_id     = 0xFFFFFFFF
    id          = hash & mask_id
    t           = (hash >> 32) & mask_t
    mode        = (hash >> 56) & mask_mode
    return t, id, mode


def energy(data, proc, name):
    return sum(0.5*data**2)

def reduce_energy(parts, n):
    return sum(parts)/n


def reduce_sum(parts, n):
    return sum(parts)


def zval(data, proc, name):
    """
        computes the sum of squared z values for
        variable 'name' and rank 'proc'
    """
    xsigma = data - average[name][proc]
    msigma = stddev[name][proc]

    ssz = sum(np.divide(xsigma, msigma, out=np.zeros_like(xsigma), where=msigma != 0) ** 2)

    return ssz


def rho_nominator(data, proc, name):
    """
        computes the sum of squared differences between two states
        data[0] <- first state
        data[1] <- second state
    """
    d1 = data[0]
    d2 = data[1]

    avg1 = np.array(average_x[0])
    avg2 = np.array(average_x[1])

    assert(len(d1) == len(d2))

    return sum((d1-avg1)*(d2-avg2))


def rho_denumerator_left(data, proc, name):
    """
        computes the sum of squared differences between two states
        data[0] <- first state
        data[1] <- second state
    """
    avg = np.array(average_x[0])

    return sum((data-avg)**2)


def rho_denumerator_right(data, proc, name):
    """
        computes the sum of squared differences between two states
        data[0] <- first state
        data[1] <- second state
    """
    avg = np.array(average_x[1])

    return sum((data-avg)**2)


def sse(data, proc, name):
    """
        computes the sum of squared differences between two states
        data[0] <- first state
        data[1] <- second state
    """
    d1 = data[0]
    d2 = data[1]

    assert(len(d1) == len(d2))

    return sum((d1-d2)**2)


def avg_x(data, proc, name):
    return sum(data)


def reduce_avg_x(parts, n):
    return sum(parts)/n


def reduce_sse(parts, n):
    return np.sqrt(sum(parts) / n)


def maximum(data, proc, name):
    """
        computes the maximum value
        data[0] <- first state
        data[1] <- second state
    """
    return np.max(data)


def minimum(data, proc, name):
    """
        computes the maximum value
        data[0] <- first state
        data[1] <- second state
    """
    return np.min(data)


def maximum_abs(data, proc, name):
    """
        computes the maximum value
        data[0] <- first state
        data[1] <- second state
    """
    return np.max(np.abs(data))


def minimum_abs(data, proc, name):
    """
        computes the maximum value
        data[0] <- first state
        data[1] <- second state
    """
    return np.min(np.abs(data))


def reduce_maximum(maxima, n):
    return np.max(maxima)


def reduce_minimum(minima, n):
    return np.min(minima)


def pme(data, proc, name):
    """
        computes the pointwise maximum error between 2 states
        data[0] <- first state
        data[1] <- second state
    """
    a1 = data[0]
    a2 = data[1]

    assert(len(a1) == len(a2))

    return max(np.abs(a1-a2))


def reduce_pme(max_errors, n):
    return max(max_errors)


def compare(proc, sids, name, meta, func):

    global state_buffer

    states = []

    for sid in sids:

        if sid not in state_buffer:

            state_buffer[sid] = {}

        if proc not in state_buffer[sid]:

            print(f"loading state id:{sid}|rank:{proc} from file system")

            trigger(START_LOAD_STATE_VALIDATOR, 0)

            item = meta[sid][proc][name]
            ckpt_file = item['ckpt_file']
            ckpt = open(ckpt_file, 'rb')

            if item['mode'] == 0:
                ckpt.seek(item['offset'])
                bytes = ckpt.read(item['size'])
                state_buffer[sid][proc] = array.array('d', bytes)

            else:
                data = []
                n = item['count']
                bs = 1024 * 1024
                nb = n // bs + (1 if n % bs != 0 else 0)

                ckpt.seek(item['offset'])

                for b in range(nb):
                    bytes = ckpt.read(8)
                    bs = int.from_bytes(bytes, byteorder='little')
                    bytes = ckpt.read(bs)
                    block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                    data = [*data, *block]

                state_buffer[sid][proc] = data

            ckpt.close()

            trigger(STOP_LOAD_STATE_VALIDATOR, 0)

        states.append(state_buffer[sid][proc])

    return func(states, proc, name)


def compare_wrapper( variables, sids, ndim, nprocs, meta, func, reduce_func, operation, cpc ):
    pool = Pool()

    dfl = []
    for name in variables:
        original = decode_state_id( sids[0] )
        compared = decode_state_id( sids[1] )
        data_size = 0
        size_compared = 0
        for proc in range(nprocs):
            data_size += float(meta[sids[0]][proc][name]['count'] * 8)
            size_compared += float(meta[sids[1]][proc][name]['size'])
        rate_compared = data_size / size_compared
        results = pool.map(partial(compare, sids=sids, name=name, meta=meta, func=func), range(nprocs))
        reduced = reduce_func(results, ndim)
        dfl.append( {
            'variable' : name,
            'operation' : operation,
            'value' : reduced,
            'mode' : cpc[compared[2]].mode,
            'parameter': cpc[compared[2]].parameter,
            't' : original[0],
            'id' : original[1],
            'rate' : rate_compared,
            'cid' : compared[2],
            'sid': sids[1]
        } )

    return pd.DataFrame(dfl)


def evaluate(proc, sid, name, meta, func):

    global state_buffer

    item = meta[sid][proc][name]
    ckpt_file = item['ckpt_file']
    ckpt = open(ckpt_file, 'rb')

    if sid not in state_buffer:

        state_buffer[sid] = {}

    if proc not in state_buffer[sid]:

        print(f"loading state id:{sid}|rank:{proc} from file system")

        trigger(START_LOAD_STATE_VALIDATOR, 0)

        if item['mode'] == 0:
            ckpt.seek(item['offset'])
            bytes = ckpt.read(item['size'])

            state_buffer[sid][proc] = np.array(array.array('d', bytes))

        else:
            data = []
            n = item['count']
            bs = 1024 * 1024
            nb = n // bs + (1 if n % bs != 0 else 0)

            ckpt.seek(item['offset'])

            for b in range(nb):
                bytes = ckpt.read(8)
                bs = int.from_bytes(bytes, byteorder='little')
                bytes = ckpt.read(bs)
                block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                data = [*data, *block]

            state_buffer[sid][proc] = np.array(data)

        ckpt.close()

        trigger(STOP_LOAD_STATE_VALIDATOR, 0)

    return func(state_buffer[sid][proc], proc, name)

def evaluate_wrapper( variables, sid, ndim, nprocs, meta, func, reduce_func, operation, cpc ):
    pool = Pool()

    dfl = []
    for name in variables:
        t, id, p = decode_state_id( sid )
        data_size = 0
        size_original = 0
        for proc in range(nprocs):
            data_size += float(meta[sid][proc][name]['count'] * 8)
            size_original += float(meta[sid][proc][name]['size'])
        rate_original = data_size / size_original
        results = pool.map(partial(evaluate, sid=sid, name=name, meta=meta, func=func), range(nprocs))
        reduced = reduce_func(results, ndim)
        dfl.append( {
            'variable' : name,
            'operation' : operation,
            'value' : reduced,
            'mode' : int(cpc[p].mode),
            'parameter': int(cpc[p].parameter),
            't' : int(t),
            'id' : int(id),
            'rate' : rate_original,
            'cid' : p,
            'sid' : sid
        } )

    return pd.DataFrame(dfl)


def create_dataframe_row( name, operation, value, cpc, t, id, rate, sid ):
    return pd.DataFrame([{
        'variable': name,
        'operation': operation,
        'value': value,
        'mode': cpc.mode,
        'parameter': cpc.parameter,
        't': t,
        'id': id,
        'rate': rate,
        'cid': cpc.id,
        'sid': sid
    }])


def ensemble_wrapper( variables, weights, nprocs, meta, func, cpc_ids ):

    res = {}
    for name in variables:
        with Pool() as pool:
            res[name] = pool.map(partial(func, weights=weights, cpc_ids=cpc_ids, name=name, meta=meta), range(nprocs))

    return res


def ensemble_mean(proc, weights, cpc_ids, name, meta):

    global state_buffer

    stemp = encode_state_id(weights[0].state_id.t, weights[0].state_id.id, 0)
    ndim = meta[stemp][proc][name]['count']
    ssum = np.zeros(ndim)

    for idx,weight in enumerate(weights):

        sid = encode_state_id(weight.state_id.t, weight.state_id.id, cpc_ids[idx])

        item = meta[sid][proc][name]
        ckpt_file = item['ckpt_file']

        if sid not in state_buffer:

            state_buffer[sid] = {}

        if proc not in state_buffer[sid]:

            ckpt = open(ckpt_file, 'rb')

            print(f"loading state id:{sid}|rank:{proc} from file system")

            trigger(START_LOAD_STATE_VALIDATOR, 0)

            if item['mode'] == 0:
                ckpt.seek(item['offset'])
                bytes = ckpt.read(item['size'])

                state_buffer[sid][proc] = np.array(array.array('d', bytes))

            else:
                data = []
                n = item['count']
                bs = 1024 * 1024
                nb = n // bs + (1 if n % bs != 0 else 0)

                ckpt.seek(item['offset'])

                for b in range(nb):
                    bytes = ckpt.read(8)
                    bs = int.from_bytes(bytes, byteorder='little')
                    bytes = ckpt.read(bs)
                    block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                    data = [*data, *block]

                state_buffer[sid][proc] = np.array(data)

            ckpt.close()

            trigger(STOP_LOAD_STATE_VALIDATOR, 0)

        ssum += weight.weight * state_buffer[sid][proc]

    return ssum


def ensemble_stddev(proc, weights, cpc_ids, name, meta):

    global state_buffer

    stemp = encode_state_id(weights[0].state_id.t, weights[0].state_id.id, 0)
    ndim = meta[stemp][proc][name]['count']
    ssum = np.zeros(ndim)

    for idx,weight in enumerate(weights):

        sid = encode_state_id(weight.state_id.t, weight.state_id.id, cpc_ids[idx])

        item = meta[sid][proc][name]
        ckpt_file = item['ckpt_file']

        if sid not in state_buffer:

            state_buffer[sid] = {}

        if proc not in state_buffer[sid]:

            ckpt = open(ckpt_file, 'rb')

            print(f"loading state id:{sid}|rank:{proc} from file system")

            trigger(START_LOAD_STATE_VALIDATOR, 0)

            if item['mode'] == 0:
                ckpt.seek(item['offset'])
                bytes = ckpt.read(item['size'])

                state_buffer[sid][proc] = np.array(array.array('d', bytes))

            else:
                data = []
                n = item['count']
                bs = 1024 * 1024
                nb = n // bs + (1 if n % bs != 0 else 0)

                ckpt.seek(item['offset'])

                for b in range(nb):
                    bytes = ckpt.read(8)
                    bs = int.from_bytes(bytes, byteorder='little')
                    bytes = ckpt.read(bs)
                    block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                    data = [*data, *block]

                state_buffer[sid][proc] = np.array(data)

            ckpt.close()

            trigger(STOP_LOAD_STATE_VALIDATOR, 0)

        ssum += weight.weight * (state_buffer[sid][proc] - average[name][proc])**2

    return ssum


def ping( socket ):
    msg = cm.Message()
    send_message( socket, msg )


def pong( socket ):
    socket.recv()


def reduce_evaluate_df( validators, df ):
    """
    reduce dictionary from slave to master validators
    ping and pong ensure the alternating send/recv and
    recv/send pattern vor master and slaves
    """
    global validator_socket
    trigger(START_REDUCE_EVALUATE_DATAFRAME_VALIDATOR, 0)
    if validator_id == 0:
        for id in validators:
            ping(validator_socket[id])
            df_validator = pandas_zmq.recv_dataframe(validator_socket[id])
            df = df.append(df_validator, ignore_index=True)
    else:
        pong(validator_socket)
        pandas_zmq.send_dataframe(validator_socket, df)
    trigger(STOP_REDUCE_EVALUATE_DATAFRAME_VALIDATOR, 0)

    return df


def send_weights( socket, weights ):
    wrapper = cm.StatisticWeights()
    wrapper.weights.extend( weights )
    send_message( socket, wrapper )


def receive_weights(socket):
    msg = socket.recv()  # only polling
    wrapper = cm.StatisticWeights()
    wrapper.ParseFromString(msg)
    return wrapper.weights


def allreduce_weights( validators, weights ):
    """
    reduce double from slave to master validators
    ping and pong ensure the alternating send/recv and
    recv/send pattern vor master and slaves
    """
    global validator_socket

    if validator_id == 0:
        for id in validators:
            ping(validator_socket[id])
            weights.extend( receive_weights(validator_socket[id]) )
        for id in validators:
            send_weights(validator_socket[id], weights)
        for id in validators:
            pong(validator_socket[id])


    else:
        pong(validator_socket)
        send_weights(validator_socket, weights)
        weights = receive_weights(validator_socket)
        ping(validator_socket)

    return weights


def validate(meta, compare_function, compare_reduction, evaluate_function,
             evaluate_reduction, ndims, nprocs, variables, cpc, state_ids, weights, validators):

    global average, stddev, average_x
    print('[ Compute single state statistics ]')
    df_evaluate = pd.DataFrame()
    for state_id in state_ids:
        print('─' * 100)
        print(f'|>  t: {state_id.t}, id: {state_id.id}')
        print('─' * 100)
        average_x = []
        original = encode_state_id(state_id.t, state_id.id, 0)
        print(f'|   -> computing max value')
        trigger(START_COMPUTE_VMAX_VALIDATOR, 0)
        df_vmax = evaluate_wrapper(variables, original, ndims, nprocs, meta, maximum, reduce_maximum, 'maximum', cpc)
        trigger(STOP_COMPUTE_VMAX_VALIDATOR, 0)
        xmax = df_vmax['value'].iloc[-1]
        print('| ')
        print(f"|       x_max: {xmax}")
        print('| ')
        print(f'|   -> computing absolute max value')
        trigger(START_COMPUTE_VMAX_VALIDATOR, 0)
        df_vmaxabs = evaluate_wrapper(variables, original, ndims, nprocs, meta, maximum_abs, reduce_maximum, 'maximum_abs', cpc)
        trigger(STOP_COMPUTE_VMAX_VALIDATOR, 0)
        xmaxabs = df_vmaxabs['value'].iloc[-1]
        print('| ')
        print(f"|       x_max_abs: {xmaxabs}")
        print('| ')
        print(f'|   -> computing min value')
        trigger(START_COMPUTE_VMIN_VALIDATOR, 0)
        df_vmin = evaluate_wrapper(variables, original, ndims, nprocs, meta, minimum, reduce_minimum, 'minimum', cpc)
        trigger(STOP_COMPUTE_VMIN_VALIDATOR, 0)
        xmin = df_vmin['value'].iloc[-1]
        print('| ')
        print(f"|       x_min: {xmin}")
        print('| ')
        print(f'|   -> computing avg value')
        trigger(START_COMPUTE_XAVG_VALIDATOR, 0)
        df_avg = evaluate_wrapper(variables, original, ndims, nprocs, meta, avg_x, reduce_avg_x, 'average', cpc)
        trigger(STOP_COMPUTE_XAVG_VALIDATOR, 0)
        print('| ')
        print(f"|       x_avg: {df_avg['value'].iloc[-1]}")
        print('| ')
        average_x.append(df_avg['value'][0])
        print(f'|   -> computing range')
        df_range = create_dataframe_row('state1', 'range', xmax - xmin, cpc[0], state_id.t, state_id.id, 1.0, original)
        range = df_range['value'].iloc[-1]
        print('| ')
        print(f"|       range: {range}")
        print('| ')
        df_evaluate = df_evaluate.append( pd.concat( [df_avg, df_vmax, df_vmin, df_range], ignore_index=True ), ignore_index=True )
        for p in cpc[1:]:
            print('─' * 100)
            print(f'|>  parameter-id: {p.id} ' + get_parameter_info(p))
            print('─' * 100)
            compared = encode_state_id( state_id.t, state_id.id, p.id )
            print(f'|   -> computing avg value of compressed state')
            trigger(START_COMPUTE_XAVG_VALIDATOR, 0)
            df_avg_compared = evaluate_wrapper(variables, compared, ndims, nprocs, meta, avg_x, reduce_avg_x, 'average', cpc)
            trigger(STOP_COMPUTE_XAVG_VALIDATOR, 0)
            print('| ')
            print(f"|       x_avg: {df_avg_compared['value'].iloc[-1]}")
            print('| ')
            cpc_rate = df_avg_compared['rate'].iloc[-1]
            average_x.append(df_avg_compared['value'][0])
            print(f'|   -> computing pearson correlation coefficient')
            trigger(START_COMPUTE_PEARSON_VALIDATOR, 0)
            df_rho_nominator = compare_wrapper( variables, [original, compared], ndims, nprocs, meta, rho_nominator, reduce_sum, 'rho_nominator', cpc)
            df_rho_denumerator_left = evaluate_wrapper(variables, original, ndims, nprocs, meta, rho_denumerator_left, reduce_sum, 'df_rho_denumerator_left', cpc)
            df_rho_denumerator_right = evaluate_wrapper(variables, compared, ndims, nprocs, meta, rho_denumerator_right, reduce_sum, 'df_rho_denumerator_right', cpc)
            # TODO write function and iterate over variable names to assign rho
            rho = df_rho_nominator['value'][0] / np.sqrt( df_rho_denumerator_left['value'][0] * df_rho_denumerator_right['value'][0])
            trigger(STOP_COMPUTE_PEARSON_VALIDATOR, 0)
            print('| ')
            print(f"|       rho: {rho}")
            print('| ')
            df_rho_denumerator_right.at[0, 'value'] = rho
            df_rho_denumerator_right.at[0, 'operation'] = 'rho'
            df_rho = df_rho_denumerator_right
            print(f'|   -> computing RMSE of compressed state')
            trigger(START_COMPUTE_RMSE_VALIDATOR, 0)
            df_rmse = compare_wrapper( variables, [original, compared], ndims, nprocs, meta, sse, reduce_sse, 'RMSE', cpc)
            rmse = df_rmse['value'].iloc[-1]
            nrmse = rmse/range
            df_nrmse = create_dataframe_row('state1', 'nrmse', nrmse, p, state_id.t, state_id.id, cpc_rate, compared)
            trigger(STOP_COMPUTE_RMSE_VALIDATOR, 0)
            print('| ')
            print(f"|       RMSE: {rmse}")
            print(f"|       RMSE (normalized): {nrmse}")
            print('| ')
            print(f'|   -> computing pointwise maximum error of compressed')
            trigger(START_COMPUTE_PEMAX_VALIDATOR, 0)
            df_emax = compare_wrapper( variables, [original, compared], ndims, nprocs, meta, pme, reduce_pme, 'PE_max', cpc)
            emax = df_emax['value'].iloc[-1]
            nemax = emax/range
            df_nemax = create_dataframe_row('state1', 'nemax', nemax, p, state_id.t, state_id.id, cpc_rate, compared)
            trigger(STOP_COMPUTE_PEMAX_VALIDATOR, 0)
            print('| ')
            print(f"|       PE_max: {emax}")
            print(f"|       PE_max (normalized): {nemax}")
            print('| ')
            print(f'|   -> computing peak signal to noise ratio')
            psnr = 20 * np.log10( xmax / rmse )
            df_psnr = create_dataframe_row('state1', 'psnr', psnr, p, state_id.t, state_id.id, cpc_rate, compared)
            print('| ')
            print(f"|       PSNR: {psnr}")
            print('| ')
            df_evaluate = df_evaluate.append( pd.concat([df_avg_compared, df_rho, df_rmse, df_nrmse, df_emax, df_nemax, df_psnr] , ignore_index=True ), ignore_index=True )

    weights_temp = weights.copy()
    global_weights = allreduce_weights( validators, weights )
    weights = weights_temp.copy()

    print('[ Compute ensemble statistics ]')

    # Load all ensemble states
    trigger(START_LOAD_STATE_FULL_VALIDATOR, 0)
    ss = 1
    for weight in global_weights:
        sid = encode_state_id(weight.state_id.t, weight.state_id.id, 0)
        if sid in state_buffer:
            continue
        progress = f"({ss}/{len(global_weights)})"
        print(f"Loading sid '{sid}' {progress}")
        load_ckpt_data(meta, sid, nprocs, "state1")
        ss += 1
    trigger(STOP_LOAD_STATE_FULL_VALIDATOR, 0)

    z_value = {}
    cpc_ids = [0] * (len(global_weights) - 1)
    for p in cpc:
        sid_DEL = -1
        print('─' * 100)
        print(f'|>  z-value statistics')
        print(f'|>  parameter-id: {p.id} ' + get_parameter_info(p))
        print('─' * 100)
        for name in variables:
            z_value[name] = np.array([])
        for weight in weights:
            if sid_DEL in state_buffer:
                del state_buffer[sid_DEL]
                gc.collect()
            trigger(START_ZVAL_FULL_VALIDATOR, 0)
            print(f'|>  M -> t: {weight.state_id.t}, id: {weight.state_id.id}')
            sid_EXCL = encode_state_id(weight.state_id.t, weight.state_id.id, p.id)
            sid_DEL = sid_EXCL
            if sid_EXCL not in state_buffer:
                progress = f"(1/1)"
                print(f"Loading sid '{sid_EXCL}' {progress}")
                load_ckpt_data(meta, sid_EXCL, nprocs, "state1")
            weights_M = [w for w in global_weights if w != weight]
            weight_norm = 0
            for w in weights_M:
                weight_norm += w.weight
            trigger(START_COMPUTE_ENAVG_VALIDATOR, 0)
            print("|    computing ensemble/M average")
            average = ensemble_wrapper(variables, weights_M, nprocs, meta, ensemble_mean, cpc_ids)
            # correct normalization
            for name in average:
                for proc, _ in enumerate(average[name]):
                    average[name][proc] /= weight_norm
            #print(f"average: {average['state1'][0][0:3]}")
            trigger(STOP_COMPUTE_ENAVG_VALIDATOR, 0)
            trigger(START_COMPUTE_ENSTDDEV_VALIDATOR, 0)
            print("|    computing ensemble/M sigma")
            stddev = ensemble_wrapper(variables, weights_M, nprocs, meta, ensemble_stddev, cpc_ids)
            # correct normalization and take root
            for name in stddev:
                for proc, _ in enumerate(stddev[name]):
                    stddev[name][proc] = np.sqrt(stddev[name][proc]/weight_norm)
            #print(f"stddev: {stddev['state1'][0][0:3]}")
            trigger(STOP_COMPUTE_ENSTDDEV_VALIDATOR, 0)
            print(f'|   -> computing RMSZ value')
            trigger(START_COMPUTE_RMSZ_VALIDATOR, 0)
            df_zval = evaluate_wrapper(variables, sid_EXCL, ndims, nprocs, meta, zval, reduce_sse, 'z_value', cpc)
            trigger(STOP_COMPUTE_RMSZ_VALIDATOR, 0)
            print('| ')
            print(f"|       RSMZ: {df_zval['value'].iloc[-1]}")
            print('| ')
            df_evaluate = df_evaluate.append(df_zval, ignore_index=True)
            trigger(STOP_ZVAL_FULL_VALIDATOR, 0)

    # TODO compute ensemble average and stddev for full ensemble states
    z_value_bias = {}
    for p in cpc:
        if p.id == 0:
            continue
        cpc_ids = [p.id] * (len(global_weights)-1)
        ss = 1
        trigger(START_LOAD_STATE_FULL_VALIDATOR, 0)
        for weight in global_weights:
            evict = encode_state_id(weight.state_id.t, weight.state_id.id, p.id - 1)
            del state_buffer[evict]
            gc.collect()
            sid = encode_state_id(weight.state_id.t, weight.state_id.id, p.id)
            if sid in state_buffer:
                continue
            progress = f"({ss}/{len(global_weights)})"
            print(f"Loading sid '{sid}' {progress}")
            load_ckpt_data(meta, sid, nprocs, "state1")
            ss += 1
        trigger(STOP_LOAD_STATE_FULL_VALIDATOR, 0)
        print('─' * 100)
        print(f'|>  z-value-bias statistics')
        print(f'|>  parameter-id: {p.id} ' + get_parameter_info(p))
        print('─' * 100)
        for name in variables:
            z_value_bias[name] = np.array([])
        for weight in weights:
            trigger(START_ZVAL_BIAS_FULL_VALIDATOR, 0)
            print(f'|>  M -> t: {weight.state_id.t}, id: {weight.state_id.id}')
            sid_EXCL = encode_state_id(weight.state_id.t, weight.state_id.id, p.id)
            weights_M = [w for w in global_weights if w != weight]
            weight_norm = 0
            for w in weights_M:
                weight_norm += w.weight
            trigger(START_COMPUTE_ENAVG_VALIDATOR, 0)
            print("|    computing ensemble/M average")
            average = ensemble_wrapper(variables, weights_M, nprocs, meta, ensemble_mean, cpc_ids)
            # correct normalization
            for name in average:
                for proc, _ in enumerate(average[name]):
                    average[name][proc] /= weight_norm
            #print(f"average: {average['state1'][0][0:3]}")
            trigger(STOP_COMPUTE_ENAVG_VALIDATOR, 0)
            trigger(START_COMPUTE_ENSTDDEV_VALIDATOR, 0)
            print("|    computing ensemble/M sigma")
            stddev = ensemble_wrapper(variables, weights_M, nprocs, meta, ensemble_stddev, cpc_ids)
            # correct normalization and take root
            for name in stddev:
                for proc, _ in enumerate(stddev[name]):
                    stddev[name][proc] = np.sqrt(stddev[name][proc]/weight_norm)
            #print(f"stddev: {stddev['state1'][0][0:3]}")
            trigger(STOP_COMPUTE_ENSTDDEV_VALIDATOR, 0)
            print(f'|   -> computing RMSZ-bias value')
            # TODO include the other rmsz test mentioned in Bake (X_c,i in X_0,j!=i ensemble)
            trigger(START_COMPUTE_RMSZ_VALIDATOR, 0)
            df_zval_bias = evaluate_wrapper(variables, sid_EXCL, ndims, nprocs, meta, zval, reduce_sse, 'z_value_bias', cpc)
            trigger(STOP_COMPUTE_RMSZ_VALIDATOR, 0)
            print('| ')
            print(f"|       RSMZ-bias: {df_zval_bias['value'].iloc[-1]}")
            print('| ')
            df_evaluate = df_evaluate.append(df_zval_bias, ignore_index=True)
            trigger(STOP_ZVAL_BIAS_FULL_VALIDATOR, 0)

    df_evaluate = reduce_evaluate_df(validators, df_evaluate)

    if validator_id == 0:
        # TODO get cycle in a better way
        cycle=state_ids[0].t
        df_evaluate.to_csv(experimentPath + f'validation_eval_{cycle}.csv', sep=",")

    print(df_evaluate)


class cpc_t:
    __items = 0
    def __init__(self, name, mode, type, parameter):
        self.name = name
        self.mode = None
        self.type = 0
        if mode == 'none':
            self.mode = FTI_CPC_MODE_NONE
        if mode == 'fpzip':
            self.mode = FTI_CPC_FPZIP
        if mode == 'zfp':
            self.mode = FTI_CPC_ZFP
        if mode == 'single' or mode == 'sp':
            self.mode = FTI_CPC_SINGLE
        if mode == 'half' or mode == 'hp':
            self.mode = FTI_CPC_HALF
        assert(self.mode is not None)
        if type == 'accuracy':
            self.type = FTI_CPC_ACCURACY
        if type == 'precision':
            self.type = FTI_CPC_PRECISION
        self.parameter = int(parameter)
        self.id = int(cpc_t.__items)
        cpc_t.__items += 1
    def num_parameters(self):
        return self.__items


class Validator:

    # set global class values
    def __init__(
            self,
            evaluation_function=None,
            evaluation_reduction=None,
            compare_function=None,
            compare_reduction=None,
    ):

        if compare_function is None:
            self.m_compare_function = sse
        if compare_reduction is None:
            self.m_compare_reduction = reduce_sse
        if evaluation_function is None:
            self.m_evaluation_function = energy
        if evaluation_reduction is None:
            self.m_evaluation_reduction = reduce_energy
        self.m_meta = {}
        self.m_meta_compare = {}
        self.m_is_validate = False
        self.m_meta_evaluate = {}
        self.m_meta_statistic = {}
        self.m_num_procs = 0
        self.m_state_dimension = 0
        self.m_cpc_parameters = []
        self.m_varnames = []
        self.m_varnames_cpc = []
        self.m_cycle = []
        self.m_num_cores = len(os.sched_getaffinity(0))
        self.m_state_ids = []
        self.m_weights = []
        self.init()

    # initialize validator
    def init(self):
        global server_socket, validator_socket, validator_id

        with open( experimentPath + 'compression.json') as fp:
            cpc_json = json.load(fp)

        host = get_node_name()

        with open(experimentPath + f'worker-{validator_id}-ip.dat', 'w') as f:
            f.write(host)

        self.m_varnames = cpc_json['variables']

        print(self.m_varnames)

        # required to unify the meta data creation
        for name in self.m_varnames:
            self.m_cpc_parameters.append(cpc_t(name, 'none', 0, 0))

        if cpc_json['compression']['method'] == 'adapt': return

        assert(cpc_json['compression']['method'] == 'validate')

        for item in cpc_json['compression']['validate']:
            name = None
            mode = None
            type = None
            if 'name' in item:
                name = item['name']
                if item['name'] not in self.m_varnames_cpc:
                    self.m_varnames_cpc.append(item['name'])
            if 'mode' in item:
                mode = item['mode']
            if 'type' in item:
                type = item['type']
            if 'parameter' in item:
                try:
                    for p in item['parameter']:
                        self.m_cpc_parameters.append(cpc_t(name, mode, type, int(p)))
                except:
                    self.m_cpc_parameters.append(cpc_t(name, mode, type, int(item['parameter'])))
            else:
                self.m_cpc_parameters.append(cpc_t(name, mode, 0, 0))

        self.m_is_validate = True

        for cpc in self.m_cpc_parameters:
            print(f'[{cpc.id}] name: {cpc.name} mode: {cpc.mode}, parameter: {cpc.parameter}')


    def populate_meta( self, states, cpc ):

        for state in states:

            for p in cpc:

                sid = encode_state_id(state.state_id.t, state.state_id.id, p.id)

                if sid in self.m_meta:
                    continue

                self.m_cycle = state.state_id.t

                path = checkpointPath + str(sid)

                meta_pattern = path + '/Meta*-worker*-serialized.fti'
                ckpt_pattern = path + '/Ckpt*-worker*-serialized.fti'
                meta_files = glob.glob(meta_pattern)
                ckpt_files = glob.glob(ckpt_pattern)

                meta_item = {}

                meta_item["weight"] = state.weight

                proc = 0
                for idx, f in enumerate(meta_files):
                    fh = open(f)
                    fstring = fh.read()
                    fh.close()

                    procs_per_node = 0
                    buf = io.StringIO(fstring)

                    base = 0

                    self.m_state_dimension = 0
                    for line in iter(lambda: buf.readline(), ""):
                        nb_lines = int(line.replace("\n", ""))
                        meta_str = ""
                        for i in range(nb_lines):
                            meta_str = meta_str + buf.readline()
                        config = configparser.ConfigParser()
                        config.read_string(meta_str)

                        vars = {}
                        varid = 0
                        count = 0
                        while f'var{varid}_id' in config['0']:
                            name        = config['0'][f'var{varid}_idchar']
                            mode        = int(config['0'][f'var{varid}_compression_mode'])
                            type        = int(config['0'][f'var{varid}_compression_type'])
                            parameter   = int(config['0'][f'var{varid}_compression_parameter'])
                            size        = int(config['0'][f'var{varid}_size'])
                            count       = int(config['0'][f'var{varid}_count'])
                            if name in self.m_varnames:
                                vars[name] = {
                                    "ckpt_file" : ckpt_files[idx],
                                    "mode"      : mode,
                                    "type"      : type,
                                    "parameter" : parameter,
                                    "offset"    : base,
                                    "size"      : size,
                                    "count"     : count
                                }
                            base += size
                            varid += 1

                        meta_item[proc] = vars

                        self.m_state_dimension += count

                        proc += 1
                        procs_per_node += 1

                self.m_num_procs = proc

                self.m_meta[sid] = meta_item


    def handle_request( self, request ):

        global state_buffer

        gc.collect()

        #ty = request.WhichOneof('content')
        #if ty == 'prefetch_request_validator':
        #    weight = request.prefetch_request_validator.weight
        #    state_id = request.prefetch_request_validator.weight.state_id
        #    print(f"[validator received prefetch request for state t:{state_id.t}, id:{state_id.id}]")
        #    self.populate_meta([weight], self.m_cpc_parameters)
        #    ss = 1
        #    for p in self.m_cpc_parameters:
        #        sid = encode_state_id(state_id.t, state_id.id, p.id)
        #        if sid in state_buffer:
        #            continue
        #        progress = f"({ss}/{len(self.m_cpc_parameters)})"
        #        print(f"Loading sid '{sid}' {progress}")
        #        load_ckpt_data(self.m_meta, sid, self.m_num_procs, "state1")
        #        ss += 1
        #    maybe_write(is_server=False, validator_id=validator_id)
        #    return

        print(f"[validator received validation request]")
        self.m_weights = []
        self.m_state_ids = []
        for item in request.validation_request.to_validate:
            self.m_weights.append(item)
            self.m_state_ids.append(item.state_id)

        validators = request.validation_request.validator_ids
        validators.remove(0)

        weights_temp = self.m_weights.copy()
        global_weights = allreduce_weights(validators, self.m_weights)
        self.m_weights = weights_temp.copy()

        self.populate_meta(global_weights, self.m_cpc_parameters)

        print(f"state_dimension: {self.m_state_dimension}")
        print(f"num_procs: {self.m_num_procs}")

        ss = 1
        trigger(START_LOAD_STATE_FULL_VALIDATOR, 0)
        for weight in self.m_weights:
            for p in self.m_cpc_parameters:
                sid = encode_state_id(weight.state_id.t, weight.state_id.id, p.id)
                if sid in state_buffer:
                    continue
                progress = f"({ss}/{len(self.m_weights) * len(self.m_cpc_parameters)})"
                print(f"Loading sid '{sid}' {progress}")
                load_ckpt_data(self.m_meta, sid, self.m_num_procs, "state1")
                ss += 1
        trigger(STOP_LOAD_STATE_FULL_VALIDATOR, 0)

        if self.m_is_validate:
            trigger(START_VALIDATE_VALIDATOR, 0)
            validate(
                self.m_meta,
                self.m_compare_function,
                self.m_compare_reduction,
                self.m_evaluation_function,
                self.m_evaluation_reduction,
                self.m_state_dimension,
                self.m_num_procs,
                self.m_varnames,
                self.m_cpc_parameters,
                self.m_state_ids,
                self.m_weights,
                validators
            )
            trigger(STOP_VALIDATE_VALIDATOR, 0)

        # remove states from before
        state_buffer.clear()

        # remove old meta data
        self.m_meta.clear()

        maybe_write(is_server=False, validator_id=validator_id)

    # main
    def run(self):
        empty = cm.Message()
        empty.validation_request.SetInParent()

        while True:
            # server communication
            response = cm.Message()
            send_message(server_socket, response)
            msg = server_socket.recv()
            request = parse(msg)

            connect_validator_sockets()

            print("received task... ")
            if request == empty:
                continue

            self.handle_request( request )


if __name__ == "__main__":
    nprocs = len(os.sched_getaffinity(0))
    print("number of cores: ", nprocs)
    print("++ EXECUTING WITH DEFAULT VALIDATOR ++")
    __default_validator = Validator()
    __default_validator.run()
