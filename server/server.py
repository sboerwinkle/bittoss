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

EMPTY_MSG = b'\0\0\0\0'

usage = "Usage: frame_latency [port] [starting_players]\nlatency must be greater than 0, less than " + str(BUF_SLOTS//4) + "\nPort default is 15000\nstarting_players is probably not an option you need"

class Host:
    def __init__(self, latency, starting_players):
        self.buf = [[EMPTY_MSG] * starting_players for _ in range(BUF_SLOTS)]
        self.frame = 0
        self.clients = [None] * starting_players
        self.latency = latency

class Client:
    def __init__(self, ix, nethandler):
        self.ix = ix
        self.nethandler = nethandler
        self.inited = False

def rm(clients, ix, cl):
    if ix >= len(clients) or clients[ix] is not cl:
        # We have a couple conditions that can cause us to throw out a client.
        # Without getting too much into it, we've got at least one condition where we might try to remove somebody that's already removed.
        # Fortunately we can just synchronize on the list of clients
        return
    clients[ix] = None
    for c in clients:
        if c is not None:
            break
    else:
        # Normally we don't do this since the client doesn't handle disconnections as elegantly as new connections.
        # (Partly by design; people should be allowed to rejoin games.)
        # However, if there's nobody left to remember, we can cleanly reset w/out confusing anybody.
        # This is also nice so the server can be running continuously, at least in theory.
        print("All clients disconnected, resetting client list")
        # I don't think this will mess up any threads? I guess we'll see...
        clients.clear()

class ClientNetHandler(asyncio.Protocol):
    def __init__(self, host):
        self.host = host
        self.recvd = b''

    def connection_made(self, transport):
        self.transport = transport
        clients = self.host.clients
        for ix in range(len(clients)):
            if clients[ix] is None:
                break
        else:
            ix = len(clients)
            clients.append(None)
            for b in self.host.buf:
                b.append(EMPTY_MSG)
        self.ix = ix
        self.client = Client(ix, self)
        clients[ix] = self.client
        print(f"Connected client at position {ix} from {transport.get_extra_info('peername')}")

    def data_received(self, data):
        host = self.host
        ix = self.ix
        self.recvd += data
        try:
            while True:
                l = len(self.recvd)
                if l < 4:
                    break
                end = 4 + int.from_bytes(self.recvd[:4], 'big')
                if end < 8:
                    print(f"Client {ix} sent too few bytes expecting minimum 4 for the frame ID")
                    raise Exception("Too few bytes")
                if l < end:
                    # We don't yet have enough data for the next complete message
                    break
                src_frame = int.from_bytes(self.recvd[4:8], 'big')
                payload = self.recvd[:end]
                self.recvd = self.recvd[end:]

                if src_frame >= FRAME_ID_MAX:
                    print(f"Bad frame number {src_frame}, raising exception now")
                    raise Exception("Bad frame number, invalid network communication")
                delt = (host.frame + FRAME_ID_MAX//2 - src_frame) % FRAME_ID_MAX - FRAME_ID_MAX//2
                if delt > host.latency:
                    print(f"client {ix} delivered packet {delt-host.latency} frames late")
                    # If they're late, then at least we want them to have as little latency as possible
                    # There shouldn't be anything in the current frame's payload, so just assume we can overwrite it.
                    dest_frame = host.frame
                else:
                    if delt <= 0:
                        print(f"client {ix} delivered packet {1-delt} frames _early_?")
                        # If it's too far in advance, we'd be writing it into a weird nonsense place
                        if host.latency - delt >= BUF_SLOTS:
                            print("    (discarding)")
                            continue
                    dest_frame = (src_frame + host.latency) % FRAME_ID_MAX
                host.buf[dest_frame%BUF_SLOTS][ix] = payload
        except Exception as exc:
            print(f"Exception while handling client data, killing client {exc}")
            traceback.print_exc()
            self.transport.close()

    def connection_lost(self, exc):
        #rm MUST ONLY be called from here. If you want this, call client.nethandler.transport.close()
        rm(self.host.clients, self.ix, self.client)
        if exc is not None:
            print(f'Client connection aborted: {exc}')
            traceback.print_exc()

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
                # Make other clients aware by sending an intro message before the data.
                cl.inited = True
                m = bytes([0x80, ix, host.latency]) + frame.to_bytes(4, 'big') + msg
            cl.nethandler.transport.write(m)

    print("Gathering up tasks")
    connection_listener.cancel()
    await asyncio.gather(connection_listener, return_exceptions=True)

    # Originally we'd never get here until everybody had disconnected anyway,
    # so it we could assume the `gather` would complete almost immediately.
    # Now this is actually unreachable, so... guess we'll have to revisit this
    # when the exit condition is fixed.
    #await asyncio.gather(*handlers, return_exceptions = True)

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
