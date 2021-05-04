"""A server module"""
import os
import sys
import random
import time
import zmq
import p2p_pb2 as cm
import numpy as np
import math
from enum import Enum

from collections import OrderedDict


# Configuration:
LAUNCHER_PING_INTERVAL = 8  # seconds
LAUNCHER_TIMEOUT = 60  # seconds

CYCLES = int(sys.argv[1])
PARTICLES = int(sys.argv[2])
# 3 assimilator type
RUNNER_TIMEOUT = int(sys.argv[4])
# 5 Server slowdown factor
LAUNCHER_NODE_NAME = sys.argv[6]

stealable_jobs = PARTICLES

# define stepsize of assimilation window
# warmup stepsize is set below...
if os.getenv('MELISSA_DA_NSTEPS'):
    NSTEPS = int(os.getenv('MELISSA_DA_NSTEPS'))
else:
    NSTEPS = 1

print('Melissa Server started with %d particles for %d cycles' %
      (PARTICLES, CYCLES))
print("RUNNER_TIMEOUT:", RUNNER_TIMEOUT)

# TODO: dirty! install properly
sys.path.append('%s/melissa/utility/melissa4py' %
                os.getenv('MELISSA_DA_SOURCE_PATH'))
import ctypes
#from melissa4py import message
from melissa4py.message import MessageType
from melissa4py.message import ServerNodeName
from melissa4py.message import ConnectionRequest, ConnectionResponse
from melissa4py.message import SimulationData
from melissa4py.message import JobDetails
from melissa4py.message import Stop
from melissa4py.message import SimulationStatusMessage
from melissa4py.fault_tolerance import Simulation  #, SimulationStatus  we redefine simulation Status here to be able to end timeout notifications!

sys.path.append('%s/launcher' % os.getenv('MELISSA_DA_SOURCE_PATH'))
from utils import get_node_name

context = zmq.Context()
context.setsockopt(zmq.LINGER, 0)

# Tuning melissa4py adding messages needed in Melissa-DA context
class Alive:
    def encode(self):
        return bytes(ctypes.c_int32(7))  # Alive is 7, see melissa_messages.h

# patch state id so we can use it as dict index:
# TODO Better would be: Generate another class around it that has the state_id as member !
# (might break if Protobuf will define their own hash function)
# if not cm.StateId.__hash__:
# very dirty to not have the but latest protobuf on juwels defines this function with
# an error handler...
# https://groups.google.com/g/protobuf/c/0p0EMmEWiKQ?pli=1
cm.StateId.__hash__ = lambda x : (x.t, x.id).__hash__()

class SimulationStatus(Enum):
    CONNECTED = 0
    RUNNING = 1
    FINISHED = 2
    TIMEOUT = 4


from common import *

assimilation_cycle = 1
"""List of runner ids that are considered faulty. If they send data it is ignored."""
faulty_runners = set()
"""
State ids in general

consist of 2 parts: the assimilation cycle and the state id it self.
"""

"""
Weights of states.

Example:
    {StateId: 1023, StateId: 512}
"""
state_weights = {}

