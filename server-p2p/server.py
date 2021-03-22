"""A server module"""
import os
import sys
import random
import time
import zmq
import control_messages_pb2 as cm
import numpy as np

# Configuration:
LAUNCHER_PING_INTERVAL = 8  # seconds
LAUNCHER_TIMEOUT = 60  # seconds

CYCLES = int(sys.argv[1])
PARTICLES = int(sys.argv[2])
# 3 assimilator type
RUNNER_TIMEOUT = int(sys.argv[4])
# 5 Server slowdown factor
LAUNCHER_NODE_NAME = sys.argv[6]

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
from melissa4py.fault_tolerance import Simulation, SimulationStatus

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
if not cm.StateId.__hash__:
    cm.StateId.__hash__ = lambda x : (x.t, x.id).__hash__()


SimulationStatus.TIMEOUT = 4  # see melissa_messages.h

from common import parse

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


def bind_socket(t, addr):
    socket = context.socket(t)
    socket.bind(addr)
    port = socket.getsockopt(zmq.LAST_ENDPOINT)
    port = port.decode().split(':')[-1]
    port = int(port)
    return socket, port


# Socket for job requests
addr = "tcp://127.0.0.1:4000"  # TODO: make ports changeable, maybe even select them automatically!
print('binding to', addr)
job_socket, port_job_socket = \
        bind_socket(zmq.REP, addr)

# Socket for general purpose requests
addr = "tcp://127.0.0.1:4001"
print('binding to', addr)
gp_socket, port_gp_socket = \
        bind_socket(zmq.REP, addr)
print('general purpose port:', port_gp_socket)


def can_do_update_step():
    """simplest case where we wait that all particles were propagated always"""
    return len(unscheduled_jobs) == 0


# Populate unscheduled jobs
for p in range(PARTICLES):
    jid = cm.StateId()
    jid.t = assimilation_cycle
    jid.id = p

    pid = cm.StateId()
    pid.t = 0
    pid.id = p

    unscheduled_jobs[jid] = pid

print('Server up now')

def send_message(socket, data):
    socket.send(data.SerializeToString())

def accept_weight(msg):
    """remove jobs from the running_jobs list where we receive the weights"""

    assert msg.WhichOneof('content') == 'weight'

    state_id = msg.weight.state_id

    del running_jobs[state_id]
    weight = msg.weight.weight

    # store result
    state_weights[state_id] = weight
    print("Received weight", weight, "for", state_id, ".",
          len(unscheduled_jobs), "unscheduled jobs left to do this cycle")

    gp_socket.send(0)  # send an empty ack. Check this works like this.


    # Update knowledge on cached states:
    runner_id = msg.runner_id
    state_cache[runner_id].append(state_id)
    state_cache_with_prefetch[runner_id].append(state_id)

"""
DNS list of runners

Fromat:
    {runner_id: {head_rank_0: ('frog1', 8080)}, ...}, ...}

"""
runners = {}
def accept_runner_request(msg):
    # store request
    runner_id = msg.runner_id
    head_rank = msg.runner_request.head_rank
    if not runner_id in runners:
        runners[runner_id] = {}
    runners[runner_id][head_rank] = msg.runner_request.socket

    # remove all faulty runners
    for rid in faulty_runners:
        del runners[rid]

    # generate reply:
    reply = cm.Message()
    shuffeled_runners = random.shuffle(list(runners))
    for rid in shuffeled_runners:
        if rid == runner_id:
            continue
        if head_rank in runners[rid]:
            s = reply.runner_response.sockets.add()
            s.CopyFrom(runners[rid][head_rank])


    send_message(gp_socket, reply)

def count_runners_with_state(state_id):
    # Number of runners that have the state in their memory (or are about to get it)
    return len(filter(lambda x: state_id in state_cache[x], state_cache))

def accept_delete(msg):
    runner_id = msg.runner_id

    update_state_knowledge(msg.delete_request, runner_id)

    # Attach importance to states on runner and sort:
    sorted_importance = sorted(
            zip(map(calculate_parent_state_importance, state_cache[runner_id]),
                state_cache[runner_id]) )

    reply = cm.Message()
    reply.delete_response
    # Try to delete something with importance < 1 --> must be stored on other resource too.
    if sorted_importance[0][0] < 1:
        reply.delete_response.to_delete = sorted_importance[0][1]
    else:
        # if minimum >= 1: select something that possibly is stored on a different
        # runner too.
        for _, state_id in sorted_importance:
            runners_with_it = count_runners_with_state
            if runners_with_it > 1:
                reply.delete_response.to_delete = sorted_importance[0][1]
                break

    if reply.delete_response.to_delete:
        state_cache[runner_id].remove(reply.delete_response.to_delete)
    else:
        print("nothing good was found to be deleted on", runner_id)
        reply.delete_response.to_delete = sorted_importance[0][1]
        print("reply:", reply)

    send_message(gp_socket, reply)


def calculate_parent_state_importance(parent_state_id):
    # Calculate the importance of a parent state id

    # Number of unscheduled tasks dependent on it
    d = len(filter(lambda x: unscheduled_jobs[x] == parent_state_id, unscheduled_jobs))

    return d / count_runners_with_state(parent_state_id)


def calculate_runner_importance(runner_id):
    # Cumulate the importance of all states stored on the runner
    return np.sum(map(calculate_parent_state_importance, state_cache_with_prefetch[runner_id]))

