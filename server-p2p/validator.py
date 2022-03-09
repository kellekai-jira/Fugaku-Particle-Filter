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



from utils import get_node_name
from common import bind_socket, parse

context = zmq.Context()

experimentPath = os.getcwd() + '/'
checkpointPath = os.path.dirname(os.getcwd()) + '/Global/'

print(f"experimentPath: {experimentPath}")
print(f"checkpointPath: {checkpointPath}")

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



FTI_CPC_MODE_NONE   = 0
FTI_CPC_FPZIP       = 1
FTI_CPC_ZFP         = 2
FTI_CPC_SINGLE      = 3
FTI_CPC_HALF        = 4
FTI_CPC_STRIP       = 5
FTI_CPC_TYPE_NONE   = 0
FTI_CPC_ACCURACY    = 1
FTI_CPC_PRECISION   = 2


def energy(data):

    energy_sum = 0

    for val in data:
        energy_sum += 0.5 * val ** 2

    return energy_sum

def reduce_energy(parts, n):

    energy_avg = 0

    for part in parts:
        energy_avg += part

    return energy_avg / n


'''
    computes the sum of squared differences between two states
    data[0] <- first state
    data[1] <- second state
'''
def sse(data):

    sigma = 0
    a1 = data[0]
    a2 = data[1]

    assert(len(a1) == len(a2))

    for i in range(len(a1)):
        sigma += (a1[i] - a2[i]) ** 2

    return sigma

def reduce_sse(parts, n):
    sigma = 0
    for part in parts:
        sigma += part

    return np.sqrt(sigma / n)


def ensemble_mean(proc, sids, name, meta_data):

    x_avg = np.array([])
    for sid in sids:
        weight = meta_data[sid]['weight']
        meta = meta_data[sid][proc][name]
        ckpt_file = meta['ckpt_file']
        ckpt = open(ckpt_file, 'rb')
        mode = meta['mode']

        if mode == 0:
            ckpt.seek(meta['offset'])
            bytes = ckpt.read(meta['size'])
            data = array.array('d', bytes)

        else:
            data = []
            n = meta['count']
            bs = 1024 * 1024
            nb = n // bs + (1 if n % bs != 0 else 0)

            ckpt.seek(meta['offset'])

            for b in range(nb):
                bytes = ckpt.read(8)
                bs = int.from_bytes(bytes, byteorder='little')
                bytes = ckpt.read(bs)
                block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                data = [*data, *block]

        ckpt.close()
        if x_avg.size == 0:
            x_avg = weight * np.array(data)
        else:
            x_avg += weight * np.array(data)

    return x_avg


def wrapper2dict( wrapper ):
    d = {}
    for variable in wrapper.variables:
        d[variable.name] = {}
        for idx, rank in enumerate(variable.ranks):
            d[variable.name][idx] = rank.data
    return d


def dict2wrapper( dct ):
    wrapper = cm.StatisticWrapper()
    for name in dct:
        variable = cm.StatisticVariable()
        variable.name = name
        for data in dct[name]:
            rank = cm.StatisticData()
            rank.data.extend(dct[name][data])
            variable.ranks.append(rank)
        wrapper.variables.append(variable)
    return wrapper


def ensemble_stddev(proc, sids, name, meta_data, avg_dict):

    x_stddev = np.array([])
    for sid in sids:
        weight = meta_data[sid]['weight']
        meta = meta_data[sid][proc][name]
        ckpt_file = meta['ckpt_file']
        ckpt = open(ckpt_file, 'rb')
        mode = meta['mode']

        if mode == 0:
            ckpt.seek(meta['offset'])
            bytes = ckpt.read(meta['size'])
            data = array.array('d', bytes)

        else:
            data = []
            n = meta['count']
            bs = 1024 * 1024
            nb = n // bs + (1 if n % bs != 0 else 0)

            ckpt.seek(meta['offset'])

            for b in range(nb):
                bytes = ckpt.read(8)
                bs = int.from_bytes(bytes, byteorder='little')
                bytes = ckpt.read(bs)
                block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                data = [*data, *block]

        ckpt.close()
        if x_stddev.size == 0:
            x_stddev = weight * ( (np.array(data) - avg_dict[name][proc]) ** 2 )
        else:
            x_stddev += weight * ( (np.array(data) - avg_dict[name][proc]) ** 2 )

    return x_stddev

