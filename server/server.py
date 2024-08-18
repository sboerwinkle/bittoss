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
# max built-up leeway before kicking a client for over-usage. Unit is bytes
USAGE_POOL = 4 * (1024 * 1024)
# max sustained data transfer. Unit is bytes/sec
USAGE_POOL_RECOVERY_PER_SEC = 20 * 1024
# We add some bytes to our usage calculations to compensate for headers. Unit is bytes
HEADERADJ = 120
FRAMERATE = 15

EMPTY_MSG = b'\0\0\0\0'

usage = """Usage: [port] [starting_players]

This program can be run without arguments (e.g. \"./server.py\") for default behavior.
Alternatively, a port number can be provided as the single argument.
A second argument will be interpreted as the `starting_players`, but this is only useful for debugging."""

class Host:
    def __init__(self, starting_players):
        self.frame = 0
        self.clients = [None] * starting_players
        self.clientpool = CLIENT_POOL

class ClientNetHandler(asyncio.Protocol):
    def __init__(self, host):
        self.host = host
        self.recvd = b''
        self.inited = False
        self.complete_messages = []
        # for client network-rate-limiting
        self.usagepool = USAGE_POOL
        self.usagesince = time.monotonic()

    def connection_made(self, transport):
        self.transport = transport
        # for client reconnection limiting
        if self.host.clientpool < 1:
            print("Rejecting client because host's clientpool is exhausted")
            self.transport.close()
        self.host.clientpool -= 1
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

    def data_received(self, data):
        host = self.host
        ix = self.ix
        self.recvd += data

        ti = time.monotonic()
        if ti - self.usagesince >= 1.0:
            self.usagepool += USAGE_POOL_RECOVERY_PER_SEC*(ti-self.usagesince)
            if self.usagepool > USAGE_POOL:
                self.usagepool = USAGE_POOL
            #print(f"Client {ix} data usage pool remaining is {round(self.usagepool/1024, 1)} kb)")
            self.usagesince = ti
        self.usagepool -= len(data)+HEADERADJ
        if self.usagepool < 0:
            print(f"Closed client {ix} for using too much data")
            self.transport.close()
            return
        try:
            while True:
                l = len(self.recvd)
                if l < 4:
                    break
                size_bytes = self.recvd[:4]
                end = 4 + int.from_bytes(size_bytes, 'big')
                if end < 8:
                    print(f"Client {ix} sent too few bytes; expecting minimum 8 for size + frame but got {end}")
                    raise Exception("Too few bytes")
                if end > MAX_MESSAGE_SIZE:
                    print(f"Closed client {ix} for trying to broadcast too large a message")
                    self.transport.close()
                    break
                if l < end:
                    # We don't yet have enough data for the next complete message
                    break
                frame_bytes = self.recvd[4:8]
                payload_bytes = self.recvd[8:end]
                self.recvd = self.recvd[end:]
                frame = int.from_bytes(frame_bytes, 'big')

                if frame >= FRAME_ID_MAX:
                    raise Exception(f"Bad frame number {frame}, invalid network communication")
                # Because frame numbers wrap, we do some math to get an offset with +/- FRAME_ID_MAX//2.
                # `delt == 0` corresponds to getting data for the frame that's about to go out.
                delt = (frame - host.frame + FRAME_ID_MAX//2) % FRAME_ID_MAX - FRAME_ID_MAX//2
                if delt < 0:
                    print(f"client {ix} delivered packet {-delt} frames late")
                    # If they're late, write it in for the upcoming frame instead.
                    # This will be later than they intended, but at least it's not lost.
                    frame_bytes = host.frame.to_bytes(4, 'big')
                elif delt > MAX_AHEAD:
                    print(f"client {ix} delivered packet {delt} frames early, when the max allowed is {MAX_AHEAD}")
                    # Existence of this limit takes the guesswork out of how long it takes a new client to sync,
                    # and keeps clients from having to hang on to arbitrarily many frames of data
                    frame_bytes = ((host.frame + MAX_AHEAD) % FRAME_ID_MAX).to_bytes(4, 'big')

                self.complete_messages.append(size_bytes+frame_bytes+payload_bytes)

                if len(self.complete_messages) > MAX_AHEAD*3:
                    # Haven't though this all the way through, but I feel like `MAX_AHEAD*2` might result from
                    # the server freezing up and a client with bad ping. `*3` is just excessive though.
                    # We could also rely on the throughput limit, but there is processing overhead per-message.
                    print(f"Closed client {ix} for sending too many messages")
                    self.transport.close()
        except Exception as exc:
            print("Exception while handling client data, killing client")
            traceback.print_exc()
            self.transport.close()

    def connection_lost(self, exc):
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
            # Normally we don't do this since the client doesn't handle disconnections as elegantly as new connections.
            # (Partly by design; people should be allowed to rejoin games.)
            # However, if there's nobody left to remember, we can cleanly reset w/out confusing anybody.
            # This is also nice so the server can be running continuously, at least in theory.
            print("All clients disconnected, resetting client list")
            clients.clear()

async def loop(host, port):
    lp = asyncio.get_event_loop()
    try:
        # Open port
        server_socket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # This should be inherited by sockets created via 'accept'
        server_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        # Asyncio turns off mapped addresses, so we have to provide our own socket with mapped addresses enabled
        server_socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        server_socket.bind(('', port))

        server = await lp.create_server(
            lambda: ClientNetHandler(host),
            sock=server_socket
        )
        print("Server started on port " + str(port))
    except:
        print("Couldn't create network server!")
        traceback.print_exc()
        return

    # Framerate setup
    incr = 1 / FRAMERATE
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
        target += incr

        clients = host.clients.copy()
        numClients = len(clients)
        frame = host.frame
        host.frame = (frame+1)%FRAME_ID_MAX

        host.clientpool += CLIENT_POOL_RECOVERY_PER_SEC * incr
        if host.clientpool > CLIENT_POOL:
            host.clientpool = CLIENT_POOL

        # TODO We're building this from complete_messages now
        # Considered sending the frame early if we got all the data in,
        # including a 4-byte "delay" entry in the header.
        # Haven't done that yet, not sure how much help it would be.
        msg = frame.to_bytes(4, 'big') + numClients.to_bytes(1, 'big')
        # Add everyone's data to the message, then clear their data
        for c in clients:
            if c is None:
                msg += bytes([255]) # 255 == -1 (signed char)
                continue
            items = c.complete_messages
            msg += bytes([len(items)])
            for i in items:
                msg += i;
            items.clear()

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
                m = bytes([0x81, ix, numClients]) + frame.to_bytes(4, 'big') + msg
            cl.transport.write(m)
    # End of our infinite `loop()`.
    # We don't have any exit condition right now,
    # but if we did we'd probably want to cancel `server`
    # and possibly clean up anybody still in `host.clients`...

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

    asyncio.get_event_loop().run_until_complete(loop(Host(starting_players), port))
    print("Goodbye!")