class DueDates:
    """
    Due dates

    {due date (int in s): set((unner_id 1, job_id, parent_id), (runner_id2..., job_id, parent_id)}
    """
    due_dates = OrderedDict()
    last_check = 0

    @staticmethod
    def add(runner_id, job_id, parent_id):
        due_time = int(time.time()) + RUNNER_TIMEOUT
        dict_append(DueDates.due_dates, due_time, (runner_id, job_id, parent_id))
        print("adding due date at", due_time, runner_id, job_id, parent_id)

    @staticmethod
    def remove(runner_id, job_id):
        """Trys to find and remove associated due dates"""
        found = False
        for dd in list(DueDates.due_dates):
            for i, elem in enumerate(DueDates.due_dates[dd]):
                if elem[0] == runner_id and elem[1] == job_id:
                    found = True
                    del DueDates.due_dates[dd][i]
                    if len(DueDates.due_dates[dd]) == 0:
                        del DueDates.due_dates[dd]
                    parent_id = elem[2]
                    break

        print("removing due date", runner_id, job_id, parent_id)
        assert found

        return parent_id

    @staticmethod
    def get_by_runner(runner_id):
        res = []
        for dd in DueDates.due_dates:
            for i, elem in enumerate(DueDates.due_dates[dd]):
                if elem[0] == runner_id:
                    res.append(elem[2])

        return res

    @staticmethod
    def check_violations():
        """ Check if runner has problems to finish a task and notifies launcher to kill it in this case"""
        global stealable_jobs

        now = int(time.time())
        if DueDates.last_check == now:
            return

        DueDates.last_check = now
        for dd in list(DueDates.due_dates):  # FIXME: check that ordered! but probably not necessary!
            if dd > now:
                print('checking dd > now:', dd, '>', now)
                break
            else:
                print("now", now, "dd", dd, 'runners that crash:', DueDates.due_dates[dd])
                for rid, jid, pid in DueDates.due_dates[dd]:
                    if rid not in faulty_runners:
                        launcher.notify(rid, SimulationStatus.TIMEOUT)
                        faulty_runners.add(rid)
                        del runners_last[rid]

                        print("Due date passed for runner id %d" % rid)

                    if rid in scheduled_jobs:
                        del scheduled_jobs[rid]
                    else:
                        stealable_jobs += 1

                    if pid not in alpha:
                        alpha[pid] = 0
                    alpha[pid] += 1

                del DueDates.due_dates[dd]

"""
Contains all runners and which states they have prefetched

Format (by state):
    {state_id: list(runner_id,...)}
"""
class StateCache:
    c = {} # FIXME: might be faster with [] as inner container!
    cr = {}

    @staticmethod
    def add_runner(state_id, runner_id):
        dict_append(StateCache.c, state_id, runner_id)
        dict_append(StateCache.cr, runner_id, state_id)


    @staticmethod
    def remove_runner(state_id, runner_id):
        dict_remove(StateCache.c, state_id, runner_id)
        dict_remove(StateCache.cr, runner_id, state_id)

    @staticmethod
    def get_runners(state_id):
        if state_id in StateCache.c:
            return [x for x in StateCache.c[state_id] if x not in faulty_runners]
        else:
            return []

    @staticmethod
    def get_by_runner(runner_id):
        if runner_id in StateCache.cr:
            return StateCache.cr[runner_id]
        else:
            return []

    @staticmethod
    def update(msg, runner_id):
        # Update knowledge about state caches of runners:
        # This is the operation probably quite often and it is freaking slow!
        for s in StateCache.cr[runner_id]:
            dict_remove(StateCache.c, s, runner_id)
        StateCache.cr[runner_id] = msg.cached_states
        for state_id in msg.cached_states:
            dict_append(StateCache.c, state_id, runner_id)

def dict_append(d, where, what):
    if where not in d:
        d[where] = []
    d[where].append(what)

def dict_remove(d, where, what):
    d[where].remove(what)
    if len(d[where]) == 0:
        del d[where]

# Socket for job requests
addr = "tcp://*:4000"  # TODO: make ports changeable, maybe even select them automatically!
print('binding to', addr)
job_socket, port_job_socket = \
        bind_socket(context, zmq.REP, addr)

# Socket for general purpose requests
gp_socket = None
addr = "tcp://*:4001"
print('binding to', addr)
gp_socket, port_gp_socket = \
        bind_socket(context, zmq.REP, addr)
print('general purpose port:', port_gp_socket)

weights_this_cycle = 0

print('Server up now')

def send_message(socket, data):
    socket.send(data.SerializeToString())

def maybe_update():
    """
    Checks if we can update and if so calls do_update_step routine.
    simplest case where we wait that all particles were propagated always
    called always after a weight arrived.

    """
    global weights_this_cycle, stealable_jobs
    print(f"Can we do an update? len(unscheduled_jobs)={len(alpha)}, stealable_jobs={stealable_jobs})")
    if weights_this_cycle == PARTICLES and len(alpha) == 0 and stealable_jobs == 0:
        # will populate unscheduled jobs
        trigger(START_FILTER_UPDATE, assimilation_cycle)
        old_assimilation_cycle = assimilation_cycle
        nsteps = do_update_step()
        trigger(STOP_FILTER_UPDATE, old_assimilation_cycle)
        trigger(STOP_ITERATION, old_assimilation_cycle)
        if nsteps <= 0:
            # end: tell launcher to kill everything
            global launcher
            del launcher
            time.sleep(1)  # normally not necessary due to linger?!
            print('Gracefully ending server now.')
            exit(0)
        trigger(START_ITERATION, assimilation_cycle)
        # TODO: FTI_push_to_deeper_level(unscheduled_jobs)

        # not necessary since we only answer job requests if job is there... answer_open_job_requests()


