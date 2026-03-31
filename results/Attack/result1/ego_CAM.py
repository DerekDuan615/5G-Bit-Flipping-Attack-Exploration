import socket
import struct
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math
import os

from os.path import join as join
import json


def RDSDataCleanUp(file_path, begin_index, data_length):
    
    """  The columns of the dataset from the file_path should like this:

        Index(['SimTime', 'LonAccel', 'LatAccel', 'Throttle', 'Brake', 'Gear',
       'Heading', 'HeadingError', 'HeadwayDistance', 'HeadwayTime', 'Lane',    
       'LaneOffset', 'RoadOffset', 'Steer', 'TailwayDistance', 'TailwayTime',  
       'Velocity', 'LatVelocity', 'VertVelocity', 'XPos', 'YPos', 'ZPos',      
       'Roll', 'Pitch', 'Yaw', 'EngineRPM', 'SlipFR', 'SlipFL', 'SlipRR',      
       'SlipRL', 'User1', 'User2', 'User3', 'User4', 'User5', 'User6', 'User7',
       'User8', 'User9', 'User10', 'User11', 'User12', 'User13', 'User14',     
       'User15', 'User16', 'User17', 'User18', 'User19', 'User20', 'User21'],  51 columns in total"""    
    
    ### Transfer the file to a pandas DataFrame ###
    prec_traj_raw_df = pd.read_csv(file_path,
                                   sep="\s+",  # separator whitespace
                                   index_col=0)   

    """An example result of prec_traj_raw_df is shown below:

                        SimTime  LonAccel  LatAccel  Throttle  Brake  Gear    Heading  HeadingError  ...  User14  User15  User16  User17  User18  User19  User20  User21
        VidTime                                                                                 ...                                                                
        0.016667   184.600 -0.006539 -0.000011       0.0    0.0   4.0  89.926318     -0.001281  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        0.033367   184.617 -0.006539 -0.000011       0.0    0.0   4.0  89.926230     -0.001283  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        0.050033   184.633 -0.006539 -0.000011       0.0    0.0   4.0  89.926148     -0.001284  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        0.066733   184.650 -0.006539 -0.000011       0.0    0.0   4.0  89.926066     -0.001286  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        0.083400   184.667 -0.006539 -0.000011       0.0    0.0   4.0  89.925977     -0.001287  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        ...            ...       ...       ...       ...    ...   ...        ...           ...  ...     ...     ...     ...     ...     ...     ...     ...     ...
        70.687300  255.200  0.000000 -0.000000       0.0    0.0   0.0   0.164171      0.002871  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        70.704000  255.217  0.000000 -0.000000       0.0    0.0   0.0   0.164171      0.002871  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        70.720600  255.233  0.000000 -0.000000       0.0    0.0   0.0   0.164171      0.002871  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        70.737300  255.250  0.000000 -0.000000       0.0    0.0   0.0   0.164171      0.002871  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        70.754000  255.267  0.000000 -0.000000       0.0    0.0   0.0   0.164171      0.002871  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0

        [4241 rows x 51 columns]"""   


    ### Since the row index of the raw df is the first column of the raw data (because of the output format of RDS 1000), we should manually add the row index from 0 to its length ###
    prec_traj_df = prec_traj_raw_df.copy()     # Make a copy to keep the raw data
    prec_traj_df = prec_traj_df.reset_index()   

    """
        Now the example would look like this:
                VidTime  SimTime  LonAccel  LatAccel  Throttle  Brake  Gear    Heading  ...  User14  User15  User16  User17  User18  User19  User20  User21
        0      0.016667  184.600 -0.006539 -0.000011       0.0    0.0   4.0  89.926318  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        1      0.033367  184.617 -0.006539 -0.000011       0.0    0.0   4.0  89.926230  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        2      0.050033  184.633 -0.006539 -0.000011       0.0    0.0   4.0  89.926148  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        3      0.066733  184.650 -0.006539 -0.000011       0.0    0.0   4.0  89.926066  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        4      0.083400  184.667 -0.006539 -0.000011       0.0    0.0   4.0  89.925977  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        ...         ...      ...       ...       ...       ...    ...   ...        ...  ...     ...     ...     ...     ...     ...     ...     ...     ...
        4236  70.687300  255.200  0.000000 -0.000000       0.0    0.0   0.0   0.164171  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        4237  70.704000  255.217  0.000000 -0.000000       0.0    0.0   0.0   0.164171  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        4238  70.720600  255.233  0.000000 -0.000000       0.0    0.0   0.0   0.164171  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        4239  70.737300  255.250  0.000000 -0.000000       0.0    0.0   0.0   0.164171  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0
        4240  70.754000  255.267  0.000000 -0.000000       0.0    0.0   0.0   0.164171  ...     0.0     0.0     0.0     0.0     0.0     0.0     0.0     0.0

        [4241 rows x 52 columns]
    """

    ### Replace invalid entries (e.g., '.') with NaN (we can comment it if there is no invalid entries, or add more if there are different invalid entries) ###
    prec_traj_df.replace('.', np.nan, inplace=True)

    ### Make sure the type of all values in the df is float for future numeric operations ###
    prec_traj_df = prec_traj_df.astype(float) 

    ### For the data generated from RDS 1000, it may have duplicates at one time step "e.g., 3 rows of 'SimTime=1'". So we need to remove the duplicates ###
    prec_traj_df.drop_duplicates(subset= 'SimTime', 
                                 keep = 'first',
                                 inplace = True)    
        
    ### After removing duplicate rows, the row index retain the original index values, so we need to reset the row index ###
    prec_traj_df = prec_traj_df.reset_index()       

    ### Truncate the df by removing some columns, only keep the columns within the column_name_list ###
    column_name_list = ['SimTime', 'LonAccel', 'Velocity']  # We will calculate the Position by ourselves based on these 3 for convenience
    prec_traj_df = prec_traj_df[column_name_list]

    ### Truncate the df by removing some rows if necessary ###
    #JOON: experimenting for short messages for now
    # begin_index = 500                      # Set it to the value you want
    end_index = begin_index + data_length    # Set it to the value you want
    # end_index = len(prec_traj_df.index) - 500
    prec_traj_df = prec_traj_df.loc[begin_index: end_index, :]  # only keep the rows between the begin_index and end_index  
    prec_traj_df.reset_index(drop=True, inplace=True)           # then reset the row index
    
    
    ### Calculates the time interval between consecutive entries in the 'SimTime' column and converts these differences to a list ###     
    prec_traj_df['dT'] = prec_traj_df['SimTime'].diff().tolist()  # Assigning it to a new column 'dT'   

    ### the way we calculate the time interval will make the first row value of 'dT' to be NaN, therefore, we should finally shift all values 1 row up
    prec_traj_df['dT'].iloc[:len(prec_traj_df)-1] = prec_traj_df['dT'].iloc[1:].values
    """ 
    e.g. row_index  SimTime   dT
            0        1        NaN  # No previous item to subtract from, result is NaN
            1        4        3.0  # Difference between 4 (current) and 1 (previous)
            2        9        5.0  # Difference between 9 (current) and 4 (previous)
            3       16        7.0  # Difference between 16 (current) and 9 (previous)
            4       25        9.0  # Difference between 25 (current) and 16 (previous)
    """

    ### reset the df index to remove the last row ### 
    #JOON: initial code did not work, so replaced it with a straightforward function
    # prec_traj_df.reset_index(drop= True, inplace= True)
    prec_traj_df = prec_traj_df.head(-1)

    ### add a new column of position and we want to calculate it by ourselves since it is complex to convert it from the "XPos/YPos/ZPos" from RDS1000 ###
    prec_traj_df['Position'] = np.zeros(len(prec_traj_df))    
    initial_pos = 0  # set the initial position to zero

    ### Create some intermediate arrays for Position calculation ###       
    pos_array = np.zeros(len(prec_traj_df))      # 1-D array, 'len(prec_traj_df)' items
    kin_array = np.zeros((len(prec_traj_df),1))  # 2-D array, 'len(leader_df)' rows, 1 column
    ut_array = np.zeros((len(prec_traj_df), 1))  
    at2_array = np.zeros((len(prec_traj_df),1)) 

    ut_array[:,0] = np.multiply(prec_traj_df['Velocity'].values, prec_traj_df['dT'].values)   # calculate 'v*t' and put the values in the 1st column (the only column)
    at2_array[:,0] = np.multiply(prec_traj_df['LonAccel'].values, np.square(prec_traj_df['dT'].values))  # calculate 'a*t^2'
    kin_array[:,0] = ut_array[:,0] + 0.5*at2_array[:,0]          # calculate 'v*t + 0.5*a*t^2'
    pos_array[0] = initial_pos

            
    for i in np.arange(1, len(prec_traj_df)):
        pos_array[i] = pos_array[i-1] + kin_array[i-1]
            
    prec_traj_df['Position'] = pos_array    

    return prec_traj_df


