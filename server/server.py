#!/usr/bin/env python3
# Copied from https://www.bogotobogo.com/python/python_network_programming_tcp_server_client_chat_server_chat_client_select.php
# ... and then modified

# chat_server.py
 
import sys, socket, select, time
import asyncio
import os

HOST = '' 
SOCKET_LIST = []
RECV_BUFFER = 4096
FRAME_ID_MAX = 128

EMPTY_MSG = b'\0\0\0\0'

usage = "Usage: frame_latency [port]\nPort default is 15000\nlatency must be greater than 0, less than " + str(FRAME_ID_MAX//4)

# TODO clients and buf are used, like, everywhere; maybe make them object members at some point?

class Host:
    def __init__(self, latency):
        self.buf = [list() for _ in range(FRAME_ID_MAX)]
        self.frame = 0
        self.clients = []
        self.latency = latency

class Client:
    def __init__(self, ix, sock):
        self.ix = ix
        self.sock = sock
        self.listener = None
        self.inited = False

def rm(clients, ix, cl):
    if clients[ix] is not cl:
        # We have a couple conditions that can cause us to throw out a client.
        # Without getting too much into it, we've got at least one condition where we might try to remove somebody that's already removed.
        # Fortunately we can just synchronize on the list of clients
        return
    clients[ix] = None
    # According to https://docs.python.org/3/library/asyncio-task.html#creating-tasks,
    # asyncio only keeps weak references to tasks, so no need to gather it up or anything
    # if we just want it to disappear after cancellation.
    # I want to say I had a different experience, so maybe I'm misreading the docs -
    # but even if this does leak a reference, it's only when a player disconnects, which is pretty rare.
    cl.handler.cancel()
    # Honestly I have no idea what I'm doing here, let's try to close this socket politely
    try:
        cl.sock.shutdown(socket.SHUT_RDWR)
    except:
        pass
    try:
        cl.sock.close()
    except:
        pass

async def handle_client(host, ix):
    lp = asyncio.get_event_loop()
    cl = host.clients[ix]
    sock = cl.sock
    recvd = b'';
    try:
        while True:
            data = await lp.sock_recv(sock, RECV_BUFFER)
            if not data:
                print(f"Client {ix} disconnected")
                rm(host.clients, ix, cl)
                return
            recvd += data
            while True:
                l = len(recvd)
                if l < 5:
                    break
                end = 5 + int.from_bytes(recvd[1:5], 'big')
                if l < end:
                    break
                src_frame = recvd[0]
                payload = recvd[1:end]
                recvd = recvd[end:]

                if src_frame >= FRAME_ID_MAX:
                    print(f"Bad frame number {src_frame}, raising exception now")
                    raise Exception("Bad frame number, invalid network communication")
                delt = (host.frame + FRAME_ID_MAX//2 - src_frame) % FRAME_ID_MAX - FRAME_ID_MAX//2
                if delt > host.latency:
                    print(f"client {ix} delivered packet {delt-host.latency} frames late")
                    # If they're late, then at least we want them to have as little latency as possible
                    # There shouldn't be anythig in the current frame's payload, so just assume we can overwrite it.
                    dest_frame = host.frame
                else:
                    dest_frame = (src_frame + host.latency) % FRAME_ID_MAX
                host.buf[dest_frame][ix] = payload
    except:
        # TODO Print exception
        print(f"Exception, closing socket {ix}")
        try:
            rm(host.clients, ix, cl)
        except:
            pass # TODO be more descriptive

async def get_clients(host, port):
    lp = asyncio.get_event_loop()

    # Open port
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.setblocking(False)
    server_socket.bind((HOST, port))
    server_socket.listen()
 
    print("Server started on port " + str(port))

    clients = host.clients
    while True:
        s, _addr = await lp.sock_accept(server_socket)
        s.setblocking(False)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        for ix in range(len(clients)):
            if clients[ix] is None:
                break
        else:
            ix = len(clients)
            clients.append(None)
            for b in host.buf:
                b.append(EMPTY_MSG)
        cl = Client(ix, s)
        clients[ix] = cl
        cl.handler = asyncio.create_task(handle_client(host, ix))
        print(f"Connected client at position {ix}")
    # TODO This is unreachable, do we need to clean this up?
    #      May not matter since this only happens when the program exits,
    #      in which case we can probably (maybe) rely on the OS to handle the cleanup
    server_socket.shutdown(socket.SHUT_RDWR) # Not sure this is necessary, but I know it doesn't hurt!
    server_socket.close()

""" TODO: Rework into not operating directly on sockets?
https://docs.python.org/3/library/asyncio-eventloop.html#working-with-socket-objects-directly

In general, protocol implementations that use transport-based APIs such as loop.create_connection() and loop.create_server() are faster than implementations that work with sockets directly. However, there are some use cases when performance is not critical, and working with socket objects directly is more convenient.
"""

async def loop(host, port, framerate = 30):
    connection_listener = asyncio.create_task(get_clients(host, port))

    recv_len_warnings = 10


    # Framerate setup
    incr = 1 / framerate
    target = 0

    # Main loop
    # TODO: Maybe need an exit condition?
    while True:

        # Framerate
        t = time.monotonic()
        duration = target - t
        if duration <= 0:
            if target != 0:
                print("Missed a frame!")
            target = t
        else:
            try:
                await asyncio.sleep(duration)
            except KeyboardInterrupt:
                print("Keyboard interrupt caught, quitting")
                break
        target += incr

        clients = host.clients.copy()
        frame = host.frame
        host.frame = (frame+1)%FRAME_ID_MAX

        # Considered sending the frame early if we got all the data in,
        # including a 4-byte "delay" entry in the header.
        # Haven't done that yet, not sure how much help it would be.
        msg = bytes([frame, len(clients)])
        pieces = host.buf[frame]
        for ix in range(len(clients)):
            msg += pieces[ix]
            pieces[ix] = EMPTY_MSG

        msg_len = len(msg)
        for ix in range(len(clients)):
            cl = clients[ix]
            if cl is None:
                continue
            if cl.inited: # Ugh this feels ugly but oh well
                m = msg
                l = msg_len
            else:
                cl.inited = True
                m = bytes([0x80, ix, host.latency, frame]) + msg
                l = 4 + msg_len
            if cl.sock.send(m) != l:
                # Time to pay the piper for my shitty netcode,
                # I think there's a good chance this will choke up if I try to send a couple KB at once...
                print(f"Network backup, closing socket for client {ix}");
                rm(host.clients, ix, cl)

    print("Gathering up tasks")
    connection_listener.cancel()
    await asycio.gather(connection_listener, return_exceptions=True)

    # Originally we'd never get here until everybody had disconnected anyway,
    # so it we could assume the `gather` would complete almost immediately.
    # Now this is actually unreachable, so... guess we'll have to revisit this
    # when the exit condition is fixed.
    #await asyncio.gather(*handlers, return_exceptions = True)

if __name__ == "__main__":
    args = sys.argv
    if len(args) < 2 or len(args) > 3:
        print(usage)
        sys.exit(1)

    try:
        latency = int(args[1])
    except:
        print(usage)
        sys.exit(1)

    if len(args) > 2:
        try:
            port = int(args[2])
        except:
            print(usage)
            sys.exit(1)
    else:
        port = 15000
        print("Using default port.")

    asyncio.get_event_loop().run_until_complete(loop(Host(latency), port))
    print("Goodbye!")
