from multiprocessing import Pool
import numpy as np
import configparser
import p2p_pb2 as cm
import io
import fpzip
import glob
from functools import partial
import array
import json
import os
import zmq
import pandas as pd

from utils import get_node_name
from common import bind_socket, parse


experimentPath = os.getcwd() + '/'
checkpointPath = os.path.dirname(os.getcwd()) + '/Global/'

print(f"experimentPath: {experimentPath}")
print(f"checkpointPath: {checkpointPath}")

FTI_CPC_MODE_NONE   = 0
FTI_CPC_FPZIP       = 1
FTI_CPC_ZFP         = 2
FTI_CPC_SINGLE      = 3
FTI_CPC_HALF        = 4
FTI_CPC_STRIP       = 5
FTI_CPC_TYPE_NONE   = 0
FTI_CPC_ACCURACY    = 1
FTI_CPC_PRECISION   = 2


def evaluate_state(self, proc, sid, meta_data, func):

    meta = meta_data[sid][proc]
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

def compare_states(self, proc, sid, meta_data, func):

    states = []

    for state in meta_data[sid]:
    #for state in meta_data:

        meta = meta_data[sid][state][proc]
        #meta = meta_data[state][proc]
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


def send_message(socket, data):
    socket.send(data.SerializeToString())


def elegantPair( x, y ):
    return  (x * x + x + y) if (x >= y) else (y * y + x)


def elegantUnpair( z ) -> (int, int):
    sqrtz = int(np.floor(np.sqrt(z)))
    sqz = int(sqrtz ** 2)
    if ((z - sqz) >= sqrtz):
        return sqrtz, z - sqz - sqrtz
    else:
        return z - sqz, sqrtz


def encode_state_id( t, id, mode ):
    return elegantPair( mode, elegantPair( t, id ) )


def decode_state_id( hash ):
    mode, tid = elegantUnpair( hash )
    t, id = elegantUnpair( tid )
    return t, id, mode


