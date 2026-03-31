import socket
import struct
import numpy as np
import pandas as pd


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

    #JOON: trivial dataframe for attack simulation

    # import ipdb; ipdb.set_trace()

    # prec_traj_df = pd.DataFrame(columns=prec_traj_df.columns)

    # prec_traj_df.loc[0] = [100.0, 0.0, 2.0, 0.1, 0.0]
    # print(prec_traj_df)

    return prec_traj_df


# Quantize the CAV states and concatenate them based on unaligned packed encoding rule (UPER).
# After checksum: 9-bits for acc (signed, unit: 0.1 m/s^2), 14-bits for vel (unsigned, unit: 0.01m/s), 15-bits for position (unsigned, unit: 0.1m), 10-bit for counter
def quantize(lon_accel, velocity, position, counter):
    # Quantize fields
    # 9 bits signed for lon_accel, range -160..161, unit 0.1 m/s^2
    q_lon_accel = int(round(lon_accel / 0.1))    # if lon_accel = 10.32, q_lon_accel would be 103
    if q_lon_accel < -160:
        q_lon_accel = -160
    elif q_lon_accel > 161:
        q_lon_accel = 161

    # Correct quantization
    if q_lon_accel < 0:
        q_lon_accel_unsigned = q_lon_accel + 512  # negative -160 to -1 stored as 352-511
    else:
        q_lon_accel_unsigned = q_lon_accel        # positive 0 to 161 stored as 0-161

    q_lon_accel_raw = q_lon_accel_unsigned & 0x1FF  # keep only the lowest 9 bits, 

    # 14 bits unsigned for velocity, unit 0.01 m/s, range 0..16383
    q_velocity = int(round(velocity / 0.01))
    q_velocity = max(0, min(q_velocity, 16383))

    # 15 bits unsigned for position, unit 0.1 m, range 0..32767
    q_position = int(round(position / 0.1))
    q_position = max(0, min(q_position, 32767))

    # 10 bits for counter, range 0..1023
    q_counter = counter & 0x3FF  # keep only the lowest 10 bits

    # Now, pack the bits unaligned: 9+14+15+10=48 bits (6 bytes)
    bitstream = (q_lon_accel_raw << (14+15+10)) | (q_velocity << (15+10)) | (q_position << 10) | q_counter

    data_bytes = bitstream.to_bytes(6, byteorder='big')  # 48 bits == 6 bytes, 'data_bytes' is a byte object
    return data_bytes

if __name__ == '__main__':
    # define the path to the collected data
    # You can change any file from 'first.dat' to 'sixth.dat' in 'application/data'.
    # Should only need little adjustment for other preceding trajectory data from RDS 1000
    file_name = "fifth" 
    file_path = f"./data/{file_name}.dat"
    begin_index = 500
    data_length = 1000
    
    ### Get Dataframe of the leader from the data collected from RDS 1000 ###
    prec_traj_df = RDSDataCleanUp(file_path, begin_index, data_length)    
    
    # Create a socket object
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    specific_address = ('10.0.0.2', 44495)  # IP address of UE assigned by CN, might be changed, check with command 'ipconfig'
    ego_address = ('192.168.70.135', 65432) # Fixed IP address
    
    client_socket.bind(specific_address)

    # Send preceding CAV's states:
    for counter in range(0, len(prec_traj_df)):
        row = prec_traj_df.iloc[counter]
        data_to_send = quantize(row['LonAccel'], row['Velocity'], row['Position'], counter)
        client_socket.sendto(data_to_send, ego_address)
        print("Sent data for timestep", counter)

        data, addr = client_socket.recvfrom(1024)
        if data == b'\x00':
            print("Connection Failed!")

    # end_data = (-1,0,0,0)
    # serialized_end_data = struct.pack('!4f', *end_data)
    # client_socket.sendto(serialized_end_data, ego_address)

    end_counter = 1022
    data_to_send = quantize(0, 0, 0, end_counter)
    client_socket.sendto(data_to_send, ego_address)
    client_socket.close()
    print("Communication ended.")