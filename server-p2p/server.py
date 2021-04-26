"""A server module"""
import os
import sys
import random
import time
import zmq
import p2p_pb2 as cm
import numpy as np
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

"""
Unscheduled jobs. They represent particles that will be needed to propagate for this
iteration.

Data structure consits of pairs:
    parent_state_id: [new state id == job id, ...]

Example:
    {StateId: StateId, (2,60): [(1,43)]}
"""
unscheduled_jobs = {}

"""
Jobs scheduled to a runner. This is important to react correctly to prefetch requests

Example:
    {runner_id: (job_id, parent_id)}
"""
scheduled_jobs = {}  #FIXME: shceduled job due date!

"""
Jobs currently running on some runner.

Data structure constis of triples:
    due data: [list of new state id == job id, runner_id, parent_state_id]

Example:
    {runner_id:  [(job_id, parent_id)]}
"""
running_jobs = {}


class DueDates:
    """
    Due dates

    {due date (int in s): set((unner_id 1, job_id), (runner_id2..., job_id)}
    """
    due_dates = OrderedDict()

    @staticmethod
    def add(runner_id, job_id):
        dict_set_add(DueDates.due_dates, int(time.time() + RUNNER_TIMEOUT), (runner_id, job_id))

    @staticmethod
    def remove(runner_id, job_id):
        """Trys to find and remove associated due dates"""
        to_find = (runner_id, job_id)
        print('Try to find ', to_find, 'in', DueDates.due_dates)
        found = False
        for dd in DueDates.due_dates:
            if to_find in DueDates.due_dates[dd]:
                found = True
                dict_set_remove(DueDates.due_dates, dd, to_find)
                break

        assert found

    @staticmethod
    def check_violations():
        """ Check if runner has problems to finish a task and notifies launcher to kill it in this case"""
        now = int(time.time())
        for dd in list(DueDates.due_dates):  # FIXME: check that ordered! but probably not necessary!
            if dd > now:
                break
            else:
                for rid, _ in DueDates.due_dates[dd]:
                    launcher.notify(rid, SimulationStatus.TIMEOUT)
                    faulty_runners.add(rid)

                    if rid in scheduled_jobs:
                        job_id, parent_id = scheduled_jobs[rid]
                        dict_append(unscheduled_jobs, parent_id, job_id)

                    if rid in running_jobs:
                        global stealable_jobs
                        for job_id, parent_id in running_jobs[rid]:
                            dict_append(unscheduled_jobs, parent_id, job_id)
                            stealable_jobs += 1
                del DueDates.due_dates[dd]

"""
Contains all runners and which states they have prefetched

Format (by state):
    {state_id: set(runner_id,...)}
"""
class StateCache:
    c = {}

    @staticmethod
    def add_runner(state_id, runner_id):
        dict_set_add(StateCache.c, state_id, runner_id)

    @staticmethod
    def remove_runner(state_id, runner_id):
        dict_set_remove(StateCache.c, state_id, runner_id)

    @staticmethod
    def get_runners(state_id):
        if state_id in StateCache.c:
            return list(StateCache.c[state_id])
        else:
            return None



    def update(msg, runner_id):
        # Update knowledge about state caches of runners:
        for state_id in msg.cached_states:
            StateCache.add_runner(state_id, runner_id)






def dict_append(d, where, what):
    if where not in d:
        d[where] = []
    d[where].append(what)

def dict_set_add(d, where, what):
    if where not in d:
        d[where] = set()
    d[where].add(what)

def dict_set_remove(d, where, what):
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
stealable_jobs = PARTICLES  # Those are jobs that are not yet running. only if such jobs exist we accept job requests.

def init_ens():
# Populate unscheduled jobs
    for p in range(PARTICLES):
        job_id = cm.StateId()
        job_id.t = assimilation_cycle
        job_id.id = p

        parent_id = cm.StateId()
        parent_id.t = 0
        parent_id.id = p

        dict_append(unscheduled_jobs, parent_id, job_id)
init_ens()

print('Server up now')

def send_message(socket, data):
    socket.send(data.SerializeToString())

