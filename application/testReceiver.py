import socket
import json
import struct

# CN listens to the request from UE
# Creates a socket object for communication over the network.
# socket.AF_INET: Specifies the address family for IPv4. 
# socket.SOCK_DGRAM: Specifies the socket type as UDP.

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
    # Does not bind the socket with a specific receiver IP address
    s.bind(('0.0.0.0', 1234))
    print("Waiting for data from UE...")

    # Receive data and sender's address
    data, addr = s.recvfrom(1024)

    # Extract sender's IP and port
    sender_ip = addr[0]
    sender_port = addr[1]

    # Decode received data
    # received_data = json.loads(data.decode('utf-8'))
    received_data = struct.unpack('!3f', data)  # Assuming you expect to receive three floats

    # Extract receiver's specific interface IP
    receiver_ip, receiver_port = s.getsockname()
    print(f"Received data on interface bound to IP: {receiver_ip}, Port: {receiver_port}")

    # Print all details
    # print(f"Sender IP: {sender_ip}")
    # print(f"Sender Port: {sender_port}")
    # print(f"Receiver IP: {receiver_ip}")
    # print(f"Receiver Port: {receiver_port}")
    print(f"Received Data: {received_data}")

    # # Optionally, send a response back
    # response_data = (1.1, 2.2, 3.4, 5.6)  # Example response data
    # serialized_response = struct.pack('!4f', *response_data)
    # s.sendto(serialized_response, addr)

    # # Prepare the response data
    # response_data = {'key3': 'value3', 'key4': 'value4'}
    # serialized_response = json.dumps(response_data)

    # # Send response back to UE
    # s.sendto(serialized_response.encode('utf-8'), addr)


    