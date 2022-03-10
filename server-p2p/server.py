"""A server module"""
import os
import sys
import random
import time
import zmq
import p2p_pb2 as cm
import numpy as np
import math
import pickle
import copy
from dict_helpers import dict_append, dict_remove
from StateCache import StateCache
import scheduling_policy
import glob
import re


from collections import OrderedDict

from LauncherConnection import LauncherConnection


validate_states = None
validation_sockets = {}
validator_ids = []

# Configuration:

CYCLES = int(sys.argv[1])
PARTICLES = int(sys.argv[2])
# 3 assimilator type
RUNNER_TIMEOUT = int(sys.argv[4])
# 5 Server slowdown factor
LAUNCHER_NODE_NAME = sys.argv[6]

IS_SPECULATIVE = False

# jobs that are not yet scheduled or running this cycle
stealable_jobs = PARTICLES

import builtins as __builtin__
print_start_time = time.time()
def print(*args, **kwargs):
    # Add timestampt to each print
    return __builtin__.print("[ %.3f" % (time.time() - print_start_time), "]", *args, **kwargs)

sys.path.append('%s/launcher' % os.getenv('MELISSA_DA_SOURCE_PATH'))
from utils import get_node_name

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
sys.path.append('%s/server-p2p/melissa4py' %
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

context = zmq.Context()
context.setsockopt(zmq.LINGER, 0)

def count_members(a):
    res = 0
    for _, val in a.items():
        res += val
    return res

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

from SimulationStatus import SimulationStatus


from common import *

# REM: particles produced in cycle n will have an as timestep of their job id relying on parentstates with t=n-1
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
Contains information which job id corresponds to which parent id

Example:
    {Job_id: parent_id}
"""
job_to_parent_map = {}

class DueDates:
    due_dates = OrderedDict()
    last_check = 0

    rpp = {}  # runners per parent

    all_per_runner = {}

    @staticmethod
    def add(runner_id, job_id, parent_id):
        due_time = int(time.time()) + RUNNER_TIMEOUT

        # storing the same info 3 times for quicker access
        dict_append(DueDates.due_dates, due_time, (runner_id, job_id, parent_id))
        dict_append(DueDates.rpp, parent_id, runner_id)
        dict_append(DueDates.all_per_runner, runner_id, (due_time, job_id, parent_id))

        # print("adding due date at", due_time, runner_id, job_id, parent_id)

    @staticmethod
    def remove(runner_id, job_id):
        """Trys to find and remove associated due dates"""
        found = False
        for i, (dd, jid, parent_id) in enumerate(DueDates.all_per_runner[runner_id]):
            if jid == job_id:
                found = True
                break
        # print("removing due date", runner_id, job_id, parent_id)
        assert found
        del DueDates.all_per_runner[runner_id][i]  # remove from all by runner

        found = False
        for i, elem in enumerate(DueDates.due_dates[dd]):
            if elem[0] == runner_id and elem[1] == job_id:
                del DueDates.due_dates[dd][i]  # remove from due_dates
                if len(DueDates.due_dates[dd]) == 0:
                    del DueDates.due_dates[dd]
                found = True
                break

        assert found

        dict_remove(DueDates.rpp, parent_id, runner_id) #remove from rpp

        return parent_id

    @staticmethod
    def get_by_runner(runner_id):
        # list first all job_id's and then all parent_id's this runner is concerned with
        res = [apr[1] for apr in DueDates.all_per_runner[runner_id]] + [apr[2] for apr in DueDates.all_per_runner[runner_id]]
        return res

    @staticmethod
    def check_violations():
        """ Check if runner has problems to finish a task and notifies launcher to kill it in this case"""
        global stealable_jobs, len_alpha, runners_last

        now = int(time.time())
        if DueDates.last_check == now:
            return

        DueDates.last_check = now
        for dd in list(DueDates.due_dates):  # FIXME: check that ordered! but probably not necessary!
            if dd > now:
                # print('checking dd > now:', dd, '>', now)
                break
            else:
                # print("now", now, "dd", dd, 'runners that crash:', DueDates.due_dates[dd])
                for rid, jid, pid in DueDates.due_dates[dd]:
                    if rid not in faulty_runners:
                        launcher.notify(rid, SimulationStatus.TIMEOUT)
                        faulty_runners.add(rid)
                        if rid in runners_last:
                            del runners_last[rid]

                        print("Due date passed for runner id %d" % rid)

                    if rid in scheduled_jobs:
                        del scheduled_jobs[rid]
                    # job was already scheduled or running.
                    # scheduled, running or finished jobs are not stealable. so now it is stealable again...
                    # REM: at the moment there is no *stealing* but the name is still kept
                    if jid.t == assimilation_cycle:  # don't remove jobs from alpha that were only speculative! (we calculate alpha newly all the time anyway...
                        stealable_jobs += 1
                        len_alpha += 1
                        # stealable jobs could also be calculated from all jobs that are in the alpha list ;)


                    dict_remove(DueDates.rpp, pid, rid)  # remove from rpp
                    dict_remove(DueDates.all_per_runner, rid, (dd, jid, pid))
                    print('Canceled was the job', jid, 'from parent', pid)

                del DueDates.due_dates[dd]  # remove from due dates

    @staticmethod
    def count_running_jobs(t):
        """Count jobs that are running..."""

    @staticmethod
    def runners_per_parent(parent_id):
        if parent_id in DueDates.rpp:
            return len(set(DueDates.rpp[parent_id]))
        else:
            return 0


# Socket for job requests
addr = "tcp://*:4000"  # TODO: make ports changeable, maybe even select them automatically!
print('binding to', addr)
job_socket, port_job_socket = \
        bind_socket(context, zmq.REP, addr)

# Socket for general purpose requests
addr = "tcp://*:4001"
print('binding to', addr)
gp_socket, port_gp_socket = \
        bind_socket(context, zmq.REP, addr)
print('general purpose port:', port_gp_socket)

print('Server up now')

def send_message(socket, data):
    socket.send(data.SerializeToString())

def inner_sum(a):
    s = 0
    for _, n in a.items():
        s += n
    return s


def maybe_update():
    """
    Checks if we can update and if so calls do_update_step routine.
    simplest case where we wait that all particles were propagated always
    called always after a weight arrived.

    """
    global alpha_weights, stealable_jobs
    print(f"Can we do an update? len(alpha_weights)={len(alpha_weights)}, sum={inner_sum(alpha_weights)}, {len_alpha}!=len(unscheduled_jobs)={len(alpha)}, sum={inner_sum(alpha)}, stealable_jobs={stealable_jobs})")
    if master_jobs_first_cycle == PARTICLES and len(alpha_weights) == 0 and len(alpha) == 0 and stealable_jobs == 0:
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
        trigger(START_ITERATION, len(alpha))
        # TODO: FTI_push_to_deeper_level(unscheduled_jobs)

        # not necessary since we only answer job requests if job is there... answer_open_job_requests()


def accept_weight(msg):
    """remove jobs from the running_jobs list where we receive the weights"""
    trigger(START_ACCEPT_WEIGHT, 0)


    assert msg.WhichOneof('content') == 'weight'

    state_id = msg.weight.state_id

    runner_id = msg.runner_id
    trigger(STOP_PROPAGATE_STATE, state_id.id)
    trigger(START_IDLE_RUNNER, runner_id)

    weight = msg.weight.weight
    if state_id.t == 0:
        print("got starting weight. Ignoring it")
    else:
        print("Received weight", weight, "for", state_id, ".")
        DueDates.remove(runner_id, state_id)  # this fails if we get a weight that was never a due date added for!

        # store result
        state_weights[state_id] = weight

        parent_id = job_to_parent_map[state_id]
        if parent_id in alpha_weights:
            alpha_weights[parent_id] -= 1
            if alpha_weights[parent_id] == 0:
                del alpha_weights[parent_id]

    gp_socket.send(b'')  # send an empty ack.

    # Update knowledge on cached states:
    StateCache.add(state_id, runner_id)

    trigger(STOP_ACCEPT_WEIGHT, 0)

    maybe_update()


"""
DNS list of runners

Fromat:
    {runner_id: {head_rank_0: ('frog1', 8080)}, ...}, ...}