def Ego_Traj_Df_Init(prec_traj_df, init_space_headway):
        
    ego_traj_df = pd.DataFrame(columns = ['SimTime',
                                            'dT',
                                            'Real_Prec_Acc',
                                            'Real_Prec_Vel',
                                            'Real_Prec_Pos',
                                            'Quantized_Prec_Acc',                                        
                                            'Quantized_Prec_Vel',
                                            'Quantized_Prec_Pos',
                                            'Received_Prec_Acc',                                        
                                            'Received_Prec_Vel',
                                            'Received_Prec_Pos',
                                            'Ego_Acc_Benign',
                                            'Ego_Vel_Benign', 
                                            'Ego_Pos_Benign', 
                                            'Ego_Acc',
                                            'Ego_Vel', 
                                            'Ego_Pos',             
                                            'Safe_Gap_Benign',                                  
                                            'Space_Headway_Benign',
                                            'Time_Headway_Benign',
                                            'Safe_Gap',
                                            'Space_Headway',
                                            'Time_Headway',                                            
                                            'Bit_Flip_Pairs',
                                            'XOR_Bin',
                                            'XOR_Pattern',
                                            'Real_Gap',
                                            'Communication_Success'])

    ego_traj_df['SimTime'] = prec_traj_df['SimTime'].values     # the number of rows in both DataFrames should match automatically
    ego_traj_df['dT'] = prec_traj_df['dT'].values 
    ego_traj_df['Real_Prec_Acc'] = prec_traj_df['LonAccel'].values
    ego_traj_df['Real_Prec_Vel'] = prec_traj_df['Velocity'].values
    ego_traj_df['Real_Prec_Pos'] = prec_traj_df['Position'].values

    # we don't know anything about the preceding yet, so almost everything is blank
    ego_traj_df.loc[0, 'Ego_Pos_Benign'] = -init_space_headway
    ego_traj_df.loc[0, 'Ego_Pos'] = -init_space_headway    
    ego_traj_df.loc[0, 'Ego_Vel_Benign'] = prec_traj_df.loc[0, 'Velocity']
    ego_traj_df.loc[0, 'Ego_Vel'] = prec_traj_df.loc[0, 'Velocity']

    # Initialize communication success column with zeros
    ego_traj_df['Communication_Success'] = 0
        
    return ego_traj_df