class cpc_t:
    __items = 1
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
            self.m_compare_function = self.sse
        if compare_reduction == None:
            self.m_compare_reduction = self.reduce_sse
        if evaluation_function == None:
            self.m_evaluation_function = self.energy
        if evaluation_reduction == None:
            self.m_evaluation_reduction = self.reduce_energy
        self.m_meta_compare = {}
        self.m_meta_evaluate = {}
        self.m_num_procs = 0
        self.m_validator_id = 0
        self.m_state_dimension = 0
        self.m_socket = None
        self.m_cpc_parameters = []
        self.m_num_cores = len(os.sched_getaffinity(0))
        self.init()

    # initialize validator
    def init(self):

        with open( experimentPath + 'compression.json') as fp:
            cpc_json = json.load(fp)

        assert(cpc_json['compression']['method'] == 'validate')

        for item in cpc_json['compression']['validate']:
            self.m_cpc_parameters.append(cpc_t(
                item['name'],
                item['mode'],
                item['parameter']
            ))

        for cpc in self.m_cpc_parameters:
            print(f'[{cpc.id}] name: {cpc.name} mode: {cpc.mode}, parameter: {cpc.parameter}')

        context = zmq.Context()
        context.setsockopt(zmq.LINGER, 0)
        addr = "tcp://*:4000"  # TODO: make ports changeable, maybe even select them automatically!

        self.m_socket, port_socket = \
                bind_socket(context, zmq.REQ, addr)


        assert( os.environ.get('MELISSA_DA_WORKER_ID') is not None )

        self.m_validator_id = os.getenv('MELISSA_DA_WORKER_ID')

        host = get_node_name()

        with open(experimentPath + f'worker-{self.m_validator_id}-ip.dat', 'w') as f:
            f.write(host)


    def create_metadata( self, states ):

        for state in states:

            for cpc in self.m_cpc_parameters:

                sid = encode_state_id(state.t, state.id, cpc.id)

                state_item = {}

                for p in [0,cpc.id]:

                    path = checkpointPath + str(encode_state_id(state.t, state.id, p))

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

                            mode = int(config["0"]["var1_compression_mode"])
                            type = int(config["0"]["var1_compression_type"])
                            parameter = int(config["0"]["var1_compression_parameter"])
                            offset = int(config["0"]["var1_pos"])
                            size = int(config["0"]["var1_size"])
                            count = int(config["0"]["var1_count"])
                            meta_item[proc] = {
                                "ckpt_file": ckpt_files[idx],
                                "mode": mode,
                                "type": type,
                                "parameter": parameter,
                                "offset": base + offset,
                                "size": size,
                                "count": count
                            }
                            base += size + offset

                            self.m_state_dimension += count

                            proc += 1
                            procs_per_node += 1

                    self.m_num_procs = proc
                    state_item[p] = meta_item

                    cid = encode_state_id(state.t, state.id, p)
                    if cid not in self.m_meta_evaluate:
                        self.m_meta_evaluate[cid] = meta_item

                self.m_meta_compare[sid] = state_item

    def info(self):
        for m in self.m_meta_compare:
            print(self.m_meta_compare[m])

    def energy(self, data):

        energy_sum = 0

        for val in data:
            energy_sum += 0.5 * val ** 2

        return energy_sum

    def reduce_energy(self, parts, n):

        energy_avg = 0

        for part in parts:
            energy_avg += part

        return energy_avg / n

    def sse(self, data):

        sigma = 0
        a1 = data[0]
        a2 = data[1]

        assert(len(a1) == len(a2))

        for i in range(len(a1)):
            sigma += (a1[i] - a2[i]) ** 2

        return sigma

    def reduce_sse(self, parts, n):
        sigma = 0
        for part in parts:
            sigma += part

        return np.sqrt(sigma / n)


    def validate(self):

        sigmas = []

        print("num cores: ", self.m_num_cores)

        pool = Pool()

        for sid in self.m_meta_compare:
            #results = parmap(partial(self.compare_states, id=sid), range(self.m_num_procs), nprocs = self.m_num_cores)
            results = pool.map(partial(compare_states, sid=sid, meta_data=self.m_meta_compare, func=self.m_compare_function), range(self.m_num_procs))
            sigma = self.m_compare_reduction(results, self.m_state_dimension)
            t, id, pid = decode_state_id(sid)
            mode = self.m_meta_compare[sid][pid][0]['mode']
            parameter = self.m_meta_compare[sid][pid][0]['parameter']
            size_compressed = float(self.m_meta_compare[sid][pid][0]['size'])
            size_original = float(self.m_meta_compare[sid][pid][0]['count'] * 8)
            rate = size_original / size_compressed
            sigmas.append( { 't' : t, 'id' : id, 'mode' : mode, 'parameter' : parameter, 'rate' : rate, 'sigma' : sigma } )
            print(f"[t:{t}|id:{id}|pid:{pid}] sigma -> {sigma}")

        df = pd.DataFrame(sigmas)
        df_file = experimentPath + f"validator{self.m_validator_id}-compare-t{t}.csv"
        df.to_csv(df_file, sep='\t', encoding='utf-8')

        pool = Pool()

        energies = []
        for sid in self.m_meta_evaluate:
            #results = parmap(partial(self.evaluate_state, sid=sid), range(self.m_num_procs), nprocs = self.m_num_cores)
            results = pool.map(partial(evaluate_state, sid=sid, meta_data=self.m_meta_evaluate, func=self.m_evaluation_function), range(self.m_num_procs))
            energy = self.m_evaluation_reduction(results, self.m_state_dimension)
            t, id, pid = decode_state_id(sid)
            mode = self.m_meta_evaluate[sid][0]['mode']
            parameter = self.m_meta_evaluate[sid][0]['parameter']
            size_compressed = float(self.m_meta_evaluate[sid][0]['size'])
            size_original = float(self.m_meta_evaluate[sid][0]['count'] * 8)
            rate = size_original / size_compressed
            energies.append( { 't' : t, 'id' : id, 'mode' : mode, 'parameter' : parameter, 'rate' : rate, 'energy' : energy } )
            print(f"[t:{t}|id:{id}|pid:{pid}] energy -> {energy}")

        df = pd.DataFrame(energies)
        df_file = experimentPath + f"validator{self.m_validator_id}-evaluate-t{t}.csv"
        df.to_csv(df_file, sep='\t', encoding='utf-8')


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

            states = []
            for item in request.validation_request.to_validate:
                    states.append(item)
                    print(item)

            self.create_metadata( states )
            self.validate()


if __name__ == "__main__":
    nprocs = len(os.sched_getaffinity(0))
    print("number of cores: ", nprocs)
    print("++ EXECUTING WITH DEFAULT VALIDATOR ++")
    __default_validator = Validator()
    __default_validator.run()