"""
runners = {}
def accept_runner_request(msg):
    global runners_last
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
    shuffeled_runners = [x for x in StateCache.get_runners(state_id) if x not in faulty_runners]
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
    3. a job from the afternext assimilation cycle with lowest weight (that was started specultively)
    4. a job from the next assimilation cycle with lowest weight
    5. a random job that is  neither scheduled nor running
    """
    trigger(START_ACCEPT_DELETE, 0)

    runner_id = msg.runner_id

    StateCache.update(msg.delete_request, runner_id)

    delete_from = msg.delete_request.cached_states

    # Don't delete what is scheduled
    # Don't delete working stuff (stuff that is about to be written + stuff that is about to be calculated.... see running_jobs
    blacklist = DueDates.get_by_runner(runner_id)
    # No need to check the scheduled_jobs dict since they are stored as duedate too
    if assimilation_cycle == 1:
        # Never delete the base state in the first iteration:
        blacklist = blacklist + [ s for s in delete_from if s.t == 0 and s.id == runner_id]
    delete_from = [s for s in delete_from if s not in blacklist]

    to_delete = scheduling_policy.select_evict(delete_from, alpha, state_weights, assimilation_cycle)

    reply = cm.Message()
    reply.delete_response.to_delete.CopyFrom(to_delete)

    print("Deleting", reply.delete_response.to_delete, "on runner", runner_id)
    StateCache.remove(to_delete, runner_id)


    send_message(gp_socket, reply)

    trigger(STOP_ACCEPT_DELETE, 0)