def accept_weight(msg):
    """remove jobs from the running_jobs list where we receive the weights"""
    trigger(START_ACCEPT_WEIGHT, 0)


    assert msg.WhichOneof('content') == 'weight'

    state_id = msg.weight.state_id

    runner_id = msg.runner_id
    trigger(STOP_PROPAGATE_STATE, runner_id)
    trigger(START_IDLE_RUNNER, runner_id)

    weight = msg.weight.weight
    if state_id.t == 0:
        print("got starting weight. Ignoring it")
    else:
        print("Received weight", weight, "for", state_id, ".")
        DueDates.remove(runner_id, state_id)  # this fails if we get a weight that was never a due date added for!

        # store result
        global weights_this_cycle
        weights_this_cycle += 1
        state_weights[state_id] = weight

    gp_socket.send(b'')  # send an empty ack.

    # Update knowledge on cached states:
    StateCache.add_runner(state_id, runner_id)

    trigger(STOP_ACCEPT_WEIGHT, 0)

    maybe_update()


"""
DNS list of runners

Fromat:
    {runner_id: {head_rank_0: ('frog1', 8080)}, ...}, ...}

"""
runners = {}
def accept_runner_request(msg):
    trigger(START_ACCEPT_RUNNER_REQUEST, 0)
    # store request
    runner_id = msg.runner_id
    launcher.notify_runner_connect(runner_id)
    head_rank = msg.runner_request.head_rank
    if not runner_id in runners:
        runners[runner_id] = {}
    runners[runner_id][head_rank] = msg.runner_request.socket

    # generate reply:
    reply = cm.Message()
    reply.runner_response.SetInParent()

    state_id = msg.runner_request.searched_state_id
    shuffeled_runners = StateCache.get_runners(state_id)
    if shuffeled_runners:
        # shuffle inplace
        random.shuffle(shuffeled_runners)

        # filter for runners that have the state in question:
        for rid in shuffeled_runners:
            if rid == runner_id:
                print("Warning: Why is this runner asking me if it holds this state already??")
                continue
            # can be that we do know its in this runners state but we dont know its adress!
            if rid in runners and head_rank in runners[rid]:
                s = reply.runner_response.sockets.add()
                s.CopyFrom(runners[rid][head_rank])

    # if len(reply.runner_response.sockets) == 0:
        # print("Could not find any runner with this state in", StateCache.c)

    send_message(gp_socket, reply)
    # print('scache was:', ) REM: if this is the first runner request it ispossible that the requested state is on another runner but since there was not yet this runners runner req on the server, nothing is returned
    trigger(STOP_ACCEPT_RUNNER_REQUEST, 0)

