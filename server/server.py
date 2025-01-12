#!/usr/bin/env python3
# Copied from https://www.bogotobogo.com/python/python_network_programming_tcp_server_client_chat_server_chat_client_select.php
# ... and then modified

import sys
import socket
import time
import traceback
import asyncio
import os

FRAME_ID_MAX = 1<<29

# This should match the client's MAX_AHEAD.
MAX_AHEAD = 15


# max number of clients at any given time
MAX_CLIENTS = 32
# max number of new clients in a quick burst. We allow our clients to fill up immediately on server start.
# Probably not any reason this should ever be different from MAX_CLIENTS.
CLIENT_POOL = MAX_CLIENTS
# max sustained rate of new clients. Unit is clients/sec
CLIENT_POOL_RECOVERY_PER_SEC = 0.2
# how big can a single 'message' be from a client (how much data can be associated with one game frame). Unit is bytes
MAX_MESSAGE_SIZE = 2 * (1024 * 1024)
# Most commands that can be queued up by a client at any time
MAX_CMD_COUNT = 16
# max built-up leeway before kicking a client for over-usage. Unit is bytes
USAGE_POOL = 4 * (1024 * 1024)
# max sustained data transfer. Unit is bytes/sec
USAGE_POOL_RECOVERY_PER_SEC = 20 * 1024
# We add some bytes to our usage calculations to compensate for headers. Unit is bytes
HEADER_ADJ = 50
# A slew of small messages could also tax the server unfairly, so these is a fee associated
# with processing messages. Unit is bytes, counts against the client's usage pool.
PROC_ADJ = 50
FRAMERATE = 15
INCR = 1/FRAMERATE

EMPTY_MSG = b'\0\0\0\0'

usage = """Usage: [port] [starting_players]

This program can be run without arguments (e.g. \"./server.py\") for default behavior.
Alternatively, a port number can be provided as the single argument.
A second argument will be interpreted as the `starting_players`, but this is only useful for debugging."""

clientpool = CLIENT_POOL
clientpool_t = 0
def check_clientpool():
    global clientpool, clientpool_t
    now_t = time.monotonic();
    clientpool += CLIENT_POOL_RECOVERY_PER_SEC * (now_t - clientpool_t)
    if clientpool > CLIENT_POOL:
        clientpool = CLIENT_POOL
    clientpool_t = now_t

    if clientpool > 1:
        clientpool -= 1
        return True
    return False

active_host = None
def get_host():
    global active_host
    if active_host is None:
        active_host = Host()
        asyncio.create_task(loop(active_host))
    return active_host

class Host:
    def __init__(self):
        self.frame = 0
        self.clients = [] # [None] * starting_players
        self.clientpool = CLIENT_POOL