def evaluate_state(proc, sid, name, meta_data, func):

    meta = meta_data[sid][proc][name]
    ckpt_file = meta['ckpt_file']
    ckpt = open(ckpt_file, 'rb')
    t, id, mode = decode_state_id(sid)

    if mode == 0:
        ckpt.seek(meta['offset'])
        bytes = ckpt.read(meta['size'])
        data = array.array('d', bytes)

    else:
        data = []
        n = meta['count']
        bs = 1024 * 1024
        nb = n // bs + (1 if n % bs != 0 else 0)

        ckpt.seek(meta['offset'])

        for b in range(nb):
            bytes = ckpt.read(8)
            bs = int.from_bytes(bytes, byteorder='little')
            bytes = ckpt.read(bs)
            block = fpzip.decompress(bytes, order='C')[0, 0, 0]
            data = [*data, *block]

    ckpt.close()

    return func(data)

def compare_states(proc, sid, name, meta_data, func):

    states = []

    for state in meta_data[sid]:

        meta = meta_data[sid][state][proc][name]
        ckpt_file = meta['ckpt_file']
        ckpt = open(ckpt_file, 'rb')

        if state == 0:
            ckpt.seek(meta['offset'])
            bytes = ckpt.read(meta['size'])
            states.append(array.array('d', bytes))

        else:
            data = []
            n = meta['count']
            bs = 1024 * 1024
            nb = n // bs + (1 if n % bs != 0 else 0)

            ckpt.seek(meta['offset'])

            for b in range(nb):
                bytes = ckpt.read(8)
                bs = int.from_bytes(bytes, byteorder='little')
                bytes = ckpt.read(bs)
                block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                data = [*data, *block]

            states.append(data)

        ckpt.close()

    return func(states)


def ensemble_statistics(cycle, meta_statistic, num_procs_application, validator_id, request):

    pool = Pool()

    sids = list(meta_statistic.keys())
    names = list(meta_statistic[sids[0]][0].keys())

    wrapper = cm.StatisticWrapper()
    for name in names:
        var = cm.StatisticVariable()
        var.name = name
        results = pool.map(partial(ensemble_mean, sids=sids, name=name, meta_data=meta_statistic), range(num_procs_application))
        for result in results:
            data = cm.StatisticData()
            data.data.extend(result)
            var.ranks.append(data)
        wrapper.variables.append(var)


    pattern = os.getcwd() + '/worker-*-ip.dat'
    worker_ip_files = glob.glob(pattern)

    print(worker_ip_files)
    if validator_id == 0:
        stddev = {}
        average = {}
        validator_ids = request.validation_request.validator_ids
        worker_ids = []
        validation_sockets = []
        p = re.compile("worker-(.*)-ip.dat")
        for fn in worker_ip_files:
            id = int(p.search(os.path.basename(fn)).group(1))
            if id not in validator_ids: continue
            if id not in worker_ids:
                with open(fn, 'r') as file:
                    ip = file.read().rstrip()
                addr = "tcp://" + ip + ":4001"
                so = context.socket(zmq.REP)
                so.connect(addr)
                validation_sockets.append(so)
                worker_ids.append(id)
                print(f"connected to validator: {id} with addr: {addr}")
        if len(worker_ids) > 0:
            for idv, variable in enumerate(wrapper.variables):
                average[variable.name] = {}
                for idr, rank in enumerate(variable.ranks):
                    average[variable.name][idr] = np.array(rank.data)
            for idx, vsock in enumerate(validation_sockets):
                print(f"[{worker_ids[idx]}] waiting for worker message...")
                msg = vsock.recv()# only polling
                avg_wrapper = cm.StatisticWrapper()
                avg_wrapper.ParseFromString(msg)
                for idv, variable in enumerate(avg_wrapper.variables):
                    for idr, rank in enumerate(variable.ranks):
                        average[variable.name][idr] += rank.data
            average_wrapper = dict2wrapper( average )
            for idx, vsock in enumerate(validation_sockets):
                send_message(vsock, average_wrapper)
                print(f"[{worker_ids[idx]}] sent average to worker!")
            for key in average:
                print(f"average variable '{key}': {average[key][0][0:5]}")
            pool = Pool()
            wrapper_stddev = cm.StatisticWrapper()
            for name in names:
                var = cm.StatisticVariable()
                var.name = name
                results = pool.map(partial(ensemble_stddev, sids=sids, name=name, meta_data=meta_statistic, avg_dict=average), \
                                   range(num_procs_application))
                for result in results:
                    data = cm.StatisticData()
                    data.data.extend(result)
                    var.ranks.append(data)
                wrapper_stddev.variables.append(var)
            for idv, variable in enumerate(wrapper_stddev.variables):
                stddev[variable.name] = {}
                for idr, rank in enumerate(variable.ranks):
                    stddev[variable.name][idr] = np.array(rank.data)
            for idx, vsock in enumerate(validation_sockets):
                print(f"[{worker_ids[idx]}] waiting for worker message...")
                msg = vsock.recv()# only polling
                std_wrapper = cm.StatisticWrapper()
                std_wrapper.ParseFromString(msg)
                for idv, variable in enumerate(std_wrapper.variables):
                    for idr, rank in enumerate(variable.ranks):
                        stddev[variable.name][idr] += rank.data
                response = cm.Message()
                send_message(vsock, response)
                print(f"[{worker_ids[idx]}] sent stddev to worker!")
            for name in stddev:
                num_procs = 0
                state_dims = []
                for rank in stddev[name]:
                    stddev[name][rank] = np.sqrt(stddev[name][rank])
                    num_procs += 1
                    state_dims.append(len(stddev[name][rank]))
            for key in stddev:
                print(f"stddev variable '{key}': {stddev[key][0][0:5]}")

        write_lorenz(average, stddev, cycle, num_procs, state_dims)

    else:
        addr = "tcp://*:4001"

        print("sending message...")
        socket, port_socket = \
            bind_socket(context, zmq.REQ, addr)

        send_message(socket, wrapper)
        msg = socket.recv()  # only polling
        avg_wrapper = cm.StatisticWrapper()
        avg_wrapper.ParseFromString(msg)
        average = wrapper2dict( avg_wrapper )
        pool = Pool()
        wrapper_stddev = cm.StatisticWrapper()
        for name in names:
            var = cm.StatisticVariable()
            var.name = name
            results = pool.map(
                partial(ensemble_stddev, sids=sids, name=name, meta_data=meta_statistic, avg_dict=average), \
                range(num_procs_application))
            for result in results:
                data = cm.StatisticData()
                data.data.extend(result)
                var.ranks.append(data)
            wrapper_stddev.variables.append(var)
        send_message(socket, wrapper_stddev)
        msg = socket.recv()  # only polling
        print("received average from master validator!")
        socket.close()