def accept_prefetch(msg):
    trigger(START_ACCEPT_PREFETCH, 0)

    global stealable_jobs

    runner_id = msg.runner_id
    StateCache.update(msg.prefetch_request, runner_id)
    reply = cm.Message()
    reply.prefetch_response.SetInParent()

    if assimilation_cycle > 1:  # never prefetch in iteration 1!
        if runner_id not in scheduled_jobs:

            parent_id, cachehit = select_parent(runner_id, alpha, StateCache, len(runners_last))

            if parent_id:
                trigger_select(runner_id, parent_id, cachehit)
                job_id = generate_job_id(parent_id.t + 1)
                job_to_parent_map[job_id] = parent_id
                scheduled_jobs[runner_id] = (job_id, parent_id)

                if parent_id.t == assimilation_cycle-1:  # filter out speculative particles
                    alpha[parent_id] -= 1
                    if alpha[parent_id] == 0:
                        del alpha[parent_id]

                if parent_id.t == assimilation_cycle-1:
                    stealable_jobs -= 1

                DueDates.add(runner_id, job_id, parent_id)

        if runner_id in scheduled_jobs:
            possible_prefetch = scheduled_jobs[runner_id][1]
            if runner_id in StateCache.get_runners(possible_prefetch):
                # This runner does not need to prefetch anything
                pass
            else:
                # This runner should prefetch this parent state
                reply.prefetch_response.pull_states.append(possible_prefetch)

    send_message(gp_socket, reply)
    trigger(STOP_ACCEPT_PREFETCH, 0)


def receive_message_blocking(socket):
    msg = None
    try:
        msg = socket.recv()  # only polling
        msg = parse(msg)

    except zmq.error.Again:
        # could not poll anything
        pass

    return msg


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



runners_last = {}
alpha = {}
alpha_weights = {}  # copy of alpha that is decremented as soon as a weight is there. This must be empty to fullfill the all weights received criteria
alpha_master = {}  # copy of alpha that is used to remember which weights to resample from for the next iteration, is autofilled for 0th iteration!
master_jobs_first_cycle = 0
scheduled_jobs = {}


def trigger_select(runner_id, state_id, was_cached):
    if trigger.enabled:
        now = time.time() - trigger.null_time
        trigger_select.evts.append((now, runner_id, state_id.t, state_id.id, was_cached))
trigger_select.evts = []


