from multiprocessing import Pool
import numpy as np
import configparser
import io
import fpzip
import glob
from functools import partial


class Validator:

    # set global class values
    def __init__ (
            self,
            validation_function = None,
            reduction_function = None,
            experiments = {}
            ):

        if validation_function == None:
            self.m_func = self.sse
        if reduction_function == None:
            self.m_reduce = self.reduce_sse

        self.m_experiments = experiments
        self.m_meta = {}

        self.m_state_dimension = 0
        self.init()

    # initialize validator
    def init ( self ):

        for exp in self.m_experiments:

            state_item = {}

            for state in self.m_experiments[exp]:
                meta_pattern = self.m_experiments[exp][state] + '/Meta*-worker*-serialized.fti'
                ckpt_pattern = self.m_experiments[exp][state] + '/Ckpt*-worker*-serialized.fti'
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

                        mode        = int(config["0"]["var1_compression_mode"])
                        type        = int(config["0"]["var1_compression_type"])
                        parameter   = int(config["0"]["var1_compression_parameter"])
                        offset      = int(config["0"]["var1_pos"])
                        size        = int(config["0"]["var1_size"])
                        count       = int(config["0"]["var1_count"])
                        meta_item[proc] = {
                            "ckpt_file" : ckpt_files[idx],
                            "mode"      : mode,
                            "type"      : type,
                            "parameter" : parameter,
                            "offset"    : base + offset,
                            "size"      : size,
                            "count"     : count
                        }
                        base += size + offset

                        self.m_state_dimension += count

                        proc += 1
                        procs_per_node += 1

                state_item[state] = meta_item

            self.m_meta[exp] = state_item


    def sse( self, data ):

        sigma = 0
        a1 = data['original']
        a2 = data['compressed']

        for i in range(len(a1)):
            sigma += ( a1[i] - a2[i] ) ** 2

        return sigma

    def reduce_sse ( self, parts, n ):
        sigma = 0
        for part in parts:
            sigma += part

        return np.sqrt(sigma/n)


    def compare_state( self, proc, id ):

        global m_experiments
        global m_meta

        print(f"comparing id: {id}, rank: {proc}")

        states = {}

        for state in self.m_meta[id]:
            data = []

            meta = self.m_meta[id][state][proc]
            ckpt_file = meta['ckpt_file']
            ckpt = open(ckpt_file, 'rb')

            n = meta['count']
            bs = 1024 * 1024
            nb = n // bs + ( 1 if n%bs != 0 else 0 )

            ckpt.seek(meta['offset'])

            for b in range(nb):
                bytes = ckpt.read(8)
                bs = int.from_bytes(bytes, byteorder='little')
                bytes = ckpt.read(bs)
                block = fpzip.decompress(bytes, order='C')[0,0,0]
                data = [*data, *block]

            ckpt.close()

            states[state] = data

        return self.m_func(states)




    def validate(self, id, procs):

        pool = Pool()

        results = pool.map(partial(self.compare_state, id=id), procs)

        return self.m_reduce( results, self.m_state_dimension )



if __name__ == "__main__":

    experiments = {
        1 :
            {
                'compressed'    : '/home/kellekai/STUDY/PhD/Japan/lab/melissa_readCkpt_python/data/1',
                'original'      : '/home/kellekai/STUDY/PhD/Japan/lab/melissa_readCkpt_python/data/200001'
            }
    }


    test = Validator(experiments=experiments)

    result = test.validate(1, range(47))

    print ( f"result from validation function (stddev) is : {result}")


#    init(experiments)
#
#    results = submit(1, range(47), sum_square_error)
#
#    n = 0
#    for p in self.m_meta[1]["original"]:
#        n += self.m_meta[1]["original"][p]['count']
#
#    sigma = 0
#    for result in results:
#        sigma += result
#
#    sigma = np.sqrt(sigma/n)
#
#    print(sigma)



