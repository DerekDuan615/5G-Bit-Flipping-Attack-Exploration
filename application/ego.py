import socket
import struct
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math

from os.path import join as join
import json

data_length = 1500

def RDSDataCleanUp(file_path):
    
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
    begin_index = 500                      # Set it to the value you want
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
                                            'Prec_Acc',                                        
                                            'Prec_Vel',
                                            'Prec_Pos',
                                            'Ego_Acc',
                                            'Ego_Vel', 
                                            'Ego_Pos',                                               
                                            'Space_Headway',
                                            'Time_Headway',
                                            'Safe_Gap',
                                            'Rel_Vel',
                                            'Rel_Acc'])

    ego_traj_df['SimTime'] = prec_traj_df['SimTime'].values     # the number of rows in both DataFrames should match automatically
    ego_traj_df['dT'] = prec_traj_df['dT'].values 
    # JOON: now we assume multiple channels (acc, vel, pos)
    # ego_traj_df['Prec_Vel'] = prec_traj_df['Velocity'].values 
    # ego_traj_df['Prec_Pos'] = prec_traj_df['Position'].values 

    # we don't know anything about the preceding yet, so almost everything is blank
    ego_traj_df.loc[0, 'Ego_Pos'] = -init_space_headway
        
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
            ego_traj_df = ego_traj_df[columns_to_plot]
        
        plt.figure(figsize=(10, 6))        
        for column in ego_traj_df.columns:
            plt.plot(ego_traj_df.index, ego_traj_df[column], label=column)
        
        # plt.title(title)
        # plt.xlabel(xlabel)
        # plt.ylabel(ylabel)
        plt.legend(fontsize=16)
        plt.xticks(size=16)
        plt.yticks(size=16)
        plt.grid(True)
        plt.show()

        plt.savefig(f"./graphs/{title}.png")
        print(f"Saved image to ./graphs/{title}.png")

# error_indices = []