def write_trigger_select_events():
    with open('select_events.melissa_p2p_server.csv', 'w+') as f:
        f.write("time,runner_id,state_t,state_id,was_cached\n")
        for evt in trigger_select.evts:
            it = list(evt)
            it[0] = it[0]*1000
            it[-1] = 1 if it[-1] else 0
            f.write(",".join([str(x) for x in it]))
            f.write("\n")


def needs_runner(alpha_, parent_id, Q):
    return math.ceil(alpha_[parent_id] / Q) - DueDates.runners_per_parent(parent_id) >= 1

# for stats:
state_loads = 0
state_loads_wo_cache = 0
speculative_state_loads = 0
speculative_state_loads_wo_cache = 0

len_alpha = PARTICLES
last_P = PARTICLES
def select_parent(runner_id, alpha, StateCache, R):
    """Selects either a state from this cycle or if not possible from the next cycle"""
    global state_loads, state_loads_wo_cache, speculative_state_loads, speculative_state_loads_wo_cache, len_alpha, runners_last, master_jobs_first_cycle
    if stealable_jobs > 0:
        if assimilation_cycle == 1 and master_jobs_first_cycle < PARTICLES:
            master_jobs_first_cycle += 1  # autogenerate some jobs from existing particles...
            parent_id = cm.StateId()
            parent_id.t = 0
            parent_id.id = runner_id

            if parent_id not in alpha:
                alpha[parent_id] = 0
            alpha[parent_id] += 1
            if parent_id not in alpha_master:
                alpha_master[parent_id] = 0
            alpha_master[parent_id] += 1
            if parent_id not in alpha_weights:
                alpha_weights[parent_id] = 0
            alpha_weights[parent_id] += 1
            print(f"inc_alpha_weights sum={inner_sum(alpha_weights)}")

            state_loads += 1
            state_loads_wo_cache += 1
            cachehit = False
        else:
            parent_id, cachehit = scheduling_policy.select_parent(runner_id, alpha, len_alpha, StateCache, R)

        if parent_id:
            len_alpha -= 1
            # Stat counting
            if not cachehit:
                state_loads += 1
                if runner_id in runners_last and parent_id != runners_last[runner_id]:
                    state_loads_wo_cache += 1
    else:
        if IS_SPECULATIVE:
            alpha_2, _ = resample(assimilation_cycle)
            alpha_2, _ = remove_weighted_particles(alpha_2)
            alpha_2, _ = remove_duedate_particles(alpha_2)

            P = len(alpha_2)
            if P == 0:
                return None, None

            len_alpha_2 = count_members(alpha_2)
            strategy = '1c'  # normal strategy
            # strategy = '1b'  # take highest weight, disregard cache.
            # strategy = '1d'  # schedule most important where one is sure...
            # normal strategy 1c - just use the same strategy again...:
            if strategy == '1c':
                parent_id, cachehit = scheduling_policy.select_parent(runner_id, alpha_2, len_alpha_2, StateCache, R)
            elif strategy == '1b':
                # lets do max weight scheduling... schedule the particle that is the most probable
                # to be necessary...
                parent_id = max(alpha_2, key=alpha_2.get) # .. take the particle that need to be propagated the most of the times = argmax(alpha_2)
                if parent_id:
                    cachehit = parent_id in StateCache.get_by_runner(runner_id)
            elif strategy == '1d':
                parent_id = max(alpha_2, key=alpha_2.get) # .. take the particle that need to be propagated the most of the times = argmax(alpha_2)
                if parent_id:
                    if alpha_2[parent_id] > 1:
                        cachehit = parent_id in StateCache.get_by_runner(runner_id)
                    else:
                        return None, None


            if parent_id:
                # Stat counting
                if not cachehit:
                    speculative_state_loads += 1
                    if runner_id in runners_last and parent_id != runners_last[runner_id]:
                        speculative_state_loads_wo_cache += 1
        else:
            return None, None

    runners_last[runner_id] = parent_id

    return parent_id, cachehit