def compare(proc, sids, name, meta, func):

    states = []

    for sid in sids:

        meta = meta[sid][proc][name]
        ckpt_file = meta['ckpt_file']
        ckpt = open(ckpt_file, 'rb')

        if meta['mode'] == 0:
            ckpt.seek(meta['offset'])
            bytes = ckpt.read(meta['size'])
            states.append(array.array('d', bytes))

        else:
            data = []
            n = meta['count']
            bs = 1024 * 1024
            nb = n // bs + (1 if n % bs != 0 else 0)

            ckpt.seek(meta['offset'])

            for b in range(nb):
                bytes = ckpt.read(8)
                bs = int.from_bytes(bytes, byteorder='little')
                bytes = ckpt.read(bs)
                block = fpzip.decompress(bytes, order='C')[0, 0, 0]
                data = [*data, *block]

            states.append(data)

        ckpt.close()

    return func(states)


def compare_states_wrapper( variables, sids, ndim, nprocs, meta, func, reduce_func, operation, cpc ):
    pool = Pool()

    dfl = []
    for name in variables:
        original = decode_state_id( sids[0] )
        compared = decode_state_id( sids[1] )
        data_size = 0
        size_original = 0
        size_compared = 0
        for proc in range(nprocs):
            data_size += float(meta[sids[0]][proc][name]['count'] * 8)
            size_original += float(meta[sids[0]][proc][name]['size'])
            size_compared += float(meta[sids[1]][proc][name]['size'])
        rate_original = size_original / data_size
        rate_compared = size_compared / data_size
        results = pool.map(partial(compare, sids=sids, name=name, meta=meta, func=func), range(nprocs))
        reduced = reduce_func(results, ndim)
        reduced.append( {
            'variable' : name,
            'operation' : operation,
            'mode_original' : cpc[original[2]]['mode'],
            'mode_compared' : cpc[compared[2]]['mode'],
            'parameter_original': cpc[original[2]]['parameter'],
            'parameter_compared': cpc[compared[2]]['parameter'],
            't' : original[0],
            'id' : original[1],
            'rate_original' : rate_original,
            'rate_compared' : rate_compared
        } )
        print(reduced[-1])

    return pd.DataFrame(dfl)