if __name__ == '__main__':

    # define the path to the collected data
    file_name = "fifth"
    file_path = f"./data/{file_name}.dat"

    ### Get Dataframe of the leader from the data collected from RDS 1000 ###
    prec_traj_df = RDSDataCleanUp(file_path)

    ### Set the Parameters that the ego vehicle needs ###    
    speed_limit = 35.0
    #JOON: the original paper implementation does not take into account stopping
    min_speed = 0.5
    CACC_thw = 0.55
    init_space_headway = 20.0
    max_acc = 8.0
    max_dec = -8.0

    error_indices = []

    ### Initialize the Trajectory Dataframe of ego vehicle ###
    # we will not have the entire trajectory at first
    ego_traj_df = Ego_Traj_Df_Init(prec_traj_df, init_space_headway)
    # import ipdb; ipdb.set_trace()

    # initialize counter
    counter = 0

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Get the local IP address for binding
    host = '0.0.0.0'  # Listen on all available interfaces
    port = 65432      # Port to listen on

    server_socket.bind((host, port))
    print(f"EGO listening on {host}:{port}")

    ### Start Simulation ###
    while True:        
        ### Read the states of the Preceding CAV at current time step ###
        #TODO: this will be where communication happens
        # prec_acc = prec_traj_df.loc[counter, 'LonAccel']
        # prec_vel = prec_traj_df.loc[counter, 'Velocity']
        # prec_pos = prec_traj_df.loc[counter, 'Position']
        # dT = prec_traj_df.loc[counter, 'dT']
        # receive data from preceding

        #JOON: this might need to change if additional overhead is introduced
        if counter > 0:
            server_socket.settimeout(0.30)

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
            prec_counter, prec_acc, prec_vel, prec_pos = received_data
            print("Recived data:", received_data)

            # ack = (1,0,0,0)
            # # print(data_to_send)
            # serialized_ack = struct.pack('!4i', *ack)
            # server_socket.sendto(serialized_ack, addr)
            ack = b'\x01'
            server_socket.sendto(ack, addr)

        except socket.timeout:
            # Handle the timeout exception
            print("Connection or receive operation timed out")
            print("Using previous step's acceleration value")
            prec_counter = counter
            # fill out the missing data at this timestep as best as possible
            prec_acc = ego_traj_df.at[counter - 1, 'Prec_Acc']
            # prec_vel = ego_traj_df.at[counter, 'Prec_Vel']
            # prec_pos = ego_traj_df.at[counter, 'Prec_Pos']
            # JOON: since we now do not know vel and pos, we have to approximate with kinematics
            prec_vel = ego_traj_df.at[counter - 1, 'Prec_Vel'] + prec_acc * ego_traj_df.at[counter - 1, 'dT']        
            prec_pos = ego_traj_df.at[counter - 1, 'Prec_Pos'] + \
                       ego_traj_df.at[counter - 1, 'Prec_Vel'] * ego_traj_df.at[counter - 1, 'dT'] + \
                       0.5 * prec_acc * (ego_traj_df.at[counter - 1, 'dT']**2)

            # ack = (0,0,0,0)
            # # print(data_to_send)
            # serialized_ack = struct.pack('!4i', *ack)
            # server_socket.sendto(serialized_ack, addr)

            ack = b'\x00'
            server_socket.sendto(ack, addr)

            error_indices.append(counter)

        # if preceding ends the connection, break out
        if prec_counter == -1 or counter == len(prec_traj_df):
            break

        # cap values in a reasonable range
        prec_acc = min(max(-8, prec_acc), 8)
        prec_vel = min(max(-50, prec_vel), 50)

        print(f"Counter: {prec_counter}, Prec_Acc: {prec_acc}, Prec_Vel: {prec_vel}, Prec_Pos: {prec_pos}\n")

        # first, fill in the new information provided by the preceding at this timestep
        ego_traj_df.at[counter, 'Prec_Acc'] = prec_acc
        ego_traj_df.at[counter, 'Prec_Vel'] = prec_vel
        ego_traj_df.at[counter, 'Prec_Pos'] = prec_pos


        ### Read the states of the Ego CAV at current time step ###
        ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
        dT = ego_traj_df.loc[counter, 'dT']

        ### Read the states of both the Preceding and Ego CAV at the previous time step ### 
        if counter == 0:              
            ego_vel = prec_vel
            ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
            prev_ego_acc = 0
            prev_ego_vel = ego_traj_df.loc[0, 'Ego_Vel']
            prev_ego_pos = ego_traj_df.loc[0, 'Ego_Pos']
            prev_prec_acc = ego_traj_df.loc[0, 'Prec_Acc']
            prev_prec_vel = ego_traj_df.loc[0, 'Prec_Vel']
            prev_prec_pos = ego_traj_df.loc[0, 'Prec_Pos']            
            prev_dT = 0
        else:
            ego_vel = ego_traj_df.loc[counter, 'Ego_Vel']
            prev_ego_acc = ego_traj_df.loc[counter-1, 'Ego_Acc']
            prev_ego_vel = ego_traj_df.loc[counter-1, 'Ego_Vel']
            prev_ego_pos = ego_traj_df.loc[counter-1, 'Ego_Pos']
            prev_prec_acc = ego_traj_df.loc[counter-1, 'Prec_Acc']
            prev_prec_vel = ego_traj_df.loc[counter-1, 'Prec_Vel']
            prev_prec_pos = ego_traj_df.loc[counter-1, 'Prec_Pos']              
            prev_dT = ego_traj_df.loc[counter-1, 'dT'] 

        ### Calculate Ego_Acc at the current time step ###
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
      
        ### Compute the velocity and position state of ego CAV at next time step, and store in its Dataframe ###
        next_ego_vel = ego_vel + ego_acc * dT        
        next_ego_pos = ego_pos + ego_vel * dT + 0.5 * ego_acc * (dT**2)

        ego_traj_df.loc[counter, 'Ego_Acc'] = ego_acc
        ego_traj_df.loc[counter + 1, 'Ego_Vel'] = next_ego_vel
        ego_traj_df.loc[counter + 1, 'Ego_Pos'] = next_ego_pos
        ego_traj_df.loc[counter, 'Safe_Gap'] = safe_gap
        ego_traj_df.loc[counter + 1, 'Rel_Vel'] = next_ego_vel - ego_vel
        ego_traj_df.loc[counter, 'Rel_Acc'] = ego_acc - prev_ego_acc
        # JOON: has to calculate based on real data not the corrupted one
        # ego_traj_df.loc[counter, 'Real_Gap'] = prec_pos - ego_pos 
        # ego_traj_df.loc[counter, 'Time_Headway'] = (prec_pos - ego_pos) / ego_vel  
        real_prec_pos = prec_traj_df.at[counter, 'Position']
        ego_traj_df.loc[counter, 'Real_Gap'] = real_prec_pos - ego_pos 
        ego_traj_df.loc[counter, 'Time_Headway'] = (prec_pos - ego_pos) / ego_vel  
        ego_traj_df.loc[counter, 'Real_Time_Headway'] = (real_prec_pos - ego_pos) / ego_vel

        # ### Increment the counter ###  
        counter += 1   

    # remove the last row, implementation detail
    ego_traj_df = ego_traj_df.head(-1)

    server_socket.close()
    print("Connection ended.")

    print("Connection Fail Rate:", len(error_indices)/len(ego_traj_df.index))
    # print("Failed Indices:", error_indices)

    # this represents the desired space headway imposed by the time headway constant
    # there will be some constant gap between this value and the real gap due to G_min
    ego_traj_df['Desired_Space_HW'] = ego_traj_df['Ego_Vel'] * 0.55
    ego_traj_df['Real_Prec_Acc'] = prec_traj_df['LonAccel']
    ego_traj_df['Real_Prec_Vel'] = prec_traj_df['Velocity']
    ego_traj_df['Real_Prec_Pos'] = prec_traj_df['Position']
    
    ### Necessary Plots ###
    # PlotDf(ego_traj_df, columns_to_plot = ['Prec_Pos', 'Ego_Pos'], title = f'{file_name}_CACC_Position', xlabel='Time Step', ylabel='Position')
    # PlotDf(ego_traj_df, columns_to_plot = ['Prec_Vel', 'Ego_Vel'], title = f'{file_name}_CACC_Velocity', xlabel='Time Step', ylabel='Velocity')
    # PlotDf(ego_traj_df, columns_to_plot = ['Prec_Acc', 'Ego_Acc'], title = f'{file_name}_CACC_Acceleration', xlabel='Time Step', ylabel='Acceleration')
    PlotDf(ego_traj_df, columns_to_plot = ['Time_Headway', 'Real_Time_Headway'], title = f'{file_name}_CACC_Time_Headway', xlabel='Time Step', ylabel='Time Headway')
    PlotDf(ego_traj_df, columns_to_plot = ['Real_Gap', 'Safe_Gap', 'Desired_Space_HW'], title = f'{file_name}_CACC_Real_Gap', xlabel='Time Step', ylabel='Real Gap')
    PlotDf(ego_traj_df, columns_to_plot = ['Prec_Acc', 'Ego_Acc', 'Real_Prec_Acc'], title = f'{file_name}_CACC_Real_Acceleration', xlabel='Time Step', ylabel='Acceleration')
    PlotDf(ego_traj_df, columns_to_plot = ['Prec_Pos', 'Ego_Pos', 'Real_Prec_Pos'], title = f'{file_name}_CACC_Real_Position', xlabel='Time Step', ylabel='Position')
    PlotDf(ego_traj_df, columns_to_plot = ['Prec_Vel', 'Ego_Vel', 'Real_Prec_Vel'], title = f'{file_name}_CACC_Real_Velocity', xlabel='Time Step', ylabel='Velocity')

    # save_lists = {'prec_pos': prec_traj_df['Position'],
    #               'prec_vel': prec_traj_df['Velocity'],
    #               'prec_acc': prec_traj_df['LonAccel'],
    #               'real_prec_pos': ego_traj_df['Prec_Pos'],
    #               'real_prec_vel': ego_traj_df['Prec_Vel'],
    #               'real_prec_acc': ego_traj_df['Prec_Acc'],
    #               'ego_pos': ego_traj_df['Ego_Pos'],
    #               'ego_vel': ego_traj_df['Ego_Vel'],
    #               'ego_acc': ego_traj_df['Ego_Acc'],
    #               'real_gap': ego_traj_df['Real_Gap'],
    #               'safe_gap': ego_traj_df['Safe_Gap'],
    #               'space_headway': ego_traj_df['Desired_Space_HW'],
    #               'time_headway': ego_traj_df['Time_Headway'],
    #               }

    # file_name = 'vel_exp.json'
    # with open(file_name, 'w') as f:
    #     json.dump(save_lists, f)