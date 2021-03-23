import zmq
import p2p_pb2 as cm

def parse(buf):
    """Parse a buffer into a message object"""
    parsed = cm.Message()
    parsed.ParseFromString(buf)
    return parsed



def bind_socket(context, t, addr):
    socket = context.socket(t)
    socket.bind(addr)
    port = socket.getsockopt(zmq.LAST_ENDPOINT)
    port = port.decode().split(':')[-1]
    port = int(port)
    return socket, port