def Compute_CACC_Acc(prec_acc,
                     prec_vel,
                     prec_pos,
                     ego_vel,
                     ego_pos,
                     dT,
                     prev_ego_acc,
                     max_acc,
                     max_dec,
                     speed_limit,
                     min_speed):      

        ### Control Parameters ###
        K_a = 0.66
        K_v = 0.99
        K_g = 4.08
        G_min = 2.0
        CACC_thw = 0.55

        tau = 0.4   # parameter of the vehicle model

        ### Calculate safe space-gap ###
        gap = prec_pos - ego_pos   # Calculate the real gap
        #JOON: guessing we assume that the max_dec is the same for the ego and the preceding?
        safe_gap = 0.1*ego_vel + 0.5*(np.square(ego_vel) - np.square(prec_vel))/abs(max_dec) + 1.0  

        ### Calculate acceleration value of ego vehicle ###       
        if gap < safe_gap: 
            #### Collision Avoidance Mode ####  
            ###$####print('collision avoidance')          
            ego_acc = max_dec         
        else :         
            #### Gap Control Mode ####         
            ###$####print('gap control')
            acc_des = K_a * prec_acc + (K_v * (prec_vel - ego_vel)) + K_g * (gap - (ego_vel * CACC_thw) - G_min) # desired acceleration value
            ego_acc = (acc_des - prev_ego_acc)*dT/tau +prev_ego_acc    
        
        ### Use the min and max bounds to clip the acceleration ###    
        ego_acc = np.clip(ego_acc, a_min = max_dec, a_max = max_acc)
        next_ego_vel = ego_vel + ego_acc*dT
        if next_ego_vel > speed_limit:
            ego_acc = (speed_limit-ego_vel) / dT  
        elif next_ego_vel < min_speed:
            ego_acc = (min_speed-ego_vel) / dT  
            
        return ego_acc, safe_gap


