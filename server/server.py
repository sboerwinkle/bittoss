#!/usr/bin/env python3
# Copied from https://www.bogotobogo.com/python/python_network_programming_tcp_server_client_chat_server_chat_client_select.php
# ... and then modified

# chat_server.py
 
import sys, socket, select, time

HOST = '' 
SOCKET_LIST = []
RECV_BUFFER = 4096 

usage = "Usage: num_clients frame_latency [port]\nPort default is 15000\nlatency must be greater than 0, less than 16"

def get_clients(num_clients, port):
    # Open port

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, port))
    server_socket.listen()
 
    print("Server started on port " + str(port))

    clients = []
    for x in range(num_clients):
        clients.append(server_socket.accept()[0])
        print("Got a client...")
    print("All clients connected")
    server_socket.shutdown(socket.SHUT_RDWR) # Not sure this is necessary, but I know it doesn't hurt!
    server_socket.close()
    return clients

def loop(clients, latency, framerate = 30):

    # Init the buffer of things to be sent
    num_clients = len(clients)
    frame = 0
    buf = [[b'\0' for c in clients] for x in range(16)]

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
        clients[ix] = None
        if len(active_clients) == 0:
            print("All clients disconnected")
            running = False

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
                time.sleep(duration)
            except KeyboardInterrupt:
                print("Keyboard interrupt caught, quitting")
                break
        target += incr

        # get the list sockets which are ready to be read through select
        ready_to_read, ready_to_write, _ = select.select(active_clients,active_clients,[],0)
      
        for sock in ready_to_read:
            ix = clients.index(sock)
            # process data recieved from client, 
            try:
                # receiving data from the socket.
                data = sock.recv(RECV_BUFFER)
                if data:
                    while data:
                        src_frame = data[0]
                        if src_frame >= 16:
                            print(f"Bad frame number {src_frame}, raising exception now")
                            raise Exception("Bad frame number, invalid network communication")
                        good = True
                        delt = (frame + 15 - src_frame) % 16 + 1
                        if delt > latency:
                            print(f"client {ix} delivered packet {delt-latency} frames late")
                            good = False
                        dest_frame = (src_frame + latency) % 16
                        while len(data) < 2:
                            # If we have a partial message, the rest should be very close, so no harm in blocking for a bit
                            data += sock.recv(RECV_BUFFER)
                        end = data[1] + 2
                        while len(data) < end:
                            data += sock.recv(RECV_BUFFER)
                        payload = data[1:end]
                        data = data[end:]
                        if good:
                            buf[dest_frame][ix] = payload
                        else:
                            # If they're late, then at least we want them to have as little latency as possible
                            # There shouldn't be anythig in the current frame's payload, so just assume we can overwrite it.
                            buf[frame][ix] = payload
                else:
                    sock.close()
                    print(f"Client {ix} disconnected")
                    rm(ix)
            except:
                # TODO Print exception
                print(f"Exception, closing socket {ix}")
                try:
                    sock.close()
                except:
                    pass # TODO be more descriptive
                rm(ix)
        msg = bytes([frame])
        pieces = buf[frame]
        frame = (frame+1)%16
        for ix in range(num_clients):
            b = pieces[ix]
            pieces[ix] = b'\0'
            #if b == b'\0' and clients[ix] is not None:
            #    print(f"No packet for client {ix}") # Kinda excessive when combined with the "packet came late" messages
            msg += b
        msg_len = len(msg)
        for c in active_clients.copy():
            if c not in ready_to_write or c.send(msg) != msg_len:
                ix = clients.index(c)
                print(f"Network backup, closing socket {ix}");
                rm(ix)
                try:
                    c.close()
                except:
                    pass

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
    loop(clients, latency)
    print("Goodbye!")
