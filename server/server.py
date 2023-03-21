#!/usr/bin/env python3
# Copied from https://www.bogotobogo.com/python/python_network_programming_tcp_server_client_chat_server_chat_client_select.php
# ... and then modified

# chat_server.py
 
import sys, socket, select, time
import asyncio
import fcntl, os

HOST = '' 
SOCKET_LIST = []
RECV_BUFFER = 4096
FRAME_ID_MAX = 128

usage = "Usage: num_clients frame_latency [port]\nPort default is 15000\nlatency must be greater than 0, less than " + str(FRAME_ID_MAX//4)

def get_clients(num_clients, port):
    # Open port

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, port))
    server_socket.listen()
 
    print("Server started on port " + str(port))

    clients = []
    for x in range(num_clients):
        s = server_socket.accept()[0]
        fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        clients.append(s)
        print("Got a client...")
    print("All clients connected")
    server_socket.shutdown(socket.SHUT_RDWR) # Not sure this is necessary, but I know it doesn't hurt!
    server_socket.close()
    return clients

""" TODO: Rework into not operating directly on sockets?
https://docs.python.org/3/library/asyncio-eventloop.html#working-with-socket-objects-directly

In general, protocol implementations that use transport-based APIs such as loop.create_connection() and loop.create_server() are faster than implementations that work with sockets directly. However, there are some use cases when performance is not critical, and working with socket objects directly is more convenient.
"""

async def loop(clients, latency, framerate = 30):

    lp = asyncio.get_event_loop()

    # Init the buffer of things to be sent
    num_clients = len(clients)
    frame = 0
    buf = [[b'\0' for c in clients] for x in range(FRAME_ID_MAX)]

    recv_len_warnings = 10

    # Tell each client their position and the total number of players
    for ix in range(num_clients):
        c = clients[ix]
        c.send(bytes([0x80, ix, num_clients]))

    # Framerate setup
    incr = 1 / framerate
    target = 0

    running = True

    # Tracking which clients are still connected
    active_clients = clients.copy()
    def rm(ix):
        nonlocal running
        active_clients.remove(clients[ix])
        handlers[ix].cancel()
        clients[ix] = None
        if len(active_clients) == 0:
            print("All clients disconnected")
            running = False

    async def handle_client(ix, sock):
        recvd = b'';
        try:
            while True:
                data = await lp.sock_recv(sock, RECV_BUFFER)
                if not data:
                    sock.close()
                    print(f"Client {ix} disconnected")
                    rm(ix)
                    continue
                recvd += data
                while True:
                    l = len(recvd)
                    if l < 2:
                        break
                    end = 2 + recvd[1]
                    if l < end:
                        break
                    src_frame = recvd[0]
                    payload = recvd[1:end]
                    recvd = recvd[end:]

                    if src_frame >= FRAME_ID_MAX:
                        print(f"Bad frame number {src_frame}, raising exception now")
                        raise Exception("Bad frame number, invalid network communication")
                    delt = (frame + FRAME_ID_MAX//2 - src_frame) % FRAME_ID_MAX - FRAME_ID_MAX//2
                    if delt > latency:
                        print(f"client {ix} delivered packet {delt-latency} frames late")
                        # If they're late, then at least we want them to have as little latency as possible
                        # There shouldn't be anythig in the current frame's payload, so just assume we can overwrite it.
                        buf[frame][ix] = payload
                    else:
                        dest_frame = (src_frame + latency) % FRAME_ID_MAX
                        buf[dest_frame][ix] = payload
        except:
            # TODO Print exception
            print(f"Exception, closing socket {ix}")
            try:
                sock.close()
            except:
                pass # TODO be more descriptive

    handlers = [asyncio.create_task(handle_client(ix, clients[ix])) for ix in range(num_clients)]

    # Main loop
    while running:

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

        msg = bytes([frame]) + bytes(4) # This will be something like `remaining_micros.to_bytes(4, 'big')`
        pieces = buf[frame]
        frame = (frame+1)%FRAME_ID_MAX
        for ix in range(num_clients):
            b = pieces[ix]
            pieces[ix] = b'\0'
            #if b == b'\0' and clients[ix] is not None:
            #    print(f"No packet for client {ix}") # Kinda excessive when combined with the "packet came late" messages
            msg += b
        msg_len = len(msg)
        for c in active_clients.copy():
            if c.send(msg) != msg_len:
                ix = clients.index(c)
                print(f"Network backup, closing socket {ix}");
                rm(ix)
                try:
                    c.close()
                except:
                    pass

    print("Closing down asyncio tasks...")
    # I think this is probably a lot messier than it should be lol
    for h in handlers:
        h.cancel()
    await asyncio.gather(*handlers, return_exceptions = True)

    print("Cleaning up connections...")
    for sock in active_clients:
        sock.shutdown(socket.SHUT_RDWR)
        sock.close()
    
if __name__ == "__main__":
    args = sys.argv
    if len(args) < 3 or len(args) > 4:
        print(usage)
        sys.exit(1)

    try:
        num_clients = int(args[1])
    except:
        print(usage)
        sys.exit(1)

    try:
        latency = int(args[2])
    except:
        print(usage)
        sys.exit(1)

    if len(args) > 3:
        try:
            port = int(args[3])
        except:
            print(usage)
            sys.exit(1)
    else:
        port = 15000
        print("Using default port.")

    clients = get_clients(num_clients, port)
    if clients is None:
        print("Error, aborting")
        sys.exit(1)
    asyncio.get_event_loop().run_until_complete(loop(clients, latency))
    print("Goodbye!")