def PlotDf(ego_traj_df, columns_to_plot, title, xlabel, ylabel):
      
    if columns_to_plot is not None:
        # Ensure only the specified columns are plotted
        plot_df = ego_traj_df[columns_to_plot].copy()
        plt.figure(figsize=(10, 6))
        
        # Define different markers for each line
        markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h', 'H', '+', 'x', '8']
        
        # Calculate marker spacing based on data length - make markers less dense
        data_length = len(plot_df)
        base_interval = max(20, data_length // 8)  # Increased interval for less dense markers
        
        # First pass: plot all lines with markers (except the first one)
        marker_index = 0
        for i, column in enumerate(plot_df.columns):
            if i == 0:
                # Skip the first plot for now, we'll plot it last to put it on top
                continue
            else:
                # Plot lines with markers, lower z-order
                marker = markers[marker_index % len(markers)]
                marker_index += 1
                
                # Plot the line first with lower z-order - increased linewidth
                line = plt.plot(plot_df.index, plot_df[column], label=column, linewidth=3, zorder=5)
                line_color = line[0].get_color()  # Get the color matplotlib assigned to this line
                
                # Create a list of marker positions that are offset for each line
                marker_positions = []
                start_offset = (marker_index - 1) * (base_interval // 4)  # Offset each line's markers
                
                for pos in range(start_offset, data_length, base_interval):
                    if pos < data_length:
                        marker_positions.append(pos)
                
                # Add markers at specific positions with the same color as the line - larger markers
                if marker_positions:
                    marker_x = [plot_df.index[pos] for pos in marker_positions]
                    marker_y = [plot_df[column].iloc[pos] for pos in marker_positions]
                    plt.scatter(marker_x, marker_y, marker=marker, s=120, color=line_color, zorder=6)
        
        # Second pass: plot the first line (no markers) with highest z-order to be on top - increased linewidth
        first_column = plot_df.columns[0]
        plt.plot(plot_df.index, plot_df[first_column], label=first_column, linewidth=3, zorder=10)
        
        # Only increase font sizes for legend and tick labels (no title or axis labels)
        plt.legend(fontsize=26)
        plt.xticks(fontsize=26)
        plt.yticks(fontsize=26)
        plt.grid(True)
        
        # Minimize margins
        plt.tight_layout()
        plt.subplots_adjust(left=0.06, right=0.98, top=0.98, bottom=0.08)
        
        # Create graphs directory if it doesn't exist
        os.makedirs("./graphs", exist_ok=True)
        plt.savefig(f"./graphs/{title}.png", bbox_inches='tight', pad_inches=0.1)
        print(f"Saved image to ./graphs/{title}.png")
        plt.show()


# The input 'data' should be a byte object which has 6 bytes
def dequantize(data):

    # convert the whole bytes object into a single large integer. Its value represents all bits of the byte sequence
    bitstream = int.from_bytes(data, byteorder='big')  

    # Extract longitudinal acc (9 bits, signed), vel (14 bits), pos (15 bits), counter (10 bits)
    # Bits: | 9 lon_accel | 14 vel | 15 pos | 10 counter |

    # Extract counter (10 bits, unsigned)
    counter = bitstream & 0x3FF
    # Shift right by 10 to get the next field
    bitstream >>= 10

    # Position (15 bits, unsigned)
    position = bitstream & 0x7FFF
    bitstream >>= 15

    # Velocity (14 bits, unsigned)
    velocity = bitstream & 0x3FFF
    bitstream >>= 14

    # LonAccel (9 bits, signed)
    lon_accel_raw = bitstream & 0x1FF

    # Convert lon_accel_raw (9 bits) to signed value (-160..161)
    if lon_accel_raw >= 256:  # 0x100
        lon_accel = lon_accel_raw - 512  # Two's complement conversion for 9 bits
    else:
        lon_accel = lon_accel_raw

    # Apply quantization units
    lon_accel = lon_accel * 0.1         # 0.1 m/s^2 unit
    velocity = velocity * 0.01          # 0.01 m/s unit
    position = position * 0.1           # 0.1 m unit

    return counter, lon_accel, velocity, position

if __name__ == '__main__':
    # define the path to the collected data
    file_name = "fifth"
    file_path = f"./data/{file_name}.dat"
    begin_index = 500
    data_length = 1000
    
    ### Get Dataframe of the leader from the data collected from RDS 1000 ###
    prec_traj_df = RDSDataCleanUp(file_path, begin_index, data_length)    
    
    ### Set the Parameters that the ego vehicle needs ###    
    speed_limit = 35.0
    min_speed = 0.5
    CACC_thw = 0.55
    init_space_headway = 20.0
    max_acc = 8.0
    max_dec = -8.0
    
    ### Initialize the Trajectory Dataframe of ego vehicle ###
    ego_traj_df = Ego_Traj_Df_Init(prec_traj_df, init_space_headway)
    
    # initialize counter and error tracking
    counter = 0
    prec_counter = 0
    error_indices = []
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Get the local IP address for binding
    host = '0.0.0.0'
    port = 65432
    server_socket.bind((host, port))
    print(f"EGO listening on {host}:{port}")
    
    # Store the sender address for sending ACKs during timeouts
    sender_address = ('10.0.0.2', 44495)  # Known preceding CAV address
    
    ### Start Simulation ###
    while True:        
        communication_success = 1  # Default: success (message received)
        
        # Initialize variables to avoid NameError
        prec_counter = counter
        prec_acc = 0.0
        prec_vel = 0.0
        prec_pos = 0.0
        
        # For counter == 0, we don't wait for data, just initialize
        if counter == 0:
            # Use initial values from the trajectory data
            prec_acc = prec_traj_df.at[0, 'LonAccel']
            prec_vel = prec_traj_df.at[0, 'Velocity']
            prec_pos = prec_traj_df.at[0, 'Position']
            print(f"Initial values - Counter: {counter}, Prec_Acc: {prec_acc}, Prec_Vel: {prec_vel}, Prec_Pos: {prec_pos}")
        else:
            server_socket.settimeout(0.30)
            print("Waiting for preceding to send data...")
            
            try:
                data, addr = server_socket.recvfrom(1024)
                print("Received Data!")
                communication_success = 1  # Message received successfully
                
                # Extract sender's IP and port
                sender_ip = addr[0]
                sender_port = addr[1]
                # Update sender address for future use
                sender_address = addr
                
                # Extract receiver's specific interface IP
                receiver_ip, receiver_port = server_socket.getsockname()
                print(f"Received data on interface bound to IP: {receiver_ip}, Port: {receiver_port}")
                print(f"Received data from IP: {sender_ip}, Port: {sender_port}")
                
                # decode packed bits
                prec_counter, prec_acc, prec_vel, prec_pos = dequantize(data)
                prec_acc = round(prec_acc, 1)
                prec_vel = round(prec_vel, 2)
                prec_pos = round(prec_pos, 1)
                print("Received quantized data:", prec_counter, prec_acc, prec_vel, prec_pos)
                
                ack = b'\x01'
                server_socket.sendto(ack, addr)
                
            except socket.timeout:
                # Handle the timeout exception
                print("Connection or receive operation timed out")
                communication_success = 0  # Communication failed
                
                # Set recorded values as per requirements
                prec_counter = counter
                if counter > 0:
                    prec_acc = ego_traj_df.at[counter - 1, 'Received_Prec_Acc']
                    prec_vel = ego_traj_df.at[counter - 1, 'Received_Prec_Vel']
                    prec_pos = ego_traj_df.at[counter - 1, 'Received_Prec_Pos']
                else:
                    prec_acc = 0.0
                    prec_vel = prec_traj_df.at[0, 'Velocity']
                    prec_pos = prec_traj_df.at[0, 'Position']
                
                # Send failure ACK to known sender address
                ack = b'\x00'
                server_socket.sendto(ack, sender_address)
                print(f"Sent failure ACK to {sender_address}")
                error_indices.append(counter)
        
        # Store communication success status
        ego_traj_df.loc[counter, 'Communication_Success'] = communication_success
        
        # if preceding ends the connection, break out
        if prec_counter >= 1022 or counter >= len(prec_traj_df):
            break
            
        # cap values in a reasonable range
        prec_acc = min(max(-8, prec_acc), 8)
        prec_vel = min(max(-50, prec_vel), 50)
        print(f"Counter: {prec_counter}, Prec_Acc: {prec_acc}, Prec_Vel: {prec_vel}, Prec_Pos: {prec_pos}\n")
        
        # Get real data from the original trajectory
        real_prec_acc = prec_traj_df.at[counter, 'LonAccel']
        real_prec_vel = prec_traj_df.at[counter, 'Velocity']
        real_prec_pos = prec_traj_df.at[counter, 'Position']
        
        # Store received data (potentially under attack)
        ego_traj_df.loc[counter, 'Received_Prec_Acc'] = prec_acc
        ego_traj_df.loc[counter, 'Received_Prec_Vel'] = prec_vel
        ego_traj_df.loc[counter, 'Received_Prec_Pos'] = prec_pos
        
        ### Read the states of the Ego CAV at current time step ###
        ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
        dT = ego_traj_df.loc[counter, 'dT']
        
        ### Read the states of both the Preceding and Ego CAV at the previous time step ###
        if counter == 0:
            ego_vel = prec_vel
            ego_vel_benign = prec_vel
            ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
            ego_pos_benign = ego_traj_df.loc[counter, 'Ego_Pos_Benign']
            prev_ego_acc = 0
            prev_ego_acc_benign = 0
        else:
            ego_vel = ego_traj_df.loc[counter, 'Ego_Vel']
            ego_vel_benign = ego_traj_df.loc[counter, 'Ego_Vel_Benign']
            ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
            ego_pos_benign = ego_traj_df.loc[counter, 'Ego_Pos_Benign']
            prev_ego_acc = ego_traj_df.loc[counter-1, 'Ego_Acc']
            prev_ego_acc_benign = ego_traj_df.loc[counter-1, 'Ego_Acc_Benign']
        
        ### Calculate Benign Ego_Acc (using real preceding data) ###
        ego_acc_benign, safe_gap_benign = Compute_CACC_Acc(real_prec_acc,
                                                            real_prec_vel,
                                                            real_prec_pos,
                                                            ego_vel_benign,
                                                            ego_pos_benign,
                                                            dT,
                                                            prev_ego_acc_benign,
                                                            max_acc,
                                                            max_dec,
                                                            speed_limit,
                                                            min_speed)
        
        ### Calculate Ego_Acc based on communication success ###
        if communication_success == 0:
            # Communication failed: keep previous acceleration
            print(f"Using previous acceleration due to communication failure.")
            ego_acc = prev_ego_acc
            
            # Calculate safe_gap using current states
            safe_gap = 0.1 * ego_vel + 0.5 * (np.square(ego_vel) - np.square(prec_vel)) / abs(max_dec) + 1.0
            
        else:
            # Normal operation: calculate using CACC algorithm
            ego_acc, safe_gap = Compute_CACC_Acc(prec_acc,
                                                prec_vel,
                                                prec_pos,
                                                ego_vel,
                                                ego_pos,
                                                dT,
                                                prev_ego_acc,
                                                max_acc,
                                                max_dec,
                                                speed_limit,
                                                min_speed)
        
        # Benign trajectory
        next_ego_vel_benign = ego_vel_benign + ego_acc_benign * dT
        next_ego_pos_benign = ego_pos_benign + ego_vel_benign * dT + 0.5 * ego_acc_benign * (dT**2)
        
        ### Compute the velocity and position state of ego CAV at next time step ###
        next_ego_vel = ego_vel + ego_acc * dT
        next_ego_pos = ego_pos + ego_vel * dT + 0.5 * ego_acc * (dT**2)
        
        # Store current time step data
        ego_traj_df.loc[counter, 'Ego_Acc_Benign'] = ego_acc_benign
        ego_traj_df.loc[counter, 'Ego_Acc'] = ego_acc
        ego_traj_df.loc[counter, 'Safe_Gap'] = safe_gap
        
        # Store next time step data if not the last iteration
        if counter + 1 < len(ego_traj_df):
            ego_traj_df.loc[counter + 1, 'Ego_Vel_Benign'] = next_ego_vel_benign
            ego_traj_df.loc[counter + 1, 'Ego_Pos_Benign'] = next_ego_pos_benign
            ego_traj_df.loc[counter + 1, 'Ego_Vel'] = next_ego_vel
            ego_traj_df.loc[counter + 1, 'Ego_Pos'] = next_ego_pos
        
        # Calculate gaps and time headways
        ego_traj_df.loc[counter, 'Real_Gap'] = real_prec_pos - ego_pos

        ego_traj_df.loc[counter, 'Space_Headway_Benign'] = ego_traj_df.at[counter, 'Real_Prec_Pos'] - ego_traj_df.at[counter, 'Ego_Pos_Benign']
        ego_traj_df.loc[counter, 'Space_Headway'] = ego_traj_df.at[counter, 'Real_Prec_Pos'] - ego_traj_df.at[counter, 'Ego_Pos']     

        # Time headways
        ego_traj_df.loc[counter, 'Time_Headway_Benign'] = ego_traj_df.loc[counter, 'Space_Headway_Benign'] / ego_traj_df.at[counter, 'Ego_Vel_Benign'] if ego_traj_df.at[counter, 'Ego_Vel_Benign'] > 0 else 0
        ego_traj_df.loc[counter, 'Time_Headway'] = ego_traj_df.loc[counter, 'Space_Headway'] / ego_traj_df.at[counter, 'Ego_Vel'] if ego_traj_df.at[counter, 'Ego_Vel'] > 0 else 0
        
        # Increment the counter
        counter += 1
    
    # Rest of the code remains the same...
    ego_traj_df = ego_traj_df.head(-1)
    server_socket.close()
    print("Connection ended.")
    print("Connection Fail Rate:", len(error_indices)/len(ego_traj_df.index))
    
    ego_traj_df['Desired_Space_HW'] = ego_traj_df['Ego_Vel'] * 0.55
    
    # Plotting according to your requirements
    PlotDf(ego_traj_df, columns_to_plot=['Real_Prec_Acc', 'Received_Prec_Acc', 'Ego_Acc_Benign', 'Ego_Acc'],
           title=f'{file_name}_CACC_Acceleration', xlabel='Time Step', ylabel='Acceleration (m/s²)')
    
    PlotDf(ego_traj_df, columns_to_plot=['Real_Prec_Vel', 'Received_Prec_Vel', 'Ego_Vel_Benign', 'Ego_Vel'],
           title=f'{file_name}_CACC_Velocity',  xlabel='Time Step', ylabel='Velocity (m/s)')
    
    PlotDf(ego_traj_df, columns_to_plot=['Real_Prec_Pos', 'Received_Prec_Pos', 'Ego_Pos_Benign', 'Ego_Pos'],
           title=f'{file_name}_CACC_Position',  xlabel='Time Step', ylabel='Position (m)')
    
    PlotDf(ego_traj_df, columns_to_plot=['Time_Headway_Benign', 'Time_Headway'],
           title=f'{file_name}_CACC_Time_Headway', xlabel='Time Step', ylabel='Time Headway (s)')
    
    PlotDf(ego_traj_df, columns_to_plot=['Space_Headway_Benign', 'Space_Headway'],
           title=f'{file_name}_CACC_Space_Headway', xlabel='Time Step', ylabel='Gap (m)')
    
    # Plot communication success using the same PlotDf function
    PlotDf(ego_traj_df, columns_to_plot=['Communication_Success'],
           title=f'{file_name}_Communication_Success', xlabel='Message Number', ylabel='Communication Status (0=Failed, 1=Success)')
    
    # Save the final dataframe
    ego_traj_df.to_csv('/tmp/ego_traj.csv', index=False)
    print("Data saved to /tmp/ego_traj.csv")

# if __name__ == '__main__':
    
#     # define the path to the collected data
#     file_name = "fifth"
#     file_path = f"./data/{file_name}.dat"
#     begin_index = 500
#     data_length = 1000
    
#     ### Get Dataframe of the leader from the data collected from RDS 1000 ###
#     prec_traj_df = RDSDataCleanUp(file_path, begin_index, data_length)    
    
#     ### Set the Parameters that the ego vehicle needs ###    
#     speed_limit = 35.0
#     min_speed = 0.5
#     CACC_thw = 0.55
#     init_space_headway = 20.0
#     max_acc = 8.0
#     max_dec = -8.0
    
#     ### Initialize the Trajectory Dataframe of ego vehicle ###
#     ego_traj_df = Ego_Traj_Df_Init(prec_traj_df, init_space_headway)
    
#     # initialize counter and error tracking
#     counter = 0
#     prec_counter = 0
#     consecutive_errors = 0  # Track consecutive communication errors
#     max_consecutive_errors = 5  # Preset threshold
#     error_indices = []
    
#     server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#     # Get the local IP address for binding
#     host = '0.0.0.0'
#     port = 65432
#     server_socket.bind((host, port))
#     print(f"EGO listening on {host}:{port}")
    
#     # Store the sender address for sending ACKs during timeouts
#     sender_address = ('10.0.0.2', 44495)  # Known preceding CAV address
    
#     ### Start Simulation ###
#     while True:        
#         communication_error = 0  # Default: no error (message received)
        
#         # Initialize variables to avoid NameError
#         prec_counter = counter
#         prec_acc = 0.0
#         prec_vel = 0.0
#         prec_pos = 0.0
        
#         # For counter == 0, we don't wait for data, just initialize
#         if counter == 0:
#             # Use initial values from the trajectory data
#             prec_acc = prec_traj_df.at[0, 'LonAccel']
#             prec_vel = prec_traj_df.at[0, 'Velocity'] 
#             prec_pos = prec_traj_df.at[0, 'Position']
#             print(f"Initial values - Counter: {counter}, Prec_Acc: {prec_acc}, Prec_Vel: {prec_vel}, Prec_Pos: {prec_pos}")
#         else:
#             server_socket.settimeout(0.30)
#             print("Waiting for preceding to send data...")
            
#             try:
#                 data, addr = server_socket.recvfrom(1024)
#                 print("Received Data!")
#                 # Reset consecutive error count on successful communication
#                 consecutive_errors = 0
#                 communication_error = 0  # Message received successfully
                
#                 # Extract sender's IP and port
#                 sender_ip = addr[0]
#                 sender_port = addr[1]
#                 # Update sender address for future use
#                 sender_address = addr
                
#                 # Extract receiver's specific interface IP
#                 receiver_ip, receiver_port = server_socket.getsockname()
#                 print(f"Received data on interface bound to IP: {receiver_ip}, Port: {receiver_port}")
#                 print(f"Received data from IP: {sender_ip}, Port: {sender_port}")
                
#                 # decode packed bits
#                 prec_counter, prec_acc, prec_vel, prec_pos = dequantize(data)
#                 prec_acc = round(prec_acc, 1)
#                 prec_vel = round(prec_vel, 2)
#                 prec_pos = round(prec_pos, 1)
#                 print("Received quantized data:", prec_counter, prec_acc, prec_vel, prec_pos)
                
#                 ack = b'\x01'
#                 server_socket.sendto(ack, addr)
                
#             except socket.timeout:
#                 # Handle the timeout exception
#                 print("Connection or receive operation timed out")
#                 consecutive_errors += 1
#                 print(f"Consecutive errors: {consecutive_errors}")
#                 communication_error = 1  # Communication error occurred
                
#                 # Set recorded values as per requirements
#                 prec_counter = counter
#                 if counter > 0:
#                     prec_acc = ego_traj_df.at[counter - 1, 'Received_Prec_Acc']
#                     prec_vel = ego_traj_df.at[counter - 1, 'Received_Prec_Vel']
#                     prec_pos = ego_traj_df.at[counter - 1, 'Received_Prec_Pos']
#                 else:
#                     prec_acc = 0.0
#                     prec_vel = prec_traj_df.at[0, 'Velocity']
#                     prec_pos = prec_traj_df.at[0, 'Position']
                
#                 # Send failure ACK to known sender address
#                 ack = b'\x00'
#                 server_socket.sendto(ack, sender_address)
#                 print(f"Sent failure ACK to {sender_address}")
#                 error_indices.append(counter)
        
#         # Store communication error status
#         ego_traj_df.loc[counter, 'Communication_Error'] = communication_error
           
#         # if preceding ends the connection, break out
#         if prec_counter >= 1022 or counter >= len(prec_traj_df):
#             break
        
#         # cap values in a reasonable range
#         prec_acc = min(max(-8, prec_acc), 8)
#         prec_vel = min(max(-50, prec_vel), 50)
        
#         print(f"Counter: {prec_counter}, Prec_Acc: {prec_acc}, Prec_Vel: {prec_vel}, Prec_Pos: {prec_pos}\n")

#         # Get real data from the original trajectory
#         real_prec_acc = prec_traj_df.at[counter, 'LonAccel']
#         real_prec_vel = prec_traj_df.at[counter, 'Velocity']
#         real_prec_pos = prec_traj_df.at[counter, 'Position']

#         # Store received data (potentially under attack)
#         ego_traj_df.loc[counter, 'Received_Prec_Acc'] = prec_acc
#         ego_traj_df.loc[counter, 'Received_Prec_Vel'] = prec_vel
#         ego_traj_df.loc[counter, 'Received_Prec_Pos'] = prec_pos
        
#         ### Read the states of the Ego CAV at current time step ###
#         ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
#         dT = ego_traj_df.loc[counter, 'dT']
        
#         ### Read the states of both the Preceding and Ego CAV at the previous time step ###
#         if counter == 0:
#             ego_vel = prec_vel
#             ego_vel_benign = prec_vel
#             ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
#             ego_pos_benign = ego_traj_df.loc[counter, 'Ego_Pos_Qbenign']
#             prev_ego_acc = 0
#             prev_ego_acc_benign = 0
#         else:
#             ego_vel = ego_traj_df.loc[counter, 'Ego_Vel']
#             ego_vel_benign = ego_traj_df.loc[counter, 'Ego_Vel_Qbenign']
#             ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
#             ego_pos_benign = ego_traj_df.loc[counter, 'Ego_Pos_Qbenign']
#             prev_ego_acc = ego_traj_df.loc[counter-1, 'Ego_Acc']
#             prev_ego_acc_benign = ego_traj_df.loc[counter-1, 'Ego_Acc_Qbenign']


#         ### Calculate Benign Ego_Acc (using real preceding data) ###
#         ego_acc_benign, safe_gap_benign = Compute_CACC_Acc(real_prec_acc,
#                                                            real_prec_vel,
#                                                            real_prec_pos,
#                                                            ego_vel_benign,
#                                                            ego_pos_benign,
#                                                            dT,
#                                                            prev_ego_acc_benign,
#                                                            max_acc,
#                                                            max_dec,
#                                                            speed_limit,
#                                                            min_speed)
        
#         ### Calculate Ego_Acc based on consecutive error count ###
#         if consecutive_errors > 0 and consecutive_errors <= max_consecutive_errors:
#             # Less than or equal to preset value: keep previous acceleration
#             print(f"Using previous acceleration due to consecutive errors {consecutive_errors} less than the threshold.")
#             ego_acc = prev_ego_acc

#             # Calculate safe_gap using current states
#             safe_gap = 0.1 * ego_vel + 0.5 * (np.square(ego_vel) - np.square(prec_vel)) / abs(max_dec) + 1.0

#         elif consecutive_errors > max_consecutive_errors:
#             # More than preset value: apply maximum deceleration
#             print(f"Applying maximum deceleration due to {consecutive_errors} consecutive errors")
#             ego_acc = max_dec
            
#             # Calculate safe_gap using current states
#             safe_gap = 0.1 * ego_vel + 0.5 * (np.square(ego_vel) - np.square(prec_vel)) / abs(max_dec) + 1.0

#         else:
#             # Normal operation: calculate using CACC algorithm
#             ego_acc, safe_gap = Compute_CACC_Acc(prec_acc,
#                                                 prec_vel,
#                                                 prec_pos,
#                                                 ego_vel,
#                                                 ego_pos,
#                                                 dT,
#                                                 prev_ego_acc,
#                                                 max_acc,
#                                                 max_dec,
#                                                 speed_limit,
#                                                 min_speed)
        
#         # Benign trajectory
#         next_ego_vel_benign = ego_vel_benign + ego_acc_benign * dT
#         next_ego_pos_benign = ego_pos_benign + ego_vel_benign * dT + 0.5 * ego_acc_benign * (dT**2)
                
#         ### Compute the velocity and position state of ego CAV at next time step ###
#         next_ego_vel = ego_vel + ego_acc * dT
#         next_ego_pos = ego_pos + ego_vel * dT + 0.5 * ego_acc * (dT**2)

#         # Store current time step data
#         ego_traj_df.loc[counter, 'Ego_Acc_Qbenign'] = ego_acc_benign
#         ego_traj_df.loc[counter, 'Ego_Acc'] = ego_acc
#         ego_traj_df.loc[counter, 'Safe_Gap'] = safe_gap

#         # Store next time step data if not the last iteration
#         if counter + 1 < len(ego_traj_df):
#             ego_traj_df.loc[counter + 1, 'Ego_Vel_Qbenign'] = next_ego_vel_benign
#             ego_traj_df.loc[counter + 1, 'Ego_Pos_Qbenign'] = next_ego_pos_benign
#             ego_traj_df.loc[counter + 1, 'Ego_Vel'] = next_ego_vel
#             ego_traj_df.loc[counter + 1, 'Ego_Pos'] = next_ego_pos
        
#         # Calculate gaps and time headways
#         ego_traj_df.loc[counter, 'Real_Gap'] = real_prec_pos - ego_pos
#         ego_traj_df.loc[counter, 'Space_Headway_Qbenign'] = real_prec_pos - ego_pos_benign
#         ego_traj_df.loc[counter, 'Space_Headway'] = prec_pos - ego_pos

#         if ego_vel_benign > 0:
#             ego_traj_df.loc[counter, 'Time_Headway_Qbenign'] = ego_traj_df.loc[counter, 'Space_Headway_Qbenign'] / ego_vel_benign
#         else:
#             ego_traj_df.loc[counter, 'Time_Headway_Qbenign'] = float('inf')
            
#         if ego_vel > 0:
#             ego_traj_df.loc[counter, 'Time_Headway'] = ego_traj_df.loc[counter, 'Space_Headway'] / ego_vel
#         else:
#             ego_traj_df.loc[counter, 'Time_Headway'] = float('inf')
        
        
#         # Increment the counter
#         counter += 1
    
#     # Rest of the code remains the same...
#     ego_traj_df = ego_traj_df.head(-1)
    
#     server_socket.close()
#     print("Connection ended.")
    
#     print("Connection Fail Rate:", len(error_indices)/len(ego_traj_df.index))
    
#     ego_traj_df['Desired_Space_HW'] = ego_traj_df['Ego_Vel'] * 0.55
    
#     # Plotting according to your requirements
#     PlotDf(ego_traj_df, columns_to_plot=['Real_Prec_Acc', 'Ego_Acc_Qbenign', 'Ego_Acc'], 
#            title=f'{file_name}_CACC_Real_Acceleration', xlabel='Time Step', ylabel='Acceleration (m/s²)')
    
#     PlotDf(ego_traj_df, columns_to_plot=['Real_Prec_Vel', 'Ego_Vel_Qbenign', 'Ego_Vel'], 
#            title=f'{file_name}_CACC_Real_Velocity',  xlabel='Time Step', ylabel='Velocity (m/s)')
    
#     PlotDf(ego_traj_df, columns_to_plot=['Real_Prec_Pos', 'Ego_Pos_Qbenign', 'Ego_Pos'], 
#            title=f'{file_name}_CACC_Real_Position',  xlabel='Time Step', ylabel='Position (m)')
    
#     PlotDf(ego_traj_df, columns_to_plot=['Time_Headway_Qbenign', 'Time_Headway'], 
#            title=f'{file_name}_CACC_Time_Headway', xlabel='Time Step', ylabel='Time Headway (s)')
    
#     PlotDf(ego_traj_df, columns_to_plot=['Space_Headway_Qbenign', 'Space_Headway'], 
#            title=f'{file_name}_CACC_Real_Gap', xlabel='Time Step', ylabel='Gap (m)')
    
#     # Plot communication errors using the same PlotDf function
#     PlotDf(ego_traj_df, columns_to_plot=['Communication_Error'], 
#            title=f'{file_name}_Communication_Errors', xlabel='Message Number', ylabel='Communication Status (0=Received, 1=Error)')
    
#     # Save the final dataframe
#     ego_traj_df.to_csv('/tmp/ego_traj.csv', index=False)
#     print("Data saved to /tmp/ego_traj.csv")