"""A server module"""
import os
import sys
import random
import time
import zmq
import p2p_pb2 as cm
import numpy as np
from enum import Enum


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
from melissa4py.fault_tolerance import Simulation  #, SimulationStatus  we redefine simulation Status here to be able to send timeout notifications!

sys.path.append('%s/launcher' % os.getenv('MELISSA_DA_SOURCE_PATH'))
from utils import get_node_name

context = zmq.Context()

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
    new state id == job id: parent_state_id

Example:
    {StateId: StateId, (2,60): (1,43)}
"""
unscheduled_jobs = {}

"""
Jobs currently running on some runner.

Data structure constis of triples:
    new state id == job id: due date, runner_id, parent_state_id

Example:
    StateId: (100001347, runner_id, StateId)
"""
running_jobs = {}


"""
Contains all runners and which states they have prefetched

Format:
    {runner_id: [state_id,...]}
"""
state_cache = {}
state_cache_with_prefetch = {}


def first(x):
    """Helper to return the first element of something"""
    return x[0]

def dict_append(d, where, what):
    if where not in d:
        d[where] = []
    d[where].append(what)

def bind_socket(t, addr):
    socket = context.socket(t)
    socket.bind(addr)
    port = socket.getsockopt(zmq.LAST_ENDPOINT)
    port = port.decode().split(':')[-1]
    port = int(port)
    return socket, port


# Socket for job requests
addr = "tcp://*:4000"  # TODO: make ports changeable, maybe even select them automatically!
print('binding to', addr)
job_socket, port_job_socket = \
        bind_socket(zmq.REP, addr)

# Socket for general purpose requests
addr = "tcp://*:4001"
print('binding to', addr)
gp_socket, port_gp_socket = \
        bind_socket(zmq.REP, addr)
print('general purpose port:', port_gp_socket)

weights_this_cycle = 0


# hold those varibales updated to avoid abusive use of list, filter, map, len and sorted
"""
Amount of runners that hold a specific state request.
Warning: in case of a lots of job/prefetch failure this list may be outdated and needs to be recalculated from
state_cache ! This is not yet implemented
""" # FIXME: see comment^^
runners_with_it = {}


"""
Amount of jobs that depend on a specific parent_state_id in this assimilation_cycle
"""
dependent_jobs = {}


# Populate unscheduled jobs
for p in range(PARTICLES):
    job_id = cm.StateId()
    job_id.t = assimilation_cycle
    job_id.id = p

    parent_id = cm.StateId()
    parent_id.t = 0
    parent_id.id = p

    dependent_jobs[parent_id] = 1
    runners_with_it[parent_id] = -1

    unscheduled_jobs[job_id] = parent_id

print('Server up now')

def send_message(socket, data):
    socket.send(data.SerializeToString())

def maybe_update():
    """
    simplest case where we wait that all particles were propagated always
    called always after a weight arrived.

    """
    global weights_this_cycle
    if weights_this_cycle == PARTICLES and len(unscheduled_jobs) == 0:
        # will populate unscheduled jobs
        trigger(START_FILTER_UPDATE, assimilation_cycle)
        weights_this_cycle = 0
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


    good_state = state_id in running_jobs
    if good_state:
        global weights_this_cycle
        weights_this_cycle += 1
        runners_with_it[state_id] = 1
        parent_state_id = running_jobs[state_id][2]
        dependent_jobs[parent_state_id] -= 1

        del running_jobs[state_id]
    else:
        # we mess around with the state id for the first iteration (for init)
        print('Got Weight message', msg, 'but its job was never set to running so far. This may be normal for the initial cycle.')
        assert state_id.t == 1 or state_id.t == 0  # save initial weights without removing any jobs from unscheduled jobs
        messed_id = cm.StateId()
        messed_id.t = state_id.t
        messed_id.id = msg.runner_id

    weight = msg.weight.weight

    # store result
    state_weights[state_id] = weight
    print("Received weight", weight, "for", state_id, ".",
          len(unscheduled_jobs), "unscheduled jobs left to do this cycle")

    gp_socket.send(b'')  # send an empty ack.

    # Update knowledge on cached states:

    dict_append(state_cache, runner_id, state_id)
    dict_append(state_cache_with_prefetch, runner_id, state_id)

    trigger(STOP_ACCEPT_WEIGHT, 0)

    if good_state:
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

    # remove all faulty runners
    for rid in faulty_runners:
        if rid in runners:
            del runners[rid]


    # generate reply:
    reply = cm.Message()
    reply.runner_response.SetInParent()
    # filter for runners that have the state in question:
    shuffeled_runners = list(filter(lambda x: msg.runner_request.searched_state_id in state_cache[x], runners))
    # shuffle inplace
    random.shuffle(shuffeled_runners)
    for rid in shuffeled_runners:
        if rid == runner_id:
            continue
        if head_rank in runners[rid]:
            s = reply.runner_response.sockets.add()
            s.CopyFrom(runners[rid][head_rank])


    print('Runner request to get', msg.runner_request.searched_state_id,'::', reply)
    send_message(gp_socket, reply)
    # print('scache was:', state_cache) REM: if this is the first runner request it ispossible that the requested state is on another runner but since there was not yet this runners runner req on the server, nothing is returned
    trigger(STOP_ACCEPT_RUNNER_REQUEST, 0)

def accept_delete(msg):
    trigger(START_ACCEPT_DELETE, 0)
    # TODO Try to not delete new iterations states if there are old iteratioins states with 0 importance

    # FIXME: first delete old stuff without any importance calculation!

    runner_id = msg.runner_id

    update_state_knowledge(msg.delete_request, runner_id)


    # Filter out parent and job state_id that is running on the same runner:
    jobs_on_runner = list(filter(lambda jid: running_jobs[jid][1] == runner_id, running_jobs))
    running_state_ids = []
    if len(jobs_on_runner) > 0:
        jid = jobs_on_runner[0]
        # running job's job id and parent state id
        running_state_ids = [jid, running_jobs[jid][2]]

    states_to_delete_from = filter(lambda x: x not in running_state_ids, state_cache[runner_id])

    if assimilation_cycle == 1:  # parent state id's will have t=0
        # don't delete the state ids from time step 0 for now as they are needed as parents
        states_to_delete_from = filter(lambda x: x.t != 0, states_to_delete_from)

    states_to_delete_from = list(states_to_delete_from)
    assert len(states_to_delete_from) > 0

    # Attach importance to states on runner and sort:

    sorted_importance = sorted(
            zip(map(calculate_parent_state_importance, states_to_delete_from),
                states_to_delete_from),
            key=first)  # only sort by first element (weight) (and not by the second which would be the state id)



    reply = cm.Message()
    reply.delete_response.SetInParent()
    found_to_delete = False
    # Try to delete something with importance < 1 --> must be stored on other resource too.
    if sorted_importance[0][0] < 1.0:
        reply.delete_response.to_delete.CopyFrom(sorted_importance[0][1])
        found_to_delete = True
    else:
        # if minimum >= 1: select something that possibly is stored on a different
        # runner too.
        for _, state_id in sorted_importance:
            r = runners_with_it[state_id]
            if r > 1:
                reply.delete_response.to_delete.CopyFrom(state_id)
                found_to_delete = True
                break

    if not found_to_delete:
        print("nothing good was found to be deleted on", runner_id)
        reply.delete_response.to_delete.CopyFrom(sorted_importance[0][1])
        print("still deleting something")


    print("Deleting", reply.delete_response.to_delete, "on runner", runner_id)
    state_cache[runner_id].remove(reply.delete_response.to_delete)


    send_message(gp_socket, reply)
    runners_with_it[reply.delete_response.to_delete] -= 1

    trigger(STOP_ACCEPT_DELETE, 0)


def calculate_parent_state_importance(parent_state_id):
    trigger(START_CALC_PAR_STATE_IMPORTANCE, 0)
    # Calculate the importance of a parent state id

    r = runners_with_it[parent_state_id]
    if r == 0:
        r = 0.5  # give some motivation to load states from the pfs and avoid div/0
    if parent_state_id in dependent_jobs:
        d = dependent_jobs[parent_state_id]
    else:
        d = 0
    trigger(STOP_CALC_PAR_STATE_IMPORTANCE, 0)
    return d / r


def calculate_runner_importance(runner_id):
    # Cumulate the importance of all states stored on the runner
    return np.sum(list(map(calculate_parent_state_importance, state_cache_with_prefetch[runner_id])))

def calculate_mean_importance():
    # Get the mean importance over all runners
    return np.mean(list(map(calculate_runner_importance, state_cache_with_prefetch)))

def update_state_knowledge(msg, runner_id):
    # Update knowledge about state caches of runners:
    state_cache[runner_id] = msg.cached_states
    state_cache_with_prefetch[runner_id] = msg.cached_states  # fixme: probably we need a copy here!

def accept_prefetch(msg):
    trigger(START_ACCEPT_PREFETCH, 0)
    #
    # TODO [sebastian]
    # receive prefetch capacity (currently maximum 5 states)
    # and number of free slots. determine if it is necessary
    # to exchange cached states by new states that are more
    # important.
    #
    # proto message is:
    #
    #   message PrefetchRequest {
    #       repeated StateId cached_states = 1;
    #       uint32 capacity = 2;  // unused on server side so far.
    #       uint32 free = 3;  // unused on server side so far.
    #   }
    #
    runner_id = msg.runner_id
    update_state_knowledge(msg.prefetch_request, runner_id)

    mean_importance = calculate_mean_importance()



    # Figure out if this compute resource is a receiver or a sender:
    runner_importance = calculate_runner_importance(runner_id)


    print('Prefetch Request - mean_importance:', mean_importance, 'runner(%d) importance:' % runner_id, runner_importance)

    reply = cm.Message()
    reply.prefetch_response.SetInParent()
    if runner_importance >= mean_importance or \
            assimilation_cycle == 1 or \
            len(unscheduled_jobs) == 0:
            # this is a sender
            # or its  assimilation cycle 1 where everybody has a good parent state already (we never delete states with t=0 during assimliation cycle 2)
            # or there is nothing left to be scheduled so far...
        pass
    else:
        # This runner shall receive (prefetch) the most important state:

        # Get parent state ids of unscheduled jobs
        parent_state_ids = list(map(lambda x: unscheduled_jobs[x], unscheduled_jobs))

        # Filter out states that are already on the runner:
        parent_state_ids = list(filter(lambda state_id: not state_id in state_cache[runner_id], parent_state_ids))

        if len(parent_state_ids) > 0:
            most_important = argmax(map(calculate_parent_state_importance, parent_state_ids),
                parent_state_ids)

            # Reply state id of most important parent state (and not its importance)
            reply.prefetch_response.pull_states.append(most_important)


            # TODO: if prefetching with weight 1: take a state that is only on one runner for fault tollerance?

            dict_append(state_cache_with_prefetch, runner_id, most_important)
            runners_with_it[most_important] += 1

    send_message(gp_socket, reply)
    trigger(STOP_ACCEPT_PREFETCH, 0)



def receive_message_nonblocking(socket):
    msg = None
    try:
        msg = socket.recv(flags=zmq.NOBLOCK)  # only polling
        msg = parse(msg)

        if msg.runner_id in faulty_runners:
            print("Ignoring faulty runner's message:", msg)
            gp_socket.reply(0)
            return
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

def argmin(values, args):
    m = None
    argi = -1
    for i, v in enumerate(values):
        if not m or v < m:
            m = v
            argi = i
    if argi != -1:
        return args[argi]

def argmax(values, args):
    m = None
    argi = -1
    for i, v in enumerate(values):
        if not m or v > m:
            m = v
            argi = i
    if argi != -1:
        return args[argi]


def handle_job_requests(launcher, nsteps):
    """take a job from unscheduled jobs and send it back to the runner. take one that is
    maybe already cached."""

    trigger(START_HANDLE_JOB_REQ, 0)

    msg = receive_message_nonblocking(job_socket)

    if msg:
        assert msg.WhichOneof('content') == 'job_request'

        runner_id = msg.runner_id
        trigger(STOP_IDLE_RUNNER, runner_id)

        launcher.notify_runner_connect(runner_id)

        the_job = random.choice(list(unscheduled_jobs))

        # try to select a better job where the runner has the cache already and which is
        # only on few other runners


        # at cycle == 1 just send a random job since we don't send any prefetch and delete requests where t=0 ;)
        if assimilation_cycle != 1 and runner_id in state_cache:
            parent_state_ids = map(lambda x: unscheduled_jobs[x], unscheduled_jobs)


            # Get cached parent state ids
            useful_states = list(filter(lambda x: x in parent_state_ids, state_cache[runner_id]))

            if len(useful_states) > 0:
                # Select the state_id that is on fewest other runners
                parent_state_id = argmin(map(lambda x: runners_with_it[x], useful_states),  # todo use numpy argmin?
                    useful_states)

                # find again job for parent_state_id
                the_job = next(filter(lambda x: unscheduled_jobs[x] == parent_state_id,
                        unscheduled_jobs))


        reply = cm.Message()

        reply.job_response.job.CopyFrom(the_job)

        reply.job_response.parent.CopyFrom(unscheduled_jobs[the_job])

        reply.job_response.nsteps = nsteps
        send_message(job_socket, reply)
        trigger(START_PROPAGATE_STATE, runner_id)

        running_job = (time.time() + RUNNER_TIMEOUT, msg.runner_id,
                       unscheduled_jobs[the_job])
        print("Scheduling", the_job, "with job entry", running_job)
        running_jobs[the_job] = running_job
        del unscheduled_jobs[the_job]
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
    global assimilation_cycle

    print("======= Performing update step after cycle %d ========" % assimilation_cycle)

    this_cycle = list(filter(lambda x: x.t == assimilation_cycle,
                        state_weights))

    sum_weights = np.sum(list(map(lambda x: state_weights[x], this_cycle)))

    # normalize state weights:
    state_weights_normalized = list(map(lambda x: state_weights[x] / sum_weights, this_cycle))

    print("Weights this cycle:", this_cycle)
    out_particles = np.random.choice(this_cycle, size=len(this_cycle), p=state_weights_normalized)
    #print("Particles:", out_particles)

    assimilation_cycle += 1
    job_id = 0
    for op in out_particles:
        parent_state_id = op
        if parent_state_id not in dependent_jobs:
            dependent_jobs[parent_state_id] = 1
        else:
            dependent_jobs[parent_state_id] += 1

        jid = cm.StateId()
        jid.t = assimilation_cycle
        jid.id = job_id
        unscheduled_jobs[jid] = parent_state_id
        job_id += 1
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


def check_due_date_violations():
    """ Check if runner has problems to finish a task and notifies launcher to kill it in this case"""
    for job_id in list(running_jobs):
        due_date, runner_id, parent_state_id = running_jobs[job_id]
        if time.time() > due_date:
            faulty_runners.add(runner_id)
            launcher.notify(runner_id, SimulationStatus.TIMEOUT)

            del running_jobs[job_id]
            unscheduled_jobs[job_id] = parent_state_id


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

        if len(unscheduled_jobs) > 0:
            handle_job_requests(launcher, nsteps)

        if not launcher.receive_text():
            if not launcher.check_launcher_due_date():
                raise Exception("Launcher did not ping me for too long!")

        launcher.ping()

        check_due_date_violations()

        # Slow down CPU:
        time.sleep(0.0001)

        maybe_write()