def accept_delete(msg):
    """
    Deletes
    1. old stuff
    2. a state no further jobs depend on
    3. a job from the next assimilation cycle with lowest weight
    4. a random job
    """
    trigger(START_ACCEPT_DELETE, 0)

    runner_id = msg.runner_id

    StateCache.update(msg.delete_request, runner_id)

    delete_from = msg.delete_request.cached_states

    # don't delete what is scheduled
    # Don't delete working stuff (stuff that is about to be written + stuff that is about to be calculated.... see running_jobs
    blacklist = DueDates.get_by_runner(runner_id)
    if runner_id in scheduled_jobs:
        blacklist.append(scheduled_jobs[runner_id][1])
    if assimilation_cycle == 1:
        # never delete the base state in the first iteration:
        # print('blacklist before:', blacklist)
        blacklist = blacklist + [ s for s in delete_from if s.t == 0]
        # print('blacklist after:', blacklist)
    delete_from = [s for s in delete_from if s not in blacklist]

    too_old = []    # delete state that is too old --> O(#state cache)
    not_important = []  # delete state that is not important for any job this iteration anymore --> O(#state cache * #unscheduled jobs) , hashmaps might help?
    too_new = []  # delete state that is too new --> O(#state cache)

    for s in delete_from:
        if s.t < assimilation_cycle - 1:
            too_old.append(s)
            break

        elif s.t == assimilation_cycle - 1 and s not in alpha:
            not_important.append(s)

        elif s.t == assimilation_cycle:
            too_new.append(s)


    all_lists = too_old + not_important


    if len(all_lists) == 0:
        if len(too_new) != 0:
            # delete new state with lowest weight
            to_delete = too_new[0]
            lowest_weight = state_weights[to_delete]
            for tn in too_new[1:]:
                w = state_weights[tn]
                if w < lowest_weight:
                    to_delete = tn
                    lowest_weight = w

        else:
            # delete something randomly
            # to_delete = np.random.choice(delete_from)
            to_delete = delete_from[0]
    else:
        to_delete = all_lists[0]  # take first possible...

    reply = cm.Message()
    reply.delete_response.to_delete.CopyFrom(to_delete)

    print("Deleting", reply.delete_response.to_delete, "on runner", runner_id)
    StateCache.remove_runner(to_delete, runner_id)


    send_message(gp_socket, reply)

    trigger(STOP_ACCEPT_DELETE, 0)

def accept_prefetch(msg):
    trigger(START_ACCEPT_PREFETCH, 0)

    runner_id = msg.runner_id
    StateCache.update(msg.prefetch_request, runner_id)
    reply = cm.Message()
    reply.prefetch_response.SetInParent()

    if assimilation_cycle > 1:  # never prefetch in iteration 1!
        if runner_id not in scheduled_jobs:
            parent_id = select_good_new_parent(runner_id)
            if parent_id:
                job_id = generate_job_id()
                scheduled_jobs[runner_id] = (job_id, parent_id)

                alpha[parent_id] -= 1
                if alpha[parent_id] == 0:
                    del alpha[parent_id]

                DueDates.add(runner_id, job_id, parent_id)

        if runner_id in scheduled_jobs:
            if runner_id in StateCache.get_runners(scheduled_jobs[runner_id][1]):
                # This runner does not need to prefetch anything
                pass
            else:
                # This runner should prefetch this parent state
                reply.prefetch_response.pull_states.append(scheduled_jobs[runner_id][1])

    send_message(gp_socket, reply)
    trigger(STOP_ACCEPT_PREFETCH, 0)

def receive_message_nonblocking(socket):
    msg = None
    try:
        msg = socket.recv(flags=zmq.NOBLOCK)  # only polling
        msg = parse(msg)

        if msg.runner_id in faulty_runners:
            print("Ignoring faulty runner's message:", msg)
            socket.send(b'')

            return None
    except zmq.error.Again:
        # could not poll anything
        pass

    return msg

def handle_general_purpose():
    msg = receive_message_nonblocking(gp_socket)

    if msg:
        ty = msg.WhichOneof('content')
        if ty == 'weight':
            accept_weight(msg)
        elif ty == 'delete_request':
            accept_delete(msg)
        elif ty == 'prefetch_request':
            accept_prefetch(msg)
        elif ty == 'runner_request':
            accept_runner_request(msg)
        else:
            print("Wrong message type received!")
            assert False

def runners_per_parent(parent_id):
    # FIXME this will take way too long!
    res = set()
    # check with due dates:
    for dd in DueDates.due_dates:
        for elem in DueDates.due_dates[dd]:
            if elem[2] == parent_id:
                res.add(elem[0])


    print("res", len(res))
    return len(res)

runners_last = {}
alpha = {}
scheduled_jobs = {}


