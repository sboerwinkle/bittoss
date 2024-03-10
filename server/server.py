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
BUF_SLOTS = 128
if (FRAME_ID_MAX % BUF_SLOTS != 0):
    print("Bad server configuration, please fix bugs.")
    exit()

# max number of clients at any given time
MAX_CLIENTS = 32
# max number of new clients in a quick burst. We allow our clients to fill up immediately on server start.
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

EMPTY_MSG = b'\0\0\0\0'

usage = "Usage: frame_latency [port] [starting_players]\nlatency must be greater than 0, less than " + str(BUF_SLOTS//4) + "\nPort default is 15000\nstarting_players is probably not an option you need"

class Host:
    def __init__(self, latency, starting_players):
        self.buf = [[EMPTY_MSG] * starting_players for _ in range(BUF_SLOTS)]
        self.frame = 0
        self.clients = [None] * starting_players
        self.clientpool = CLIENT_POOL
        self.latency = latency

class ClientNetHandler(asyncio.Protocol):
    def __init__(self, host):
        self.host = host
        self.recvd = b''
        self.inited = False
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
            for b in self.host.buf:
                b.append(EMPTY_MSG)
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
            #print(f"Client {ix} data usage: {round(self.usage/(ti-self.usagesince)/1024, 1)} kb/sec (sustained max limit is {round(USAGE_POOL_RECOVERY_PER_SEC/1024, 1)} kb/sec, pool remaining is {round(self.usagepool/1024, 1)} kb)")
            self.usagesince = ti
        self.usagepool -= len(data)+HEADERADJ
        if self.usagepool < 0:
            print(f"Closed client {ix} for using too much data")
            self.transport.close()
        try:
            while True:
                l = len(self.recvd)
                if l < 4:
                    break
                end = 4 + int.from_bytes(self.recvd[:4], 'big')
                if end < 8:
                    print(f"Client {ix} sent too few bytes expecting minimum 4 for the frame ID")
                    raise Exception("Too few bytes")
                if end > MAX_MESSAGE_SIZE:
                    print(f"Closed client {ix} for trying to broadcast too large a message")
                    self.transport.close()
                    break
                if l < end:
                    # We don't yet have enough data for the next complete message
                    break
                src_frame = int.from_bytes(self.recvd[4:8], 'big')
                payload = self.recvd[:end]
                self.recvd = self.recvd[end:]

                if src_frame >= FRAME_ID_MAX:
                    raise Exception(f"Bad frame number {src_frame}, invalid network communication")
                # TODO Feels like this `dest_frame` logic needs to be rearranged to express the same
                #      checks in a less confusing way, but I'm too tired for that right now
                delt = (host.frame + FRAME_ID_MAX//2 - src_frame) % FRAME_ID_MAX - FRAME_ID_MAX//2
                if delt > host.latency:
                    print(f"client {ix} delivered packet {delt-host.latency} frames late")
                    # If they're late, then at least we want them to have as little latency as possible
                    # There shouldn't be anything in the current frame's payload, so just assume we can overwrite it.
                    dest_frame = host.frame
                else:
                    if delt <= 0:
                        print(f"client {ix} delivered packet {1-delt} frames _early_?")
                        # If it's too far in advance, we'd be writing it into a weird nonsense place.
                        # We add a `- 1` to protect the previous buffer slot, which the outbound "thread"
                        # might be reading from currently.
                        if host.latency - delt >= BUF_SLOTS - 1:
                            print("    (discarding)")
                            continue
                    dest_frame = (src_frame + host.latency) % FRAME_ID_MAX
                host.buf[dest_frame%BUF_SLOTS][ix] = payload
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

async def loop(host, port, framerate = 30):
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
            await asyncio.sleep(duration)
        target += incr

        clients = host.clients.copy()
        frame = host.frame
        host.frame = (frame+1)%FRAME_ID_MAX

        host.clientpool += CLIENT_POOL_RECOVERY_PER_SEC * incr
        if host.clientpool > MAX_CLIENTS:
            host.clientpool = MAX_CLIENTS

        # Considered sending the frame early if we got all the data in,
        # including a 4-byte "delay" entry in the header.
        # Haven't done that yet, not sure how much help it would be.
        msg = frame.to_bytes(4, 'big') + len(clients).to_bytes(1, 'big')
        pieces = host.buf[frame%BUF_SLOTS]
        # Add everyone's data to the message, then clear their data
        for ix in range(len(clients)):
            msg += pieces[ix]
            pieces[ix] = EMPTY_MSG

        for ix in range(len(clients)):
            cl = clients[ix]
            if cl is None:
                continue
            if cl.inited: # Ugh this feels ugly but oh well
                m = msg
            else:
                # This is the first time we've talked to this client,
                # they need to know which index they are and what latency
                # we're using.
                cl.inited = True
                m = bytes([0x80, ix, host.latency]) + frame.to_bytes(4, 'big') + msg
            cl.transport.write(m)
    # End of our infinite `loop()`.
    # We don't have any exit condition right now,
    # but if we did we'd probably want to cancel `server`
    # and possibly clean up anybody still in `host.clients`...

if __name__ == "__main__":
    args = sys.argv
    if len(args) < 2 or len(args) > 4:
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

    if len(args) > 3:
        try:
            starting_players = int(args[3])
        except:
            print(usage)
            sys.exit(1)
    else:
        starting_players = 0

    asyncio.get_event_loop().run_until_complete(loop(Host(latency, starting_players), port))
    print("Goodbye!")
