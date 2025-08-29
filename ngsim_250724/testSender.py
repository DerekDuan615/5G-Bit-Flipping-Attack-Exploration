import socket
import json
import struct

# Data to send
data_to_send = (300.0, 25.0, 2.0)
print(f"Sending data {data_to_send}")

# Pack the data into a binary format
serialized_data = struct.pack('!3f', *data_to_send)  # '!3f' means "network order" and three floats

# Let UE sends this file
specific_ip = '10.0.0.3'
specific_port = 44495     # The port of the sender
dn_ip = '192.168.70.135'  # IP of the data network (DN)
dn_port = 1234  # Port to send data to

# Connect to CN
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
    s.bind((specific_ip, specific_port))  # Bind to a specific IP address and any available port
    s.sendto(serialized_data, (dn_ip, dn_port))
    # s.sendto(serialized_data.encode('utf-8'), (dn_ip, dn_port))

    # # Receive response
    # response, _ = s.recvfrom(1024)
    # received_data = json.loads(response.decode('utf-8'))
    
    # received_data = struct.unpack('!4f', response)  # Assuming the response consists of three floats as well

    # Print the data received
    # print(f"Received from Machine 2: {received_data}")