def select_good_new_parent(runner_id):
    # global assimilation_cycle
    if assimilation_cycle == 1:
        parent_id = cm.StateId()
        parent_id.t = 0
        parent_id.id = runner_id
        if parent_id not in alpha:
            alpha[parent_id] = 0
        alpha[parent_id] += 1
        return parent_id
    else:
        R = len(runners_last)
        P = len(alpha)
        Q = P / R
        # FIXME: start counting at offset!
        for parent_id in alpha:
            if math.ceil(alpha[parent_id] / Q) - runners_per_parent(parent_id) >= 1:
                return parent_id

next_job_id = 0
def generate_job_id():
    global next_job_id
    job_id = cm.StateId()
    job_id.t = assimilation_cycle
    job_id.id = next_job_id
    next_job_id += 1
    return job_id

def handle_job_requests(launcher, nsteps):
    """take a job from unscheduled jobs and send it back to the runner. take one that is
    maybe already cached."""
    global stealable_jobs

    msg = receive_message_nonblocking(job_socket)

    if msg:
        trigger(START_HANDLE_JOB_REQ, 0)
        assert msg.WhichOneof('content') == 'job_request'

        runner_id = msg.runner_id

        launcher.notify_runner_connect(runner_id)

        # try to select a better job where the runner has the cache already and which is
        # only on few other runners

        reply = cm.Message()
        reply.job_response.SetInParent()

        remove_from_alpha = True

        if runner_id in scheduled_jobs:
            job_id, parent_id = scheduled_jobs[runner_id]
            del scheduled_jobs[runner_id]
            # in this case we remove the duedate for the scheduled state:
            DueDates.remove(runner_id, job_id)
            remove_from_alpha = False
        elif runners_last[runner_id] in alpha:
            parent_id = runners_last[runner_id]
        else:
            parent_id = select_good_new_parent(runner_id)

        if parent_id:
            # bookkeeping
            if remove_from_alpha:
                alpha[parent_id] -= 1
                if alpha[parent_id] == 0:
                    del alpha[parent_id]
            runners_last[runner_id] = parent_id

            stealable_jobs -= 1

            job_id = generate_job_id()

            reply.job_response.job.CopyFrom(job_id)
            reply.job_response.parent.CopyFrom(parent_id)
            print("Scheduling", (job_id, parent_id))
            trigger(STOP_IDLE_RUNNER, runner_id)
            trigger(START_PROPAGATE_STATE, runner_id)
            DueDates.add(runner_id, job_id, parent_id)

        reply.job_response.nsteps = nsteps
        send_message(job_socket, reply)
        trigger(STOP_HANDLE_JOB_REQ, 0)


class LauncherConnection:
    def __init__(self, context, server_node_name, launcher_node_name):
        self.update_launcher_due_date()
        self.linger = 10000
        self.launcher_node_name = launcher_node_name

        if server_node_name == self.launcher_node_name:
            self.launcher_node_name = '127.0.0.1'

        self.text_pull_port = 5556
        self.text_push_port = 5555
        self.text_request_port = 5554

        # Launcher (PUB) -> Server (SUB)
        self.text_puller = context.socket(zmq.SUB)
        self.text_puller.setsockopt(zmq.SUBSCRIBE, b"")
        self.text_puller.setsockopt(zmq.LINGER, self.linger)
        self.text_puller_port_name = "tcp://{}:{}".format(
            self.launcher_node_name, self.text_pull_port)
        self.text_puller.connect(self.text_puller_port_name)

        # Server (PUSH) -> Launcher (PULL)
        self.text_pusher = context.socket(zmq.PUSH)
        self.text_pusher.setsockopt(zmq.LINGER, self.linger)
        addr = "tcp://{}:{}".format(self.launcher_node_name,
                                    self.text_push_port)
        self.text_pusher.connect(addr)
        # Server (REQ) <-> Launcher (REP)
        self.text_requester = context.socket(zmq.REQ)
        self.text_requester.setsockopt(zmq.LINGER, self.linger)
        self.text_requester.connect("tcp://{}:{}".format(
            self.launcher_node_name, self.text_request_port))

        # Send node name to the launcher, get options and recover if necesary
        msg = ServerNodeName(0, server_node_name)
        self.text_pusher.send(msg.encode())
        self.update_next_message_due_date()
        self.connection_request = None
        print('Setup launcher connection, server node name:', server_node_name)

        self.known_runners = set()

    def update_next_message_due_date(self):
        self.next_message_date_to_launcher = time.time(
        ) + LAUNCHER_PING_INTERVAL

    def __del__(self):
        print("Sending Stop Message to Launcher")
        self.text_pusher.send(Stop().encode())

    def update_launcher_due_date(self):
        self.due_date_launcher = time.time() + LAUNCHER_TIMEOUT

    def check_launcher_due_date(self):
        return time.time() < self.due_date_launcher

    def receive_text(self):
        msg = None
        try:
            msg = self.text_puller.recv(flags=zmq.NOBLOCK)
        except zmq.error.Again:
            # could not poll anything
            return False
        if msg:
            print("Launcher message recieved %s" % msg)
            self.update_launcher_due_date()
            return True
            # ATM We do not care what the launcher sends us. We only check if it is still alive

    def update_launcher_next_message_date(self):
        self.next_message_date_to_launcher = time.time(
        ) + LAUNCHER_PING_INTERVAL

    def ping(self):
        if time.time() > self.next_message_date_to_launcher:
            msg = Alive()
            print('send alive')
            self.text_pusher.send(msg.encode())
            self.update_launcher_next_message_date()

    def notify(self, runner_id, status):
        msg = SimulationStatusMessage(runner_id, status)
        print("notify launcher about runner", runner_id, ":", status)
        self.text_pusher.send(msg.encode())

    def notify_runner_connect(self, runner_id):
        if not runner_id in self.known_runners:
            self.notify(runner_id,
                        SimulationStatus.RUNNING)  # notify that running
            self.known_runners.add(runner_id)
            print("Server registering Runner ID %d" % runner_id)
            runners_last[runner_id] = None