class ClientNetHandler(asyncio.Protocol):
    def __init__(self):
        self.live = True
        self.recvd = b''
        self.inited = False
        self.complete_messages = []
        # for client network-rate-limiting
        self.usagepool = USAGE_POOL
        self.usagesince = time.monotonic()
        self.missed_frames = 0

        self.offsets_used = [False] * (MAX_AHEAD + 1)
        self.offsets_offset = 0

        self.cmds = []
        self.cmd_usage = 0
        self.cmds_remaining = 0
        self.determine_cmd_phase()

    def connection_made(self, transport):
        self.transport = transport
        if not check_clientpool():
            print("Rejecting client because host's clientpool is exhausted")
            self.transport.close()
            # Should there be a `return` here? Not sure

        self.host = get_host()
        clients = self.host.clients
        for ix in range(len(clients)):
            if clients[ix] is None:
                break
        else:
            ix = len(clients)
            clients.append(None)
        if ix >= MAX_CLIENTS:
            print("Rejecting client because MAX_CLIENTS reached")
            self.transport.close()
        self.ix = ix
        clients[ix] = self
        print(f"Connected client at position {ix} from {transport.get_extra_info('peername')}")

    def check_usage(self, amt):
        self.usagepool -= amt
        if self.usagepool < 0:
            print(f"Closed client {ix} for using too much data")
            self.transport.close()
            self.live = False
            return True
        return False

    def data_received(self, data):
        self.recvd += data

        ti = time.monotonic()
        if ti - self.usagesince >= 1.0:
            self.usagepool += USAGE_POOL_RECOVERY_PER_SEC*(ti-self.usagesince)
            if self.usagepool > USAGE_POOL:
                self.usagepool = USAGE_POOL
            #print(f"Client {self.ix} data usage pool remaining is {round(self.usagepool/1024, 1)} kb)")
            self.usagesince = ti
        if self.check_usage(len(data)+HEADER_ADJ):
            return
        try:
            while self.live:
                if len(self.recvd) < self.reqd_bytes:
                    break
                self.next_phase()
        except Exception as exc:
            print("Exception while handling client data, killing client")
            traceback.print_exc()
            self.transport.close()

    def phase_size(self):
        self.reqd_bytes = self.recvd[0] + 5
        self.next_phase = self.phase_body

    def phase_body(self):
        if self.check_usage(PROC_ADJ):
            return
        self.missed_frames = 0
        size_bytes = self.recvd[0:1]
        frame_bytes = self.recvd[1:5]
        payload_bytes = self.recvd[5:self.reqd_bytes]
        self.recvd = self.recvd[self.reqd_bytes:]

        frame = int.from_bytes(frame_bytes, 'big')
        if frame >= FRAME_ID_MAX:
            raise Exception(f"Bad frame number {frame}, invalid network communication")
        # Because frame numbers wrap, we do some math to get an offset with +/- FRAME_ID_MAX//2.
        # `delt == 0` corresponds to getting data for the frame that's about to go out.
        delt = (frame - self.host.frame + FRAME_ID_MAX//2) % FRAME_ID_MAX - FRAME_ID_MAX//2

        if delt < 0:
            print(f"client {self.ix} delivered packet {-delt} frames late")
            # If they're late, write it in for the upcoming frame instead.
            # This will be later than they intended, but at least it's not lost.
            frame_bytes = self.host.frame.to_bytes(4, 'big')
            delt = 0
        elif delt > MAX_AHEAD:
            print(f"client {self.ix} delivered packet {delt} frames early, when the max allowed is {MAX_AHEAD}")
            # Existence of this limit takes the guesswork out of how long it takes a new client to sync,
            # and keeps clients from having to hang on to arbitrarily many frames of data
            frame_bytes = ((self.host.frame + MAX_AHEAD) % FRAME_ID_MAX).to_bytes(4, 'big')
            delt = MAX_AHEAD

        offset_index = (self.offsets_offset + delt) % (MAX_AHEAD + 1)
        if not self.offsets_used[offset_index]:
            self.offsets_used[offset_index] = True
            self.complete_messages.append(size_bytes+frame_bytes+payload_bytes)

        self.reqd_bytes = 1
        self.next_phase = self.phase_cmd_count

    def phase_cmd_count(self):
        self.cmds_remaining = self.recvd[0]
        self.recvd = self.recvd[1:]
        self.determine_cmd_phase()

    def phase_cmd_size(self):
        self.reqd_bytes = int.from_bytes(self.recvd[:4], 'big') + 4
        self.next_phase = self.phase_cmd
        if self.check_usage(PROC_ADJ):
            return

        self.cmd_usage += self.reqd_bytes
        if self.cmd_usage > MAX_MESSAGE_SIZE:
            print(f"Closed client {self.ix} for trying to broadcast too large a message")
            self.live = False
            self.transport.close()
        elif len(self.cmds) >= MAX_CMD_COUNT:
            print(f"Closed client {self.ix} for trying to queue too many commands")
            self.live = False
            self.transport.close()

    def phase_cmd(self):
        self.cmds.append(self.recvd[:self.reqd_bytes])
        self.recvd = self.recvd[self.reqd_bytes:]
        self.cmds_remaining -= 1
        self.determine_cmd_phase()

    def determine_cmd_phase(self):
        if self.cmds_remaining:
            self.reqd_bytes = 4
            self.next_phase = self.phase_cmd_size
        else:
            self.reqd_bytes = 1
            self.next_phase = self.phase_size

    def connection_lost(self, exc):
        global active_host
        print(f"Connection to client {self.ix} lost")
        if exc is not None:
            # Apparently in Python 3.11 this is less clumsy
            traceback.print_exception(type(exc), exc, exc.__traceback__)
        clients = self.host.clients
        clients[self.ix] = None
        for c in clients:
            if c is not None:
                break
        else:
            # If everyone drops, we can reset the number of players without confusing anybody.
            # Also, we can pause the main FRAMERATE thread, which in practice we do by killing
            # this host object and making a new one when necessary.
            active_host = None

async def do_server(port):
    try:
        # Open port
        server_socket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # This should be inherited by sockets created via 'accept'
        server_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        # Asyncio turns off mapped addresses, so we have to provide our own socket with mapped addresses enabled
        server_socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        server_socket.bind(('', port))

        server = await asyncio.get_running_loop().create_server(
            lambda: ClientNetHandler(),
            sock=server_socket
        )
        print("Server started on port " + str(port))
    except:
        print("Couldn't create network server!")
        raise
    await server.serve_forever()

async def loop(host):
    print("Starting FRAMERATE thread")
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
            await asyncio.sleep(duration)
        target += INCR

        clients = host.clients.copy()
        numClients = len(clients)
        frame = host.frame
        host.frame = (frame+1)%FRAME_ID_MAX

        anyConnectedClient = False
        msg = frame.to_bytes(4, 'big') + numClients.to_bytes(1, 'big')
        # Add everyone's data to the message, then clear their data
        for c in clients:
            if c is None:
                msg += bytes([255]) # 255 == -1 (signed char)
                continue
            anyConnectedClient = True
            items = c.complete_messages
            msg += bytes([len(items)])
            if len(items):
                # First item gets special processing,
                # so we manually extract the iterable.
                # Is this even helpful? Maybe not.
                it = items.__iter__()
                msg += it.__next__()
                # Add all pending commands here, then clear the pending list
                msg += bytes((len(c.cmds),)) # add one byte w/ number of cmds
                msg += b''.join(c.cmds)
                c.cmds.clear()
                c.cmd_usage = 0
                # Any other messages sent at this time have no attached commands.
                for i in it:
                    msg += i + b'\0';
                items.clear()
            else:
                c.missed_frames += 1
                if c.missed_frames >= FRAMERATE*5:
                    c.transport.close()
                    print(f"Closed client {c.ix} for not completing any messages for 5 seconds")
            # This frame has gone out, so whether or not the client sent anything here
            # it's time to recycle that entry in `offsets_used` (a circular buffer) so
            # that it represents a new frame (that is, the one MAX_AHEAD frames ahead)
            c.offsets_used[c.offsets_offset] = False
            c.offsets_offset = (c.offsets_offset + 1) % (MAX_AHEAD + 1)

        if not anyConnectedClient:
            print("All clients disconnected, shutting down FRAMERATE thread until next connection")
            return

        for ix in range(numClients):
            cl = clients[ix]
            if cl is None:
                continue
            if cl.inited: # Ugh this feels ugly but oh well
                m = msg
            else:
                # This is the first time we've talked to this client;
                # send some basic context about what's going on.
                # This is immediately followed by this frame's data in the usual fashion.
                cl.inited = True
                m = bytes([0x82, ix, numClients]) + frame.to_bytes(4, 'big') + msg
            cl.transport.write(m)

if __name__ == "__main__":
    args = sys.argv
    if len(args) > 3:
        print(usage)
        sys.exit(1)

    if len(args) > 1:
        try:
            port = int(args[1])
        except:
            print(usage)
            sys.exit(1)
    else:
        port = 15000
        print("Using default port.")

    if len(args) > 2:
        try:
            starting_players = int(args[2])
        except:
            print(usage)
            sys.exit(1)
    else:
        starting_players = 0

    asyncio.run(do_server(port))
    print("Goodbye!")