def maybe_update():
    """
    Checks if we can update and if so calls do_update_step routine.
    simplest case where we wait that all particles were propagated always
    called always after a weight arrived.

    """
    global weights_this_cycle
    print(f"Can we do an update? weights_this_cycle={weights_this_cycle}/{PARTICLES}, len(unscheduled_jobs)={len(unscheduled_jobs)}, stealable_jobs={stealable_jobs})")
    if weights_this_cycle == PARTICLES and len(unscheduled_jobs) == 0 and stealable_jobs == 0:
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

    global weights_this_cycle

    assert msg.WhichOneof('content') == 'weight'

    state_id = msg.weight.state_id

    runner_id = msg.runner_id
    trigger(STOP_PROPAGATE_STATE, runner_id)
    trigger(START_IDLE_RUNNER, runner_id)

    weight = msg.weight.weight

    # remove from running_jobs
    if runner_id not in running_jobs:
        print('Got Weight message', msg, 'but its job was never set to running so far. This may be normal for the initial cycle.')
        assert state_id.t == 0
    else:
        running_job = [rj for rj in running_jobs[runner_id] if rj[0] == state_id]

        if len(running_job) > 0:
            # assert len(delete) == 1  is not matched if runner timed out!
            running_jobs[runner_id].remove(running_job[0])

            # store result
            weights_this_cycle += 1
            state_weights[state_id] = weight
            print("Received weight", weight, "for", state_id, ".")

            print("dd removal from accept weight")
            DueDates.remove(runner_id, state_id)

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
            assert rid != runner_id
            if head_rank in runners[rid]:
                s = reply.runner_response.sockets.add()
                s.CopyFrom(runners[rid][head_rank])

    if len(reply.runner_response.sockets) == 0:
        print("Could not find any runner with this state in", StateCache.c)

    print('Runner request to get', msg.runner_request.searched_state_id,'::', reply)
    send_message(gp_socket, reply)
    # print('scache was:', ) REM: if this is the first runner request it ispossible that the requested state is on another runner but since there was not yet this runners runner req on the server, nothing is returned
    trigger(STOP_ACCEPT_RUNNER_REQUEST, 0)

def accept_delete(msg):
    trigger(START_ACCEPT_DELETE, 0)

    runner_id = msg.runner_id

    StateCache.update(msg.delete_request, runner_id)

    delete_from = msg.delete_request.cached_states

    # don't delete what is scheduled
    # Don't delete working stuff (stuff that is about to be written + stuff that is about to be calculated.... see running_jobs
    blacklist = ([scheduled_jobs[runner_id][1]] if runner_id in scheduled_jobs else []) + [ rj[0] for rj in running_jobs[runner_id] ] + [ rj[1] for rj in running_jobs[runner_id] ]
    delete_from = [s for s in delete_from if s not in blacklist]

    too_old = []    # delete state that is too old --> O(#state cache)
    not_important = []  # delete state that is not important for any job this iteration anymore --> O(#state cache * #unscheduled jobs) , hashmaps might help?
    too_new = []  # delete state that is too new --> O(#state cache)

    for s in delete_from:
        if s.t < assimilation_cycle - 1:
            too_old.append(s)
            break

        elif s.t == assimilation_cycle - 1 and s not in unscheduled_jobs:
            not_important.append(s)

        elif s.t == assimilation_cycle:
            too_new.append(s)

    all_lists = too_old + not_important + too_new

    if len(all_lists) == 0:
        # delete something randomly
        to_delete = np.random.choice(delete_from)
    else:
        to_delete = all_lists[0]  # take first possible...

    reply = cm.Message()
    reply.delete_response.to_delete.CopyFrom(to_delete)

    print("Deleting", reply.delete_response.to_delete, "on runner", runner_id)
    StateCache.remove_runner(to_delete, runner_id)

    send_message(gp_socket, reply)

    trigger(STOP_ACCEPT_DELETE, 0)



def remove_unscheduled_job(job_id, parent_id):
    unscheduled_jobs[parent_id].remove(job_id)
    if len(unscheduled_jobs[parent_id]) == 0:
        del unscheduled_jobs[parent_id]


def select_new_job(runner_id):
    """
    Select a new job that would be nice on this runner
    """
    if len(unscheduled_jobs) == 0:
        return

    found = False
    for s in unscheduled_jobs:
        if s in StateCache.c and runner_id in StateCache.c[s]:
            parent_id = s
            found = True
            break

    if not found:
        parent_id = np.random.choice(list(unscheduled_jobs))

    job_id = np.random.choice(unscheduled_jobs[parent_id])

    remove_unscheduled_job(job_id, parent_id)
    return job_id, parent_id