def do_update_step():
    global assimilation_cycle, weights_this_cycle, next_job_id, alpha, stealable_jobs
    print("======= Performing update step after cycle %d ========" % assimilation_cycle)

    this_cycle = [sw for sw in state_weights if sw.t == assimilation_cycle]

    # normalize state weights and resample
    sum_weights = np.sum([state_weights[x] for x in this_cycle])
    state_weights_normalized = [state_weights[x] / sum_weights for x in this_cycle]
    print('state_weights', state_weights_normalized)
    out_particles = np.random.choice(this_cycle, size=len(this_cycle), p=state_weights_normalized)

    assert stealable_jobs == 0
    stealable_jobs = PARTICLES

    assimilation_cycle += 1
    next_job_id = 0
    weights_this_cycle = 0

    for op in out_particles:
        parent_id = op
        if op not in alpha:
            alpha[op] = 0
        alpha[op] += 1

    print(f"we got P={len(alpha)} different particles!")

    return NSTEPS if assimilation_cycle < CYCLES else 0



if __name__ == '__main__':
    server_node_name = get_node_name()
    global launcher
    launcher = LauncherConnection(context, server_node_name, LAUNCHER_NODE_NAME)

    trigger(START_ITERATION, assimilation_cycle)
    # Timesteps for warmup:
    if os.getenv("MELISSA_DA_WARMUP_NSTEPS"):
        nsteps = int(os.getenv("MELISSA_DA_WARMUP_NSTEPS"))
    elif os.getenv("MELISSA_DA_NSTEPS"):
        nsteps = int(os.getenv("MELISSA_DA_NSTEPS"))
    else:
        nsteps = 1



    server_loops_last_second = 0
    last_second = 0
    while True:
        server_loops_last_second += 1
        if int(time.time()) > last_second:
            last_second = int(time.time()) + 4
            print('server loops last 5 second: %d' % server_loops_last_second)
            server_loops_last_second = 0

        # maybe for testing purpose call launcehr loop here (but only the part that does no comm  with the server...
        handle_general_purpose()

        # REM: maybe_update is called only after a weight arrived in handle_general_purpose()

        if stealable_jobs > 0:
            handle_job_requests(launcher, nsteps)

        if not launcher.receive_text():
            if not launcher.check_launcher_due_date():
                raise Exception("Launcher did not ping me for too long!")

        launcher.ping()

        DueDates.check_violations()

        # Slow down CPU:
        time.sleep(0.000001)

        maybe_write()
