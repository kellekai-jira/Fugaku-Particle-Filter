"""A server module"""
import os
import sys
import random
import time
import zmq
import control_messages_pb2 as cm

# Configuration:
LAUNCHER_PING_INTERVAL = 8  # seconds
LAUNCHER_TIMEOUT = 60  # seconds

CYCLES = int(sys.argv[1])
PARTICLES = int(sys.argv[2])
# 3 assimilator type
RUNNER_TIMEOUT = int(sys.argv[4])
# 5 Server slowdown factor
LAUNCHER_NODE_NAME = sys.argv[6]






print('Melissa Server started with %d particles for %d cycles' % (PARTICLES , CYCLES))
print("RUNNER_TIMEOUT:", RUNNER_TIMEOUT)

# TODO: dirty! install properly
sys.path.append('%s/melissa/utility/melissa4py' % os.getenv('MELISSA_DA_SOURCE_PATH'))
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
SimulationStatus.TIMEOUT = 4  # see melissa_messages.h


from common import parse


assimilation_cycle = 1

"""List of runners that are considered faulty. If they send data it is ignored."""
faulty_runners = set()

"""
State ids in general

consist of 2 parts: the assimilation cycle and the state id it self.
"""


"""
Weights of states.

Example:
    {(1, 41): 1023, (1,40): 512}
"""
state_weights = {}


"""
Unscheduled jobs. They represent particles that will be needed to propagate for this
iteration.

Data structure consits of pairs:
    new state id == job id: parent_state_id

Example:
    {(2,50): (1,42), (2,60): (1,43)}
"""
unscheduled_jobs = {}

"""
Jobs currently running on some runner.

Data structure constis of triples:
    new state id == job id: due date, runner_id, parent_state_id

Example:
    (2,55): (100001347, runner, (1,43))
"""
running_jobs = {}

def bind_socket(t, addr):
    socket = context.socket(t)
    socket.bind(addr)
    port = socket.getsockopt(zmq.LAST_ENDPOINT)
    port = port.decode().split(':')[-1]
    port = int(port)
    return socket, port

# Socket for job requests and launcher requests
addr = "tcp://127.0.0.1:6666"  # TODO: make ports changeable, maybe even select them automatically!
print('binding to', addr)
job_rep_socket, port_job_rep_socket = \
        bind_socket(zmq.REP, addr)

# Socket accepting pulled weights
addr = "tcp://127.0.0.1:6667"
print('binding to', addr)
weight_socket, port_weight_socket = \
        bind_socket(zmq.PULL, addr)
print('weight port:', port_weight_socket)

def can_do_update_step():
    """simplest case where we wait that all particles were propagated always"""
    return len(unscheduled_jobs) == 0


# Populate unscheduled jobs
for p in range(PARTICLES):
    unscheduled_jobs[(assimilation_cycle, p)] = (0, p)

print('Server up now')

def accept_weights():
    """remove jobs from the running_jobs list where we receive the weights"""
    msg = None
    try:
        msg = weight_socket.recv(flags=zmq.NOBLOCK)  # only polling
    except zmq.error.Again:
        # could not poll anything
        pass

    if msg:
        msg = parse(msg)

        assert msg.WhichOneof('content') == 'weight'

        if msg.weight.runner_id in faulty_runners:
            print("Ignoring faulty runners weight message:", msg.weight)
            return

        state_id = (msg.weight.state_id.t, msg.weight.state_id.id)

        del running_jobs[state_id]
        weight = msg.weight.weight

        # store result
        state_weights[state_id] = weight
        print("Received weight", weight, "for", state_id, ".",
            len(unscheduled_jobs), "unscheduled jobs left to do this cycle")