def validate(meta, meta_compare, compare_function, compare_reduction, meta_evaluate, evaluate_function,
             evaluate_reduction, state_dimension, num_procs_application, validator_id, variables, cpc, state_ids):


    for state_id in state_ids:
        original = encode_state_id(state_id.t, state_id.id, 0)
        for p in cpc:
            compared = encode_state_id( state_id.t, state_id.id, p.id )
            test_df = compare_states_wrapper( variables, [original, compared], state_dimension, num_procs_application, meta, sse, reduce_sse, 'RMSE', cpc)
            print(test_df)
    #sigmas = []

    #pool = Pool()

    #for sid in meta_compare:
    #    t, id, pid = decode_state_id(sid)
    #    for name in meta_compare[sid][pid][0]:
    #        results = pool.map(partial(compare_states, sid=sid, name=name, meta_data=meta_compare, func=compare_function), range(num_procs_application))
    #        sigma = compare_reduction(results, state_dimension)
    #        mode = meta_compare[sid][pid][0][name]['mode']
    #        parameter = meta_compare[sid][pid][0][name]['parameter']
    #        size_compressed = float(meta_compare[sid][pid][0][name]['size'])
    #        size_original = float(meta_compare[sid][pid][0][name]['count'] * 8)
    #        rate = size_original / size_compressed
    #        sigmas.append( { 'variable' : name, 't' : t, 'id' : id, 'mode' : mode, 'parameter' : parameter, 'rate' : rate, 'sigma' : sigma } )
    #        print(f"[{name}|t:{t}|id:{id}|pid:{pid}] sigma -> {sigma}")

    #df = pd.DataFrame(sigmas)
    #df_file = experimentPath + f"validator{validator_id}-compare-t{t}.csv"
    #df.to_csv(df_file, sep='\t', encoding='utf-8')

    #pool = Pool()

    #energies = []
    #for sid in meta_evaluate:
    #    t, id, pid = decode_state_id(sid)
    #    for name in meta_evaluate[sid][0]:
    #        results = pool.map(partial(evaluate_state, sid=sid, name=name, meta_data=meta_evaluate, func=evaluate_function), range(num_procs_application))
    #        energy = evaluate_reduction(results, state_dimension)
    #        mode = meta_evaluate[sid][0][name]['mode']
    #        parameter = meta_evaluate[sid][0][name]['parameter']
    #        size_compressed = float(meta_evaluate[sid][0][name]['size'])
    #        size_original = float(meta_evaluate[sid][0][name]['count'] * 8)
    #        rate = size_original / size_compressed
    #        energies.append( { 'variable' : name, 't' : t, 'id' : id, 'mode' : mode, 'parameter' : parameter, 'rate' : rate, 'energy' : energy } )
    #        print(f"[{name}|t:{t}|id:{id}|pid:{pid}] energy -> {energy}")

    #df = pd.DataFrame(energies)
    #df_file = experimentPath + f"validator{validator_id}-evaluate-t{t}.csv"
    #df.to_csv(df_file, sep='\t', encoding='utf-8')


def send_message(socket, data):
    socket.send(data.SerializeToString())


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