def calculate_mean_importance():
    # Get the mean importance over all runners
    return np.mean(map(calculate_runner_importance, state_cache_with_prefetch))

def update_state_knowledge(msg, runner_id):
    # Update knowledge about state caches of runners:
    state_cache[runner_id] = msg.cached_states
    state_cache_with_prefetch[runner_id] = msg.cached_states  # fixme: probably we need a copy here!

def accept_prefetch(msg):
    runner_id = msg.runner_id
    update_state_knowledge(msg.prefetch_request, runner_id)

    mean_importance = calculate_mean_importance()


    # Figure out if this compute resource is a receiver or a sender:
    runner_importance = calculate_runner_importance(runner_id)

    reply = cm.Messages
    if runner_importance >= mean_importance:
        # This is a sender. it shall not prefetch anything:
        reply.prefetch_resonse
    else:
        # This runner shall receive (prefetch) the most important state:

        # Get parent state ids of unscheduled jobs
        parent_state_ids = map(lambda x: x[1], unscheduled_jobs)
        # Attach importance to it, sort ascending and get last element
        most_important = sorted(zip(map(calculate_parent_state_importance, parent_state_ids),
            parent_state_ids))[-1]

        # Reply state id of most important parent state (and not its importance)
        reply.prefetch_resonse.pull_states.append(most_important[1])

        state_cache[runner_id].append(most_important[1])

    send_message(gp_socket, reply)



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


def hanlde_job_requests(launcher):
    """take a job from unscheduled jobs and send it back to the runner. take one that is
    maybe already cached."""

    msg = receive_message_nonblocking(job_socket)

    if msg:
        assert msg.WhichOneof('content') == 'job_request'

        runner_id = msg.runner_id

        launcher.notify_runner_connect(msg.job_request.runner_id)

        the_job = random.choice(list(unscheduled_jobs))

        # try to select a better job where the runner has the cache already and which is
        # only on few other runners

        if runner_id in state_cache:
            parent_state_ids = map(lambda x: unscheduled_jobs[x], unscheduled_jobs)

            # Get cached parent state ids
            useful_states = filter(lambda x: x in parent_state_ids, state_cache[runner_id])

            if len(useful_states) > 0:
                # Select tha state_id that is on fewest other runners
                # TODO: replace sorted zip by arg sort or something
                parent_state_id = sorted(zip(map(count_runners_with_state, useful_states),
                    useful_states))[0][1]

                # find again job for parent_state_id
                the_job = filter(lambda x: unscheduled_jobs[x] == parent_state_id,
                        unscheduled_jobs)[0]


        reply = cm.Message()

        reply.job_response.job = the_job

        reply.job_response.parent = unscheduled_jobs[the_job]

        send_message(job_socket, reply)

        running_job = (time.time() + RUNNER_TIMEOUT, msg.job_request.runner_id,
                       unscheduled_jobs[the_job])
        print("Scheduling", running_job)
        running_jobs[the_job] = running_job
        del unscheduled_jobs[the_job]


class LauncherConnection:
    def __init__(self, context, node_name, launcher_node_name):
        self.update_launcher_due_date()
        self.linger = 10000
        self.launcher_node_name = launcher_node_name
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
        msg = ServerNodeName(0, node_name)
        self.text_pusher.send(msg.encode())
        self.update_next_message_due_date()
        self.connection_request = None
        print('Setup launcher connection, server node name:', node_name)

        self.known_runners = set()

    def update_next_message_due_date(self):
        self.next_message_date_to_launcher = time.time(
        ) + LAUNCHER_PING_INTERVAL

    def __del__(self):
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

    this_cycle = filter(lambda x: x[0] == assimilation_cycle,
                        state_weights)

    best_10 = sorted(this_cycle, key=lambda x: state_weights[x])[-10:]

    assimilation_cycle += 1

    # add each 4 times to unscheduled_jobs
    job_id = 0
    for parent_state_id in best_10:
        for _ in range(4):
            jid = cm.StateId()
            jid.t = assimilation_cycle
            jid.id = job_id
            unscheduled_jobs[jid] = parent_state_id  # TODO: maybe we need a copy?
            job_id += 1

    return assimilation_cycle < CYCLES


def check_due_date_violations():
    """ Check if runner has problems to finish a task and notifies launcher to kill it in this case"""
    for job_id in running_jobs:
        due_date, runner_id, parent_state_id = running_jobs[job_id]
        if time.time() > due_date:
            faulty_runners.add(runner_id)
            launcher.notify(runner_id, SimulationStatus.TIMEOUT)

            del running_jobs[job_id]
            unscheduled_jobs[job_id] = parent_state_id


if __name__ == '__main__':
    node_name = get_node_name()
    launcher = LauncherConnection(context, node_name, LAUNCHER_NODE_NAME)

    while True:
        # maybe for testing purpose call launcehr loop here (but only the part that does no comm  with the server...
        handle_general_purpose()
        if can_do_update_step():
            if not do_update_step():  # will populate unscheduled jobs
                # end: tell launcher to kill everything
                print('End!')
                break
            # TODO: FTI_push_to_deeper_level(unscheduled_jobs)

            # not necessary since we only answer job requests if job is there... answer_open_job_requests()

        if len(unscheduled_jobs) > 0:
            hanlde_job_requests(launcher)

        if not launcher.receive_text():
            if not launcher.check_launcher_due_date():
                raise Exception("Launcher did not ping me for too long!")

        launcher.ping()

        check_due_date_violations()

        # Slow down CPU:
        time.sleep(0.01)