def accept_job_requests(launcher):
    """take a job from unscheduled jobs and send it back to the runner. take one that is
    maybe already cached."""

    msg = None
    try:
        msg = job_rep_socket.recv(flags=zmq.NOBLOCK)  # only polling
    except zmq.error.Again:
        # could not poll anything
        pass

    if msg:
        msg = parse(msg)
        assert msg.WhichOneof('content') == 'job_request'


        if msg.job_request.runner_id in faulty_runners:
            print("Ignoring faulty runners weight message:", msg.weight)
            return

        launcher.notify_runner_connect(msg.job_request.runner_id)

        the_job = random.choice(list(unscheduled_jobs.keys()))

        # try to select a better job where the runner has the cache already
        if len(msg.job_request.cached_states) > 0:
            for cs in msg.job_request.cached_states:
                cj = (cs.t, cs.id)
                if cj in unscheduled_jobs:
                    the_job = cj
                    break


        reply = cm.Message()

        reply.job_response.job.t = the_job[0]
        reply.job_response.job.id = the_job[1]

        reply.job_response.parent.t = unscheduled_jobs[the_job][0]
        reply.job_response.parent.id = unscheduled_jobs[the_job][1]

        job_rep_socket.send(reply.SerializeToString())


        running_job = (time.time()+RUNNER_TIMEOUT,
                msg.job_request.runner_id,
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
            self.launcher_node_name, self.text_pull_port
        )
        self.text_puller.connect(self.text_puller_port_name)

        # Server (PUSH) -> Launcher (PULL)
        self.text_pusher = context.socket(zmq.PUSH)
        self.text_pusher.setsockopt(zmq.LINGER, self.linger)
        addr = "tcp://{}:{}".format(
            self.launcher_node_name, self.text_push_port
        )
        self.text_pusher.connect(addr)
        # Server (REQ) <-> Launcher (REP)
        self.text_requester = context.socket(zmq.REQ)
        self.text_requester.setsockopt(zmq.LINGER, self.linger)
        self.text_requester.connect("tcp://{}:{}".format(
            self.launcher_node_name, self.text_request_port)
        )

        # Send node name to the launcher, get options and recover if necesary
        msg = ServerNodeName(0, node_name)
        self.text_pusher.send(msg.encode())
        self.update_next_message_due_date()
        self.connection_request = None
        print('Setup launcher connection, server node name:', node_name)

        self.known_runners = set()

    def update_next_message_due_date(self):
        self.next_message_date_to_launcher = time.time() + LAUNCHER_PING_INTERVAL

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
            print("Launcher message recieved %s" % msg);
            self.update_launcher_due_date()
            return True
            # ATM We do not care what the launcher sends us. We only check if it is still alive

    def update_launcher_next_message_date(self):
        self.next_message_date_to_launcher = time.time() + LAUNCHER_PING_INTERVAL

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
            self.notify(runner_id, SimulationStatus.RUNNING)  # notify that running
            self.known_runners.add(runner_id)

def do_update_step():
    """Does actual update step with resampling. Is required to fill the unscheduled_jobs
    and returns false if this was the last cycle"""
    # Something really stupid for now:
    # Sort by weights. Then take 10 best particles for next generation
    global assimilation_cycle

    this_cycle = filter(lambda x: x[0] == assimilation_cycle, state_weights.keys())

    best_10 = sorted(this_cycle, key=lambda x: state_weights[x])[-10:]

    assimilation_cycle += 1

    # add each 4 times to unscheduled_jobs
    job_id = 0
    for it in best_10:
        for _ in range(4):
            unscheduled_jobs[(assimilation_cycle, job_id)] = it  # TODO: maybe we need a copy?
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
        accept_weights()
        if can_do_update_step():
            if not do_update_step():  # will populate unscheduled jobs
                # end: tell launcher to kill everything
                print('End!')
                break
            # TODO: FTI_push_to_deeper_level(unscheduled_jobs.keys())

            # not necessary since we only answer job requests if job is there... answer_open_job_requests()

        if len(unscheduled_jobs) > 0:
            accept_job_requests(launcher)

        if not launcher.receive_text():
            if not launcher.check_launcher_due_date():
                raise Exception("Launcher did not ping me for too long!")

        launcher.ping()

        check_due_date_violations()

        # Slow down CPU:
        time.sleep(0.01)