class cpc_t:
    __items = 0
    def __init__(self, name, mode, parameter):
        self.name = name
        self.mode = None
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
        assert( self.mode != None )
        self.parameter = parameter
        self.id = cpc_t.__items
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

        if compare_function == None:
            self.m_compare_function = sse
        if compare_reduction == None:
            self.m_compare_reduction = reduce_sse
        if evaluation_function == None:
            self.m_evaluation_function = energy
        if evaluation_reduction == None:
            self.m_evaluation_reduction = reduce_energy
        self.m_meta = {}
        self.m_meta_compare = {}
        self.m_is_validate = False
        self.m_meta_evaluate = {}
        self.m_meta_statistic = {}
        self.m_num_procs = 0
        self.m_validator_id = 0
        self.m_state_dimension = 0
        self.m_socket = None
        self.m_cpc_parameters = []
        self.m_varnames = []
        self.m_varnames_cpc = []
        self.m_cycle = []
        self.m_num_cores = len(os.sched_getaffinity(0))
        self.init()

    # initialize validator
    def init(self):

        with open( experimentPath + 'compression.json') as fp:
            cpc_json = json.load(fp)

        context.setsockopt(zmq.LINGER, 0)
        addr = "tcp://*:4000"

        self.m_socket, port_socket = \
            bind_socket(context, zmq.REQ, addr)

        assert( os.environ.get('MELISSA_DA_WORKER_ID') is not None )

        self.m_validator_id = int(os.getenv('MELISSA_DA_WORKER_ID'))

        host = get_node_name()

        with open(experimentPath + f'worker-{self.m_validator_id}-ip.dat', 'w') as f:
            f.write(host)

        self.m_varnames = cpc_json['variables']

        print(self.m_varnames)

        # required to unify the meta data creation
        for name in self.m_varnames:
            self.m_cpc_parameters.append(cpc_t(name, 'none', 0))

        if cpc_json['compression']['method'] == 'adapt': return

        assert(cpc_json['compression']['method'] == 'validate')

        for item in cpc_json['compression']['validate']:
            if item['name'] not in self.m_varnames_cpc:
                self.m_varnames_cpc.append(item['name'])
            self.m_cpc_parameters.append(cpc_t(
                item['name'],
                item['mode'],
                item['parameter']
            ))

        self.m_is_validate = True

        for cpc in self.m_cpc_parameters:
            print(f'[{cpc.id}] name: {cpc.name} mode: {cpc.mode}, parameter: {cpc.parameter}')


    def populate_meta( self, states, cpc ):

        # remove old meta data
        self.m_meta.clear()

        for state in states:

            for p in cpc:

                sid = encode_state_id(state.state_id.t, state.state_id.id, p.id)

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


    def create_metadata_statistic( self, states ):

        # remove old meta data
        self.m_meta_statistic.clear()

        for state in states:

            sid = encode_state_id(state.state_id.t, state.state_id.id, 0)

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

            self.m_meta_statistic[sid] = meta_item


    def create_metadata( self, states ):

        # remove old meta data
        self.m_meta_evaluate.clear()
        self.m_meta_compare.clear()

        for state in states:

            for cpc in self.m_cpc_parameters:

                sid = encode_state_id(state.state_id.t, state.state_id.id, cpc.id)

                self.m_cycle = state.state_id.t

                state_item = {}

                for p in [0,cpc.id]:

                    path = checkpointPath + str(encode_state_id(state.state_id.t, state.state_id.id, p))

                    meta_pattern = path + '/Meta*-worker*-serialized.fti'
                    ckpt_pattern = path + '/Ckpt*-worker*-serialized.fti'
                    meta_files = glob.glob(meta_pattern)
                    ckpt_files = glob.glob(ckpt_pattern)

                    meta_item = {}
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
                                if name in self.m_varnames_cpc:
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
                    state_item[p] = meta_item

                    cid = encode_state_id(state.state_id.t, state.state_id.id, p)
                    if cid not in self.m_meta_evaluate:
                        self.m_meta_evaluate[cid] = meta_item

                self.m_meta_compare[sid] = state_item

    def info(self):
        for m in self.m_meta_compare:
            print(self.m_meta_compare[m])


    def handle_validation_request( self, request ):
        states = []
        self.m_state_ids = []
        for item in request.validation_request.to_validate:
            states.append(item)
            self.m_state_ids.append(item.state_id)
            print(item)

        self.populate_meta(states, self.m_cpc_parameters)
        #self.create_metadata_statistic(states)

        print(f"state_dimension: {self.m_state_dimension}")
        print(f"num_procs: {self.m_num_procs}")
        print(f"validator_id: {self.m_validator_id}")

        #ensemble_statistics(self.m_cycle, self.m_meta_statistic, self.m_num_procs, self.m_validator_id, request)

        if self.m_is_validate:
            #self.create_metadata(states)

            validate(
                self.m_meta,
                self.m_meta_compare,
                self.m_compare_function,
                self.m_compare_reduction,
                self.m_meta_evaluate,
                self.m_evaluation_function,
                self.m_evaluation_reduction,
                self.m_state_dimension,
                self.m_num_procs,
                self.m_validator_id,
                self.m_varnames,
                self.m_cpc_parameters,
                self.m_state_ids
            )


    def handle_statistic_request( self, request ):
        print("received statistic request")

        states = []
        for item in request.statistic_request.weights:
            states.append(item)

        self.create_metadata_statistic(states)
        print(f"state_dimension: {self.m_state_dimension}")
        print(f"num_procs: {self.m_num_procs}")
        print(f"validator_id: {self.m_validator_id}")

        ensemble_statistics(self.m_meta_statistic, self.m_num_procs, self.m_validator_id)



    # main
    def run(self):

        while True:
            response = cm.Message()
            send_message(self.m_socket, response)

            msg = self.m_socket.recv()
            request = parse(msg)
            print("received task... ", request)

            empty = cm.Message()
            empty.validation_request.SetInParent()
            if request == empty:
                continue

            ty = request.WhichOneof('content')
            if ty == 'validation_request':
                self.handle_validation_request(request)
            elif ty == 'statistic_request':
                self.handle_statistic_request(request)
            else:
                print("Wrong message type received!")
                assert False



if __name__ == "__main__":
    nprocs = len(os.sched_getaffinity(0))
    print("number of cores: ", nprocs)
    print("++ EXECUTING WITH DEFAULT VALIDATOR ++")
    __default_validator = Validator()
    __default_validator.run()