def has_scheduled(runner_id, refresh_due_date=True):
    # Ensure that something is scheduled for this runner: (otherwise schedule something or return None)
    if runner_id in scheduled_jobs:
        if refresh_due_date:
            # find old due date of this job and set it to now. this path is reached when handle_jobrequest calls has_scheduled and there was something scheduled already
            print("dd removal from has_scheduled")
            DueDates.remove(runner_id, scheduled_jobs[runner_id][0])
    else:
        new_job = select_new_job(runner_id)
        if new_job:
            scheduled_jobs[runner_id] = new_job
        else:
            return None


    if refresh_due_date:
        DueDates.add(runner_id, scheduled_jobs[runner_id][0])

    return scheduled_jobs[runner_id]

def accept_prefetch(msg):
    trigger(START_ACCEPT_PREFETCH, 0)

    runner_id = msg.runner_id
    StateCache.update(msg.prefetch_request, runner_id)
    reply = cm.Message()
    reply.prefetch_response.SetInParent()

    if assimilation_cycle > 1:
        new_job = has_scheduled(runner_id)
        if new_job:  # sometimes has_scheduled cannot schedule anything as no jobs are left...
            if runner_id in StateCache.c[new_job[1]]:
                # This runner does not need to prefetch anything
                pass
            else:
                # This runner should prefetch this parent state
                reply.prefetch_response.pull_states.append(new_job[1])

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

def handle_job_requests(launcher, nsteps):
    """take a job from unscheduled jobs and send it back to the runner. take one that is
    maybe already cached."""


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

        global stealable_jobs

        # at cycle == 1 just send a random job since we don't send any prefetch and delete requests where t=0 ;)
        if assimilation_cycle == 1:
            parent_id = list(unscheduled_jobs.keys())[0]
            job_id = unscheduled_jobs[parent_id][0]
            reply.job_response.job.CopyFrom(job_id)
            reply.job_response.parent.CopyFrom(parent_id)
            remove_unscheduled_job(job_id, parent_id)
            dict_append(running_jobs, runner_id, (job_id, parent_id))
            stealable_jobs -= 1
            DueDates.add(runner_id, job_id)
        else:
            new_job = has_scheduled(runner_id)
            if new_job:
                del scheduled_jobs[runner_id]

                dict_append(running_jobs, runner_id, new_job)

                print("Scheduling", new_job)
                reply.job_response.job.CopyFrom(new_job[0])
                reply.job_response.parent.CopyFrom(new_job[1])
                stealable_jobs -= 1

                trigger(STOP_IDLE_RUNNER, runner_id)
                trigger(START_PROPAGATE_STATE, runner_id)
            else:
                pass # implement steal maybe

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




def do_update_step():
    """Does actual update step with resampling. Is required to fill the unscheduled_jobs
    and returns false if this was the last cycle"""
    # Something really stupid for now:
    # Sort by weights. Then take 10 best particles for next generation
    global assimilation_cycle, stealable_jobs, weights_this_cycle

    print("======= Performing update step after cycle %d ========" % assimilation_cycle)

    print('state_weights:', state_weights)

    this_cycle = [sw for sw in state_weights if sw.t == assimilation_cycle]

    sum_weights = np.sum([state_weights[x] for x in this_cycle])

    # normalize state weights:
    state_weights_normalized = [state_weights[x] / sum_weights for x in this_cycle]

    print("Weights this cycle:", this_cycle)
    out_particles = np.random.choice(this_cycle, size=len(this_cycle), p=state_weights_normalized)
    #print("Particles:", out_particles)

    assimilation_cycle += 1
    job_id = 0
    for op in out_particles:
        parent_state_id = op

        jid = cm.StateId()
        jid.t = assimilation_cycle
        jid.id = job_id
        dict_append(unscheduled_jobs, parent_state_id, jid)
        job_id += 1

    stealable_jobs = PARTICLES
    weights_this_cycle = 0
    print("new unscheduled jobs:", unscheduled_jobs)



    # residual resampling: Otherwise we could use random.choice with the p parameter(copied from dorits notebook)
    # N = len(weights)
    # indexes = np.zeros(N, 'i')
    # num_copies = (np.floor(N*np.asarray(weights))).astype(int)
    # k = 0
    # for i in range(N):
        # for _ in range(num_copies[i]):
            # indexes[k] = i
            # k += 1

    # residual = N*weights - num_copies
    # residual /= sum(residual)

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
            last_second = int(time.time())
            print('server_loops_last_second: %d' % server_loops_last_second)
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
        time.sleep(0.0001)

        maybe_write()