def remove_weighted_particles(alpha_):
    number_removed = 0
    out = copy.copy(alpha_)

    # Remove already done particles
    for w in state_weights:
        parent_id = job_to_parent_map[w]
        if parent_id.t == assimilation_cycle and parent_id in out:
            number_removed += 1
            out[parent_id] -= 1
            if out[parent_id] == 0:
                del out[parent_id]

    print(f"Removed {number_removed} particles as they have weights already.")
    return out, number_removed

def remove_duedate_particles(alpha_):
    # removes particles that have already a due date and are thus scheduled
    number_removed = 0
    out = copy.copy(alpha_)

    # Remove already scheduled particles
    for _, entries in DueDates.due_dates.items():
        for _, _, parent_id in entries:
            if parent_id.t == assimilation_cycle and parent_id in out:
                number_removed += 1
                out[parent_id] -= 1
                if out[parent_id] == 0:
                    del out[parent_id]

    print(f"Removed {number_removed} particles as they are scheduled/running already.")
    return out, number_removed

next_job_id = 0
def generate_job_id(t):
    global next_job_id
    job_id = cm.StateId()
    job_id.t = t
    job_id.id = next_job_id
    next_job_id += 1
    return job_id

def handle_job_requests(launcher, nsteps):
    """take a job from unscheduled jobs and send it back to the runner. Take one that is
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
            # already something scheduled. Go ahead with it.
            job_id, parent_id = scheduled_jobs[runner_id]
            del scheduled_jobs[runner_id]
            # in this case we remove the duedate for the scheduled state:
            DueDates.remove(runner_id, job_id)
            remove_from_alpha = False
            cachehit = True
        else:
            parent_id, cachehit = select_parent(runner_id, alpha, StateCache, len(runners_last))

        if parent_id:
            trigger_select(runner_id, parent_id, cachehit)
            # bookkeeping
            if remove_from_alpha and \
                    parent_id.t == assimilation_cycle-1:  # filter out speculative particles
                alpha[parent_id] -= 1
                if alpha[parent_id] == 0:
                    del alpha[parent_id]

                stealable_jobs -= 1


            job_id = generate_job_id(parent_id.t + 1)
            job_to_parent_map[job_id] = parent_id

            reply.job_response.job.CopyFrom(job_id)
            reply.job_response.parent.CopyFrom(parent_id)
            print("Running", (job_id, parent_id), "on", runner_id)
            trigger(STOP_IDLE_RUNNER, runner_id)
            trigger(START_PROPAGATE_STATE, job_id.id)
            DueDates.add(runner_id, job_id, parent_id)

        # REM: it is totally fine to send messages without a parent_id/job_id set. In
        # this case the runner will just try again in some ms (see app_core.cxx)
        reply.job_response.nsteps = nsteps
        send_message(job_socket, reply)
        trigger(STOP_HANDLE_JOB_REQ, 0)


def resample(parent_t, alpha_master_=None):

    global validator_ids, validation_sockets

    trigger(START_RESAMPLE, parent_t)
    # get all weights from where to resample
    # TODO: this is unclean: this means that for speculative resampling we use all weights we have when the iteration is not yet finished

    print("len state_weights:", len(state_weights))
    this_cycle = [sw for sw in state_weights if sw.t == parent_t]
    print("len(this_cycle)", len(this_cycle))

    if alpha_master_ is not None:
        assert len(this_cycle) >= PARTICLES
        print("alpha_master", alpha_master_, f"sum={inner_sum(alpha_master_)}")
        am = copy.copy(alpha_master_)
        b = this_cycle
        this_cycle = []
        for sw in b:
            pid = job_to_parent_map[sw]
            if pid in am:
                am[pid] -= 1
                if am[pid] == 0:
                    del am[pid]

                this_cycle.append(sw)

        assert len(am) == 0

    sum_weights = np.sum([state_weights[x] for x in this_cycle])
    state_weights_normalized = [state_weights[x] / sum_weights for x in this_cycle]

    #############################################################
    #
    #   HANDLE STATE VALIDATION
    #
    #############################################################

    pattern = os.getcwd() + '/worker-*-ip.dat'
    worker_ip_files = glob.glob(pattern)

    p = re.compile("worker-(.*)-ip.dat")
    for fn in worker_ip_files:
        id = int(p.search(os.path.basename(fn)).group(1))
        if id not in validation_sockets:
            validator_ids.append(id)
            with open(fn, 'r') as file:
                ip = file.read().rstrip()
            addr = "tcp://" + ip + ":4000"
            so = context.socket(zmq.REP)
            so.connect(addr)
            validation_sockets[id] = so

    if len(validation_sockets) > 0:
        for id in validation_sockets:
            print(f"[{id}] waiting for worker message...")
            receive_message_blocking( validation_sockets[id] )
            print(f"[{id}] received worker message!")

        validate_states = []
        for idx, s in enumerate(this_cycle):
            weight = cm.Weight()
            weight.weight = state_weights_normalized[idx]
            weight.state_id.CopyFrom(s)
            validate_states.append(weight)

        num_states = len(validate_states)
        validators_avail = len(validation_sockets)

        chunk_size = num_states // validators_avail
        iter_mod = num_states % validators_avail
        if chunk_size > 0:
            validators = validator_ids
        else:
            validators = validator_ids[:iter_mod]

        start = 0
        for idx, id in enumerate(validators):
            end = start + chunk_size + 1 if idx < iter_mod else 0
            request = cm.Message()
            request.validation_request.validator_ids.extend(validators)
            for s in validate_states[start:end]:
                request.validation_request.to_validate.append(s)
            print(f"now sending states to worker {id}", request.validation_request.to_validate)
            send_message(validation_sockets[id], request)
            start = end

        #vid = 0
        #chunk_size = int(np.ceil(float(len(validate_states)) / len(validation_sockets)))
        #if (len(validate_states) / chunk_size) >= 1.0:
        #    validators = validator_ids
        #else:
        #    validators = [id for id in validator_ids][:len(validate_states)]
        #for i in range(0, len(validate_states), chunk_size):
        #    request = cm.Message()
        #    request.validation_request.validator_ids.extend(validators)
        #    for s in validate_states[i:i+chunk_size]:
        #        request.validation_request.to_validate.append(s)
        #    print(f"now sending states to worker {validators[vid]}", request.validation_request.to_validate)
        #    send_message(validation_sockets[validators[vid]], request)
        #    vid += 1
        for id in validator_ids:
            if id not in validators:
                request = cm.Message()
                request.validation_request.SetInParent()
                print("sending empty message to worker id: ", id)
                send_message(validation_sockets[id], request)


    print("---- Resampling from particles with t=%d ----" % parent_t)
    # print('normalized weights:', state_weights_normalized)

    method = 'residual'
    if method == 'multinomal':
        # Resampling ?multinomal? TODO: compare with paper which version it is...
        # normalize state weights and resample
        out_particles = np.random.choice(this_cycle, size=len(this_cycle), p=state_weights_normalized)
        # if parent_t < 4:  # don't do anything interesting in the first 4 hours
            # out_particles = this_cycle  # keep all particles...
    elif method == 'residual':

        out_particles = []
        rest_pids = []
        rest_probs = []
        for pid, normalized_weight in zip(this_cycle, state_weights_normalized):
            ww = normalized_weight * len(state_weights_normalized)
            if ww >= 1:
                out_particles = np.concatenate([out_particles, [pid]*int(ww)])
            if ww % 1 > 0:
                 rest_pids.append(pid)
                 rest_probs.append(ww % 1)

        # print('out_particles', out_particles)
        # print('rest_probs', rest_probs)
        # draw rest particle multinomial if necessary
        if len(rest_pids) > 0:
            np.random.seed(42)
            number_rest_pids = len(state_weights_normalized)-len(out_particles)
            print('Drawing %d particles by chance' % number_rest_pids)
            rest_particles = np.random.choice(rest_pids, size=number_rest_pids, p=np.array(rest_probs) / np.sum(rest_probs))
            out_particles = np.concatenate([out_particles, rest_particles])

    else:
        assert False  # Method unimplemented!

    print('out_parts', out_particles)

    alpha_ = {}
    number_jobs = len(out_particles)
    for op in out_particles:
        if op not in alpha_:
            alpha_[op] = 1
        else:
            alpha_[op] += 1


    trigger(STOP_RESAMPLE, parent_t)
    return alpha_, number_jobs


# invariant: amount of duedates per cycle + amount of state_weights per cycle finished + len(alpha) per cycle == PARTICLES


def do_update_step():
    global assimilation_cycle, next_job_id, alpha, alpha_weights, alpha_master, stealable_jobs, last_P, state_loads, state_loads_wo_cache, speculative_state_loads, speculative_state_loads_wo_cache, len_alpha, validate_states
    print("======= Performing update step after cycle %d ========" % assimilation_cycle)
    print(f"State load performance(R={len(runners_last)}: min=P={last_P}, real state loads={state_loads}, state loads wo cache={state_loads_wo_cache}, max={last_P+len(runners_last)-1}")
    state_loads = speculative_state_loads
    state_loads_wo_cache = speculative_state_loads_wo_cache
    speculative_state_loads = 0
    speculative_state_loads_wo_cache = 0


    assert stealable_jobs == 0

    alpha_master, stealable_jobs = resample(assimilation_cycle, alpha_master)

    alpha = copy.copy(alpha_master)  # remember which weights will be important!

    print("Stealable jobs:", stealable_jobs)
    assert(stealable_jobs == PARTICLES)


    # remove from alpha things that were preexecuted...
    alpha_weights, removed_jobs = remove_weighted_particles(alpha_master)
    stealable_jobs -= removed_jobs
    alpha, removed_jobs = remove_duedate_particles(alpha_weights)
    stealable_jobs -= removed_jobs

    print(f'len(alpha_weights), sum={inner_sum(alpha_weights)}, stealable_jobs', len(alpha_weights), stealable_jobs)

    #############################################################
    #
    #   CHECKPOINTING
    #
    #############################################################


    # Checkpoint server
    with open('checkpoint.bin.tmp', 'wb') as f:
        pickle.dump(alpha_master, f)
    os.rename('checkpoint.bin.tmp', f'checkpoint.bin.{assimilation_cycle}')

    last_P = len(alpha_master)

    assimilation_cycle += 1

    len_alpha = PARTICLES

    print(f"we got P={last_P} different particles!")

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

    IS_SPECULATIVE = os.getenv("MELISSA_DA_PARTICLE_FILTER_SPECULATIVE") == '1'

    # check if we can restart the server
    is_restart = False
    for last_assimilation_cycle in range(1, CYCLES):
        if not os.path.exists(f'checkpoint.bin.{last_assimilation_cycle+1}'):
            break
        else:
            is_restart = True

    if is_restart:
        checkpoint_file_name = f'checkpoint.bin.{last_assimilation_cycle}'
        print(f'Restarting from {checkpoint_file_name} ...')
        with open(checkpoint_file_name, 'rb') as f:
            # FIXME: test if checkpointing still works. Have a testcase for this!
            alpha_master = pickle.load(f)
            alpha_weights = copy.copy(alpha_master)
            alpha = copy.copy(alpha_master)
            assimilation_cycle = last_assimilation_cycle



    server_loops_last_second = 0
    last_second = 0
    while True:
        server_loops_last_second += 1
        if int(time.time()) > last_second:
            last_second = int(time.time()) + 4
            print('server loops last 5 second: %d' % server_loops_last_second)
            server_loops_last_second = 0
            cmd = 'echo "[SERVER INFO] number of open file descriptors: $(ls -l /proc/self/fd/ | wc -l)"'
            os.system(cmd)

        # maybe for testing purpose call launcehr loop here (but only the part that does no comm  with the server...
        handle_general_purpose()

        # REM: maybe_update is called only after a weight arrived in handle_general_purpose()

        handle_job_requests(launcher, nsteps)

        if not launcher.receive_text():
            if not launcher.check_launcher_due_date():
                raise Exception("Launcher did not ping me for too long!")

        launcher.ping()

        if trigger.enabled:  # FIXME: hack to not crash runners that are about to write traces!
            DueDates.check_violations()

        # Slow down CPU:
        time.sleep(0.000001)


        if maybe_write():
            # also write trigger select events
            write_trigger_select_events()
