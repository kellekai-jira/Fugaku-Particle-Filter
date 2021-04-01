import os
import sys
import signal
import subprocess
import random
import shutil
import logging
import threading
from importlib import reload  # to restart the logging ;)

from ctypes import cdll, create_string_buffer, c_char_p, c_wchar_p, c_int, c_double, POINTER


comm4py_path = os.getenv('MELISSA_DA_COMM4PY_PATH')
assert(comm4py_path)

c_int_p = POINTER(c_int)
c_double_p = POINTER(c_double)
melissa_comm4py = cdll.LoadLibrary(comm4py_path)

melissa_comm4py.send_message.argtypes = [c_char_p]
melissa_comm4py.send_job.argtypes = [c_int, c_char_p, c_int, c_double_p]
melissa_comm4py.send_drop.argtypes = [c_int, c_char_p]
melissa_comm4py.send_options.argtypes = [c_char_p]
melissa_comm4py.bind_message_rcv.argtypes = [c_char_p]
melissa_comm4py.bind_message_resp.argtypes = [c_char_p]
melissa_comm4py.bind_message_snd.argtypes = [c_char_p]
melissa_comm4py.send_resp_message.argtypes = [c_char_p]
#melissa_comm4py.wait_message.argtypes = [c_char_p]
#melissa_comm4py.poll_message.argtypes = [c_char_p]

# Enums:

MSG_SERVER_NODE_NAME    = 0
MSG_TIMEOUT             = 1
MSG_REGISTERED          = 2
MSG_PING                = 3
MSG_STOP                = 4

# Assimilator types:
ASSIMILATOR_DUMMY = 0
ASSIMILATOR_PDAF = 1
ASSIMILATOR_EMPTY = 2
ASSIMILATOR_CHECK_STATELESS = 3
ASSIMILATOR_PRINT_INDEX_MAP = 4
ASSIMILATOR_WRF = 5
ASSIMILATOR_PYTHON = 6

LOG_LEVEL_DEBUG = 3
LOG_LEVEL_LOG   = 2
LOG_LEVEL_ERROR = 1


def logger_function(loglevel):

    def l(*args, **kwargs):
        print(*args, **kwargs)
        nonlocal loglevel
        f = [0, logging.error, logging.info, logging.debug][loglevel]
        f(*args, **kwargs)

    return l


def defer(f):
    t = threading.Thread(daemon=True, target=f)
    t.start()
    return t

debug = logger_function(LOG_LEVEL_DEBUG)
log   = logger_function(LOG_LEVEL_LOG)
error = logger_function(LOG_LEVEL_ERROR)

def start_logging(workdir):
    logging.shutdown()
    reload(logging)
    logfile = workdir+'/melissa_launcher.log'
    logging.basicConfig(format='%(asctime)s %(message)s',
                            datefmt='%m/%d/%Y %I:%M:%S %p',
                            filename=logfile,
                            level=logging.DEBUG)

    log("Start logging into %s" % logfile)

def get_node_name():
    buf = create_string_buffer(256)
    melissa_comm4py.get_node_name(buf)
    return buf.value.decode()

def init_sockets():
    melissa_comm4py.init_context()
    melissa_comm4py.bind_message_rcv(str(5555).encode())
    melissa_comm4py.bind_message_resp(str(5554).encode())
    melissa_comm4py.bind_message_snd(str(5556).encode())

def finalize_sockets():
    melissa_comm4py.close_message()

def get_server_messages():
    msgs = []
        # TODO?: last_msg_to_server = 0
    buf = create_string_buffer(melissa_comm4py.melissa_get_message_len())
    assert len(buf) == melissa_comm4py.melissa_get_message_len()

    melissa_comm4py.poll_message(buf, len(buf))
    message = buf.value.decode().split()
    while message[0] != 'null':
        if message[0] != 'nothing':
            msg = {}
            debug('message: '+buf.value.decode())
            if message[0] == 'stop':
                msg['type'] = MSG_STOP
            elif message[0] == 'group_state':
                state = int(message[2])
                if state == 1:  # RUNNING in melissa_messages.h  TODO: unify with the enums used here!
                    msg['type'] = MSG_REGISTERED
                    debug('-> 1 = registered')
                elif state == 4:  # TIMEOUT in melissa_messages.h  TODO: unify with the enums used here!
                    msg['type'] = MSG_TIMEOUT
                    debug('-> 4 = timeout')
                else:
                    raise ValueError("Unexpected state %d", state)

                msg['runner_id'] = int(message[1])
            elif message[0] == 'server':
                msg['type'] = MSG_SERVER_NODE_NAME
                rank = int(message[1])
                assert rank == 0  # in Melissa-DA only rank 0 connects.
                msg['node_name'] = message[2]

            elif message[0] == 'alive':
                # should receive all 50 seconds
                msg['type'] = MSG_PING
            else:
                raise ValueError("Unexpected message type '%s'" % message[0])


            msgs.append(msg)

        melissa_comm4py.poll_message(buf, len(buf))
        message = buf.value.decode().split()

    return msgs

def clean_old_stats():
    print("Cleaning old results...")
    if os.path.isdir("STATS"):
        shutil.rmtree("STATS")
    else:
        print("Nothing to clean!")


def join_dicts(out, b):
    for k, v in b.items():
        #assert k not in out
        out[k] = v
    return out

# For test cases TODO: move somewhere else!:
def killing_giraffe(name):
    p = subprocess.Popen(['ps', '-x'], stdout=subprocess.PIPE)
    out, _ = p.communicate()
    pids = []
    for line in out.splitlines():
        if ' %s' % name in line.decode() and not 'mpi' in name:
            pids.append(int(line.split(None, 1)[0]))
    assert len(pids) > 0
    os.kill(random.choice(pids), signal.SIGKILL)



