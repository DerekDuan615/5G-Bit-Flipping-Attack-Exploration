import socket
import struct
import numpy as np
import pandas as pd

data_length = 500
VEHICLE_ID = 4


if __name__ == '__main__':
    # define the path to the collected data
    file_path = f"/home/derek/VSC_Python/data/ngsim/vehicle_{VEHICLE_ID}.csv"

    ### Get Dataframe of the leader from the data collected from RDS 1000 ###
    prec_traj_df = pd.read_csv(file_path)    
    # import ipdb; ipdb.set_trace()

    # Create a socket object
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_socket.settimeout(0.12)

    specific_address = ('10.0.0.2', 44495)
    ego_address = ('192.168.70.135', 65432)

    client_socket.bind(specific_address)

    # initialize counter
    counter = 0

    # loop until all data in sent
    while counter != len(prec_traj_df):
        # attempt to send data
        # print(prec_traj_df.iloc[counter])
        data_to_send = (counter,) + tuple(prec_traj_df.iloc[counter][['v_Acc', 'v_Vel', 'Global_X']])
        # print(data_to_send)
        serialized_data = struct.pack('!4f', *data_to_send)
        client_socket.sendto(serialized_data, ego_address)
        print("Sent data for timestep", counter)

        try:
            data, addr = client_socket.recvfrom(1024)
            if data == b'\x00':
                print("Connection Failed!")
        except socket.timeout:
            pass

        counter += 1

    end_data = (-1,0,0,0)
    serialized_end_data = struct.pack('!4f', *end_data)
    client_socket.sendto(serialized_end_data, ego_address)
    client_socket.close()
    print("Communication ended.")