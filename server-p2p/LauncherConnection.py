import zmq

from SimulationStatus import SimulationStatus
import sys
import os
import time

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
from utils import get_node_name

# Tuning melissa4py adding messages needed in Melissa-DA context
class Alive:
    def encode(self):
        return bytes(ctypes.c_int32(7))  # Alive is 7, see melissa_messages.h

# Configuration:
LAUNCHER_PING_INTERVAL = 8  # seconds
LAUNCHER_TIMEOUT = 6000  # seconds

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
