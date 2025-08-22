import socket
import struct

DATA_LENGTH = 500

if __name__ == '__main__':

    # initialize counter
    counter = 0
    success_counter = 0

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Get the local IP address for binding
    host = '0.0.0.0'  # Listen on all available interfaces
    port = 65432      # Port to listen on

    server_socket.bind((host, port))
    print(f"EGO listening on {host}:{port}")

    ### Start Simulation ###
    while True:        

        #JOON: this might need to change if additional overhead is introduced
        if counter > 0:
            server_socket.settimeout(0.10)

        print("Waiting for preceding to send data...")
        try:
            data, addr = server_socket.recvfrom(1024)
            print("Received Data!")

            # Extract sender's IP and port
            sender_ip = addr[0]
            sender_port = addr[1]

            # Extract receiver's specific interface IP
            receiver_ip, receiver_port = server_socket.getsockname()
            print(f"Received data on interface bound to IP: {receiver_ip}, Port: {receiver_port}")
            print(f"Received data from IP: {sender_ip}, Port: {sender_port}")

            received_data = struct.unpack('!4f', data)  
            prec_counter = received_data[0]
            print("Recived data:", received_data)

            ack = b'\x01'
            server_socket.sendto(ack, addr)

            success_counter += 1
            print(f"Current Success Count: {success_counter}")

        except socket.timeout:

            ack = b'\x00'
            server_socket.sendto(ack, addr)

        # if preceding ends the connection, break out
        if prec_counter == -1 or counter == DATA_LENGTH:
            break

        # ### Increment the counter ###  
        counter += 1   


    server_socket.close()
    print("Connection ended.")

    print(f"Connection Success Rate: {success_counter} / {DATA_LENGTH} = {success_counter/DATA_LENGTH}")