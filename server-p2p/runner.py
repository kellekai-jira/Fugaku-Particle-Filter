"""A runner module"""
import os
import time
import random
import sys
import zmq
from common import parse
import messages.py.control_messages_pb2 as cm

sys.path.append('%s/launcher' % os.getenv('MELISSA_DA_SOURCE_PATH'))
from utils import get_node_name

RUNNER_ID = int(os.getenv("MELISSA_DA_RUNNER_ID"))

print("Starting runner with id %d" % RUNNER_ID)


# 2 user functions:
def integrate(x):
    #time.sleep(random.randint(1,50)/100)  # simulate some calculation
    time.sleep(0.001)
    new_x = map(lambda x: x + 42, x)
    return new_x


def get_weight(x):
    return random.random()


context = zmq.Context()

print(os.getenv("MELISSA_SERVER_MASTER_NODE"))
MELISSA_SERVER_MASTER_NODE = os.getenv("MELISSA_SERVER_MASTER_NODE")[6:].split(
    ':')[0]
if MELISSA_SERVER_MASTER_NODE == get_node_name():
    MELISSA_SERVER_MASTER_NODE = "127.0.0.1"

job_req_socket = context.socket(zmq.REQ)
addr = "tcp://%s:%d" % (MELISSA_SERVER_MASTER_NODE, 6666)
print('connect to melissa server at', addr)
job_req_socket.connect(addr)

weight_push_socket = context.socket(zmq.PUSH)
addr = "tcp://%s:%d" % (MELISSA_SERVER_MASTER_NODE, 6667)
print('connect weight puhser to', addr)
weight_push_socket.connect(addr)

# Get all open state server sockets belonging to this runner on rank 0:
state_server_sockets = [("dummy%d" % RUNNER_ID, 1234),
                        ("dummy%d" % RUNNER_ID, 1235)]

# a state id is always a tuple:
# (assimilation_cycle, id),
# job id's too obviously as they are the state id of the resulting state
# state_cache = {
# (1, 42): [1,2,3,4],
# (1, 43): [4,4,4,4]
# }

state_cache = {}  # the state cache must become part of FTI later

# init all with a useful state cache (for now...):
for i in range(100):
    state_cache[(0, i)] = [1 + i, 2, 3, 4]


def get_job(state_cache):
    """Request a job from the server that is possibly already in the state cache"""
    #assert mpirank == 0
    msg = cm.Message()
    for t, iid in state_cache.keys():
        it = msg.job_request.cached_states.add()
        it.t = t
        it.id = iid
    msg.job_request.runner_id = RUNNER_ID
    for s in state_server_sockets:
        rank = msg.job_request.client.ranks.add()
        rank.node_name = s[0]
        rank.port = s[1]

    job_req_socket.send(msg.SerializeToString())
    print("Sent request to server")

    reply = job_req_socket.recv()
    reply = parse(reply)

    assert reply.WhichOneof('content') == 'job_response'

    print("Got response")
    print("dns update:", reply.job_response.clients)

    # let it raise an exceotion if it cannot fetch any job for now..
    return (reply.job_response.job.t, reply.job_response.job.id), \
        (reply.job_response.parent.t, reply.job_response.parent.id)
    # TODO: handle node list!


def fetch_state(state_id):
    # FIXME At the moment we assume no horizontal state transport
    #assert state_id in state_cache
    it = state_id
    if not it in state_cache:
        it = random.choice(list(state_cache.keys()))

    return state_cache[it]
    # if state_id in state_cache:
    # return state_cache[state_id]
    # else:
    # return FTI_fetch_state_extern()  # using FTI magic...
    # FTI heuristically loads states on this runners cache in the background. the list of this cache is transmitted to the server each time jobs are requested.


def send_weight_to_server(state_id, weight):
    """pushes weight to server. can probably use the same channel as get_job"""
    msg = cm.Message()
    msg.weight.state_id.t = state_id[0]
    msg.weight.state_id.id = state_id[1]
    msg.weight.weight = weight
    data = msg.SerializeToString()
    print('want to push weight')
    weight_push_socket.send(data)

    print("pushing weight", weight, "for", state_id, "to server")


if __name__ == '__main__':
    # runner algo:
    job_id, parent_state_id = get_job(state_cache)
    while True:
        print("propagating", parent_state_id, "->", job_id)
        x = fetch_state(parent_state_id)
        new_x = integrate(x)
        state_cache[job_id] = new_x
        #FTI_magic(job_id, new_x)
        w = get_weight(new_x)
        send_weight_to_server(job_id, w)
        job_id, parent_state_id = get_job(
            state_cache
        )  # job_id is the new job id the server will give me. we assume x contains also information on what time step we are on
