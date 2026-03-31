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
                                            'Mitigated_Prec_Acc',                                        
                                            'Mitigated_Prec_Vel',
                                            'Mitigated_Prec_Pos',
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
                                            'Real_Gap'])

    ego_traj_df['SimTime'] = prec_traj_df['SimTime'].values     # the number of rows in both DataFrames should match automatically
    ego_traj_df['dT'] = prec_traj_df['dT'].values 
    ego_traj_df['Real_Prec_Acc'] = prec_traj_df['LonAccel'].values
    ego_traj_df['Real_Prec_Vel'] = prec_traj_df['Velocity'].values
    ego_traj_df['Real_Prec_Pos'] = prec_traj_df['Position'].values

    # Initialize the quantized columns
    ego_traj_df['Quantized_Prec_Acc'] = (prec_traj_df['LonAccel']/0.1).round().astype(int) * 0.1

    q_velocity_column = (prec_traj_df['Velocity']/0.01).round().astype(int)
    q_velocity_column.clip(0, 16383)
    ego_traj_df['Quantized_Prec_Vel'] = q_velocity_column * 0.01

    q_position_column = (prec_traj_df['Position']/0.1).round().astype(int)
    q_position_column = q_position_column.clip(0, 32767)
    ego_traj_df['Quantized_Prec_Pos'] = q_position_column * 0.1

    # we don't know anything about the preceding yet, so almost everything is blank
    ego_traj_df.loc[0, 'Ego_Pos_Benign'] = -init_space_headway
    ego_traj_df.loc[0, 'Ego_Pos'] = -init_space_headway
    
    ego_traj_df.loc[0, 'Ego_Vel_Benign'] = prec_traj_df.loc[0, 'Velocity']
    ego_traj_df.loc[0, 'Ego_Vel'] = prec_traj_df.loc[0, 'Velocity']
        
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

# The input 'data' should be a byte object which has the original data + extended data
def dequantize(data):
    # Check if we have minimum required data
    if len(data) < 6:
        raise ValueError("Data must be at least 6 bytes")
        
    # Extract the first 6 bytes for the original data
    original_data = data[:6]
    extended_data = data[6:] if len(data) > 6 else b''
    
    # convert the whole bytes object into a single large integer. Its value represents all bits of the byte sequence
    bitstream = int.from_bytes(original_data, byteorder='big')  
    
    # Extract longitudinal acc (9 bits, signed), vel (14 bits), pos (15 bits), counter (10 bits)
    # Total: 9 + 14 + 15 + 10 = 48 bits = 6 bytes
    # Bits: | 9 lon_accel | 14 vel | 15 pos | 10 counter |
    
    # Extract counter (10 bits, unsigned) - lowest bits
    counter = bitstream & 0x3FF
    bitstream >>= 10
    
    # Position (15 bits, unsigned)
    position = bitstream & 0x7FFF
    bitstream >>= 15
    
    # Velocity (14 bits, unsigned)
    velocity = bitstream & 0x3FFF
    bitstream >>= 14
    
    # LonAccel (9 bits, signed) - highest bits
    lon_accel_raw = bitstream & 0x1FF
    print(f"DEBUG: bitstream = {bitstream:09b} ({bitstream})")
    print(f"DEBUG: lon_accel_raw = {lon_accel_raw:09b} ({lon_accel_raw})")
    
    # Convert lon_accel_raw (9 bits) to signed value (-160..161)
    if lon_accel_raw >= 256:  # 0x100
        lon_accel = lon_accel_raw - 512  # Two's complement conversion for 9 bits
    else:
        lon_accel = lon_accel_raw

    print(f"DEBUG: lon_accel (before scaling) = {lon_accel}")
    
    # Apply quantization units
    lon_accel = lon_accel * 0.1         # 0.1 m/s^2 unit
    velocity = velocity * 0.01          # 0.01 m/s unit
    position = position * 0.1           # 0.1 m unit

    print(f"DEBUG: lon_accel (after scaling) = {lon_accel}")

    # Round to desired decimal places
    lon_accel = round(lon_accel, 1)     # 1 decimal place
    velocity = round(velocity, 2)       # 2 decimal places
    position = round(position, 1)       # 1 decimal place

    print(f"DEBUG: lon_accel (final scaling) = {lon_accel}")
    
    # Process extended data if present
    bit_flip_pairs = []   # 2D list
    xor_bin = 0
    received_checksum = 0
    calculated_checksum = 0
    xor_binary_pattern = '0000000000000000'
    
    if len(extended_data) == 30:  # 6(headers) + 24(recovery data) = 30 bytes
        # Extract received checksum (bytes 0-1 of extended data)
        received_checksum = (extended_data[0] << 8) | extended_data[1]
        
        # Extract calculated checksum (bytes 2-3 of extended data)
        calculated_checksum = (extended_data[2] << 8) | extended_data[3]
        
        # Extract XOR value (bytes 4-5 of extended data)
        xor_value = (extended_data[4] << 8) | extended_data[5]
        xor_value_16bit = xor_value & 0xFFFF
        xor_bin = 1 if xor_value_16bit != 0 else 0
        
        # Convert XOR value to 16-bit binary string
        xor_binary_pattern = "B" + format(xor_value_16bit, '016b')
        
        ## Extract packed recovery data (30 bytes starting from byte 6)
        # Each recovery uses 12 bits (two 6-bit entries), so 24 bytes = 192 bits = 16 recoveries max
        recovery_data = extended_data[6:30]  # 24 bytes of recovery data
        
        # Convert recovery data to bit stream
        recovery_bitstream = 0
        for byte in recovery_data:
            recovery_bitstream = (recovery_bitstream << 8) | byte
        
        # Extract recovery pairs from the bit stream
        bit_offset = 0
        total_bits = len(recovery_data) * 8  # 192 bits total
        
        while bit_offset + 12 <= total_bits:  # Need at least 12 bits for one recovery pair
            # Extract first 6-bit entry (word_index1:2bits + bit_index1:4bits)
            remaining_bits = total_bits - bit_offset
            first_entry = (recovery_bitstream >> (remaining_bits - 6)) & 0x3F
            word_index1 = (first_entry >> 4) & 0x3
            bit_index1 = first_entry & 0xF
            bit_offset += 6
            
            # Extract second 6-bit entry (word_index2:2bits + bit_index2:4bits)
            remaining_bits = total_bits - bit_offset
            second_entry = (recovery_bitstream >> (remaining_bits - 6)) & 0x3F
            word_index2 = (second_entry >> 4) & 0x3
            bit_index2 = second_entry & 0xF
            bit_offset += 6
            
            # Check if this is a valid recovery (not all zeros)
            if first_entry != 0 or second_entry != 0:
                bit_flip_pairs.append([word_index1, bit_index1, word_index2, bit_index2])
            else:
                break  # Stop when we hit padding zeros
        
        # Ensure we always have exactly 16 sublists (pad with zeros if needed)
        while len(bit_flip_pairs) < 16:
            bit_flip_pairs.append([0, 0, 0, 0])
    else:
        print("Length of received data is not 30 bytes.")
    
    return counter, lon_accel, velocity, position, bit_flip_pairs, xor_bin, received_checksum, calculated_checksum, xor_binary_pattern, original_data

# When preceding CAV transmit acc/vel/pos
def calculate_consistency_error(acc, vel, pos, prev_acc, prev_vel, prev_pos, dt):
    """Calculate consistency error using known dt from message timing"""
    # Calculate expected values using kinematic equations with known dt
    expected_vel = prev_vel + prev_acc * dt
    expected_pos = prev_pos + prev_vel * dt + 0.5 * prev_acc * (dt ** 2)
    
    # Calculate errors
    vel_error = abs(vel - expected_vel)
    pos_error = abs(pos - expected_pos)

    print(f"DEBUG: acc={acc}, vel={vel}, pos={pos}")
    print(f"DEBUG: prev_acc={prev_acc}, prev_vel={prev_vel}, prev_pos={prev_pos}, dt={dt}")
    print(f"DEBUG: expected_vel={expected_vel}, expected_pos={expected_pos}")
    print(f"DEBUG: vel_error={vel_error}, pos_error={pos_error}, total={vel_error + pos_error}")
    
    # Return combined error
    return vel_error + pos_error

def mitigation(data_payload, bit_flip_pairs, dt, prev_miti_list=None):
    """
    Mitigate bit flip attacks using the simplified algorithm-based approach.
    No confidence mechanism, no acceleration threshold.
    Parameters:
    data_payload (bytes): 6-byte data payload (prec_net)
    bit_flip_pairs (list): List of potential bit flips (flip_list)
    dt (float): Time duration between messages (t - prev_t)
    prev_miti_list (list): Previous mitigation candidates (prev_miti_list)
    Returns:
    tuple: (best_acc, best_vel, best_pos, is_under_attack, is_bad_message, best_bit_flip_pair, all_corrections, miti_list, miti_list_2D)
    """
    # Define error threshold
    err_thre = 0.1
    
    # Get received values from data_payload
    _, prec_acc_recv, prec_vel_recv, prec_pos_recv, _,_ , _,_ , _,_ = dequantize(data_payload + b'\x00' * 30)
    # Extract valid pairs and create flip_list
    valid_pairs = []
    for pair in bit_flip_pairs:
        if all(x == 0 for x in pair):
            break
        valid_pairs.append(pair)
    is_under_attack = len(valid_pairs) > 0
    # Create flip_list based on attack status
    if is_under_attack:
        flip_list = valid_pairs  # Only corrected pairs
        print(f"Attack detected! Valid pairs: {valid_pairs[:3]}...")
    else:
        flip_list = [[0, 0, 0, 0]]  # Only original when no attack detected
        print("No attack detected - using original data")
    miti_list = []
    miti_list_2D = []
    all_corrections = []
    # If prev_miti_list is empty
    if prev_miti_list is None or len(prev_miti_list) == 0:
        print("Previous miti_list is empty")
        for pair in flip_list:
            try:
                if pair == [0, 0, 0, 0]:
                    prec_app = [prec_acc_recv, prec_vel_recv, prec_pos_recv]
                else:
                    corrected_payload = list(data_payload)
                    word_idx1, bit_idx1, word_idx2, bit_idx2 = pair
                    for word_idx, bit_idx in [(word_idx1, bit_idx1), (word_idx2, bit_idx2)]:
                        if word_idx < 3:
                            byte_idx = word_idx * 2
                            if bit_idx < 8:
                                corrected_payload[byte_idx + 1] ^= (1 << bit_idx)
                            else:
                                corrected_payload[byte_idx] ^= (1 << (bit_idx - 8))
                    _, acc, vel, pos,_ , _,_ , _,_ , _= dequantize(bytes(corrected_payload) + b'\x00' * 30)
                    prec_app = [acc, vel, pos]
                miti_list.append({
                    'pair': pair,
                    'app': prec_app,
                    'error': 0,
                    'acc': prec_app[0]
                })
                all_corrections.append([pair, prec_app, 0])
            except Exception as e:
                print(f"Error processing pair {pair}: {e}")
                continue
        
        # Filter out options with acc outside [-8, 8] range
        filtered_miti_list = [item for item in miti_list if -8 <= item['acc'] <= 8]
        
        if not filtered_miti_list:
            print("All options filtered out - using original best")
            miti_list.sort(key=lambda x: x['acc'])
            best = miti_list[0]
        else:
            filtered_miti_list.sort(key=lambda x: x['acc'])
            best = filtered_miti_list[0]
            miti_list = filtered_miti_list  # Update miti_list to filtered version
        
        # Return empty miti_list if flip_list is all zeros, otherwise return miti_list
        if not is_under_attack:  # flip_list is all zeros
            print("Returning empty miti_list (no attack case)")
            return (best['app'][0], best['app'][1], best['app'][2], is_under_attack, False,
                    best['pair'], all_corrections, [], [])
        else:
            return (best['app'][0], best['app'][1], best['app'][2], is_under_attack, False,
                    best['pair'], all_corrections, miti_list, [])
    # If flip_list is all zeros (no attack detected)
    if not is_under_attack:
        print("No attack - checking consistency")
        prec_app = [prec_acc_recv, prec_vel_recv, prec_pos_recv]
        # Check consistency with all previous candidates
        for prev_item in prev_miti_list:
            prev_app = prev_item['app']
            error = calculate_consistency_error(prec_app[0], prec_app[1], prec_app[2],
                                                prev_app[0], prev_app[1], prev_app[2], dt)
            if error < err_thre:  # epsilon threshold
                miti_list.append({
                    'pair': [0, 0, 0, 0],
                    'app': prec_app,
                    'error': error,
                    'acc': prec_app[0]
                })
        # If miti_list is empty - use kinematic prediction and put it in miti_list
        if len(miti_list) == 0:
            print("No good consistency - using kinematic prediction")
            prev_prec_app = prev_miti_list[0]['app']
            predicted_vel = prev_prec_app[1] + prev_prec_app[0] * dt
            predicted_pos = prev_prec_app[2] + prev_prec_app[1] * dt + 0.5 * prev_prec_app[0] * (dt ** 2)
            predicted_acc = prev_prec_app[0]
            
            # Round to desired decimal places
            predicted_acc = round(predicted_acc, 1)     # 1 decimal place
            predicted_vel = round(predicted_vel, 2)     # 2 decimal places
            predicted_pos = round(predicted_pos, 1)     # 1 decimal place
            prec_app = [predicted_acc, predicted_vel, predicted_pos]
            
            # Put kinematic prediction into miti_list
            miti_list.append({
                'pair': [0, 0, 0, 0],  # No bit flip pair for kinematic prediction
                'app': prec_app,
                'error': 0,  # Set error to 0 for kinematic prediction
                'acc': prec_app[0]
            })
        else:
            # Filter out options with acc outside [-8, 8] range
            filtered_miti_list = [item for item in miti_list if -8 <= item['acc'] <= 8]
            
            if not filtered_miti_list:
                print("All options filtered out - using kinematic prediction")
                prev_prec_app = prev_miti_list[0]['app']
                predicted_vel = prev_prec_app[1] + prev_prec_app[0] * dt
                predicted_pos = prev_prec_app[2] + prev_prec_app[1] * dt + 0.5 * prev_prec_app[0] * (dt ** 2)
                predicted_acc = round(prev_prec_app[0], 1)
                predicted_vel = round(predicted_vel, 2)
                predicted_pos = round(predicted_pos, 1)
                prec_app = [predicted_acc, predicted_vel, predicted_pos]
                
                # Put kinematic prediction into miti_list
                miti_list = [{
                    'pair': [0, 0, 0, 0],
                    'app': prec_app,
                    'error': 0,
                    'acc': prec_app[0]
                }]
            else:
                # Sort by acceleration
                filtered_miti_list.sort(key=lambda x: x['acc'])
                prec_app = filtered_miti_list[0]['app']
                miti_list = filtered_miti_list  # Update miti_list to filtered version
        
        all_corrections.append([[0, 0, 0, 0], prec_app, 0])
        print("No attack - returning miti_list")
        return (prec_app[0], prec_app[1], prec_app[2], False, False,
                [0, 0, 0, 0], all_corrections, miti_list, [])
    # Attack detected - process all pairs
    print(f"Attack detected - processing {len(valid_pairs)} pairs")
    for pair in flip_list:
        try:
            corrected_payload = list(data_payload)
            word_idx1, bit_idx1, word_idx2, bit_idx2 = pair
            
            for word_idx, bit_idx in [(word_idx1, bit_idx1), (word_idx2, bit_idx2)]:
                if word_idx < 3:
                    byte_idx = word_idx * 2
                    if bit_idx < 8:
                        corrected_payload[byte_idx + 1] ^= (1 << bit_idx)
                    else:
                        corrected_payload[byte_idx] ^= (1 << (bit_idx - 8))
            _, acc, vel, pos,_ , _,_ , _,_ , _= dequantize(bytes(corrected_payload) + b'\x00' * 30)
            prec_app = [acc, vel, pos]
            row = []
            for prev_item in prev_miti_list:
                prev_app = prev_item['app']
                error = calculate_consistency_error(prec_app[0], prec_app[1], prec_app[2],
                                                    prev_app[0], prev_app[1], prev_app[2], dt)
                row.append({
                    'pair': pair,
                    'app': prec_app,
                    'error': error,
                    'acc': prec_app[0]
                })
            # Add row to miti_list_2D and best of row to miti_list
            if row:
                miti_list_2D.append(row)
                best_in_row = min(row, key=lambda x: (x['error'], x['acc']))
                miti_list.append(best_in_row)
                all_corrections.append([pair, prec_app, best_in_row['error']])
        except Exception as e:
            print(f"Error processing pair {pair}: {e}")
            continue
    
    # Filter out options with acc outside [-8, 8] range
    filtered_miti_list = [item for item in miti_list if -8 <= item['acc'] <= 8]
    
    if not filtered_miti_list:
        print("All options filtered out - using original best")
        miti_list.sort(key=lambda x: (x['error'], x['acc']))
        best = miti_list[0]
    else:
        # Sort miti_list by error and acceleration
        filtered_miti_list.sort(key=lambda x: (x['error'], x['acc']))
        best = filtered_miti_list[0]
        miti_list = filtered_miti_list  # Update miti_list to filtered version
    
    print(f"Attack detected - selected pair: {best['pair']}, error: {best['error']:.6f}")
    return (best['app'][0], best['app'][1], best['app'][2], is_under_attack, False,
            best['pair'], all_corrections, miti_list, miti_list_2D)

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

    # Initialize tracking lists
    bit_flip_pairs_3D = []
    xor_bin_list = []
    received_checksum_list = []
    calculated_checksum_list = []
    error_indices = []
    xor_pattern_list = []

    # Add new tracking columns including mitigation-specific columns
    ego_traj_df['Under_Attack'] = 0
    ego_traj_df['Bad_Message'] = 0
    ego_traj_df['Mitigation_Applied'] = 0
    ego_traj_df['Best_Bit_Flip_Pair'] = ''
    ego_traj_df['All_Corrections'] = ''
    ego_traj_df['Perfect_Candidate_List'] = ''
    ego_traj_df['Miti_List'] = ''
    ego_traj_df['Miti_List_2D'] = ''
    
    # Initialize mitigation variables without assuming first message is benign
    counter = -1
    candidate_history = None  # Start with empty history
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    host = '0.0.0.0'
    port = 65432
    server_socket.bind((host, port))
    print(f"EGO listening on {host}:{port}")
    ### Start Simulation ###
    while True:        
        if counter >= 0:
            server_socket.settimeout(0.30)
        
        print("Waiting for preceding to send data...")

        try:
            data, addr = server_socket.recvfrom(1024)
            print(f"Received {len(data)} bytes of data!")

            if counter == -1:
                counter = 0
            
            # Extract sender's IP and port
            sender_ip = addr[0]
            sender_port = addr[1]
            receiver_ip, receiver_port = server_socket.getsockname()
            print(f"Received data on interface bound to IP: {receiver_ip}, Port: {receiver_port}")
            print(f"Received data from IP: {sender_ip}, Port: {sender_port}")

            # Decode the received data
            prec_counter, prec_acc_recv, prec_vel_recv, prec_pos_recv, bit_flip_pairs, xor_bin, rx_checksum, calc_checksum, xor_pattern, data_payload = dequantize(data)
            
            # Check if counter is within bounds before accessing DataFrame
            if counter >= len(ego_traj_df):
                print(f"Counter {counter} exceeds DataFrame length {len(ego_traj_df)}, breaking")
                break
            
            # Get dt for consistency checking
            dt = ego_traj_df.loc[counter, 'dT']

            # Debug candidate_history before mitigation call
            print(f"DEBUG: About to call mitigation with counter={counter}")
            print(f"DEBUG: candidate_history length: {len(candidate_history) if candidate_history else 0}")
           
            # Call mitigation function (simplified without confidence and acceleration threshold)
            prec_acc_miti, prec_vel_miti, prec_pos_miti, under_attack, bad_message, best_bit_flip_pair, all_corrections, candidate_history, miti_list_2D = mitigation(
                data_payload, bit_flip_pairs, dt, candidate_history
            )

            if under_attack or (prec_acc_recv != prec_acc_miti or prec_vel_recv != prec_vel_miti or prec_pos_recv != prec_pos_miti):
                print(f"Original: acc={prec_acc_recv:.1f}, vel={prec_vel_recv:.2f}, pos={prec_pos_recv:.1f}")
                print(f"Mitigated: acc={prec_acc_miti:.1f}, vel={prec_vel_miti:.2f}, pos={prec_pos_miti:.1f}")
                print(f"Best bit flip pair: {best_bit_flip_pair}")
                ego_traj_df.at[counter, 'Mitigation_Applied'] = 1
            else:
                ego_traj_df.at[counter, 'Mitigation_Applied'] = 0

            # Store all corrections and tracking data
            ego_traj_df.at[counter, 'All_Corrections'] = json.dumps(all_corrections)
            ego_traj_df.at[counter, 'Under_Attack'] = 1 if under_attack else 0
            ego_traj_df.at[counter, 'Bad_Message'] = 1 if bad_message else 0
            ego_traj_df.at[counter, 'Best_Bit_Flip_Pair'] = str(best_bit_flip_pair)

            # Store perfect candidate list and miti_list
            perfect_candidate_list = [item['pair'] for item in candidate_history] if candidate_history else []
            ego_traj_df.at[counter, 'Perfect_Candidate_List'] = str(perfect_candidate_list)

            # Store miti_list as JSON
            ego_traj_df.at[counter, 'Miti_List'] = json.dumps([{
                'pair': item['pair'],
                'acc': item['acc'],
                'vel': item['app'][1],
                'pos': item['app'][2],
                'error': item['error']
            } for item in candidate_history]) if candidate_history else '[]'
            
            # Store miti_list_2D as JSON
            ego_traj_df.at[counter, 'Miti_List_2D'] = json.dumps([[{
                'pair': item['pair'],
                'acc': item['acc'],
                'vel': item['app'][1],
                'pos': item['app'][2],
                'error': item['error']
            } for item in row] for row in miti_list_2D]) if miti_list_2D else '[]'

            # Store mitigated values
            ego_traj_df.at[counter, 'Mitigated_Prec_Acc'] = prec_acc_miti
            ego_traj_df.at[counter, 'Mitigated_Prec_Vel'] = prec_vel_miti
            ego_traj_df.at[counter, 'Mitigated_Prec_Pos'] = prec_pos_miti   

            # Store bit flip data
            bit_flip_pairs_3D.append(bit_flip_pairs)
            xor_bin_list.append(xor_bin)
            received_checksum_list.append(rx_checksum)
            calculated_checksum_list.append(calc_checksum)
            print("Received quantized data:", prec_counter, prec_acc_recv, prec_vel_recv, prec_pos_recv)
            print("Received checksum: 0x{:04x}".format(rx_checksum))
            print("Calculated checksum: 0x{:04x}".format(calc_checksum))
            print("XOR nonzero:", xor_bin)
            print(f"Bit flip pairs shape: {len(bit_flip_pairs)} x {len(bit_flip_pairs[0]) if bit_flip_pairs else 0}")
            if bit_flip_pairs:
                print("Bit_flip_pairs (first 3 blocks):", bit_flip_pairs[:3])
            print("Mitigated quantized data:", prec_counter, prec_acc_miti, prec_vel_miti, prec_pos_miti)
            print(f"Candidate history length: {len(candidate_history) if candidate_history else 0}")
            ack = b'\x01'
            server_socket.sendto(ack, addr)
        except socket.timeout:
            print("Connection or receive operation timed out")
            print("Using previous step's acceleration value")
            prec_counter = counter
            prec_acc_recv = 0
            prec_vel_recv = 0
            prec_pos_recv = 0
            # Check bounds before accessing DataFrame
            if counter >= len(ego_traj_df):
                print(f"Counter {counter} exceeds DataFrame length {len(ego_traj_df)}, breaking")
                break
            # Fill out missing data using kinematic prediction
            if counter > 0:
                prec_acc_miti = ego_traj_df.at[counter - 1, 'Mitigated_Prec_Acc']
                prec_vel_miti = ego_traj_df.at[counter - 1, 'Mitigated_Prec_Vel'] + prec_acc_miti * ego_traj_df.at[counter - 1, 'dT']       
                prec_pos_miti = ego_traj_df.at[counter - 1, 'Mitigated_Prec_Pos'] + \
                    ego_traj_df.at[counter - 1, 'Mitigated_Prec_Vel'] * ego_traj_df.at[counter - 1, 'dT'] + \
                    0.5 * prec_acc_miti * (ego_traj_df.at[counter - 1, 'dT']**2)
            else:
                # For first message timeout, use quantized data as fallback
                prec_acc_miti = ego_traj_df.at[0, 'Quantized_Prec_Acc']
                prec_vel_miti = ego_traj_df.at[0, 'Quantized_Prec_Vel']
                prec_pos_miti = ego_traj_df.at[0, 'Quantized_Prec_Pos']
            # Store timeout data
            bit_flip_pairs_3D.append([[3, 3, 3, 3] for _ in range(4)])
            xor_bin_list.append(2)
            received_checksum_list.append(0)
            calculated_checksum_list.append(0)
            # Store timeout tracking data
            ego_traj_df.at[counter, 'Under_Attack'] = 0
            ego_traj_df.at[counter, 'Bad_Message'] = 1  # Timeout is bad
            ego_traj_df.at[counter, 'Mitigation_Applied'] = 1  # We applied kinematic prediction
            ego_traj_df.at[counter, 'Best_Bit_Flip_Pair'] = '[0, 0, 0, 0]'
            ego_traj_df.at[counter, 'All_Corrections'] = '[]'
            ego_traj_df.at[counter, 'Perfect_Candidate_List'] = '[]'
            ego_traj_df.at[counter, 'Miti_List'] = '[]'
            ego_traj_df.at[counter, 'Miti_List_2D'] = '[]'
            ego_traj_df.at[counter, 'Mitigated_Prec_Acc'] = prec_acc_miti
            ego_traj_df.at[counter, 'Mitigated_Prec_Vel'] = prec_vel_miti
            ego_traj_df.at[counter, 'Mitigated_Prec_Pos'] = prec_pos_miti
            # Reset candidate history on timeout
            candidate_history = None
            ack = b'\x00'
            server_socket.sendto(ack, addr)
            error_indices.append(counter)

        # Check break conditions BEFORE accessing DataFrame
        if prec_counter >= 1022 or counter >= len(prec_traj_df) or counter >= len(ego_traj_df):
            print(f"Breaking: prec_counter={prec_counter}, counter={counter}, traj_len={len(prec_traj_df)}, ego_len={len(ego_traj_df)}")
            break

        # Get real and quantized data from the original trajectory
        real_prec_acc = prec_traj_df.at[counter, 'LonAccel']
        real_prec_vel = prec_traj_df.at[counter, 'Velocity']
        real_prec_pos = prec_traj_df.at[counter, 'Position']
        quan_prec_acc = ego_traj_df.at[counter, 'Quantized_Prec_Acc']
        quan_prec_vel = ego_traj_df.at[counter, 'Quantized_Prec_Vel']
        quan_prec_pos = ego_traj_df.at[counter, 'Quantized_Prec_Pos']  

        # Cap received values in reasonable range
        prec_acc_recv = min(max(-8, prec_acc_recv), 8)
        prec_vel_recv = min(max(-50, prec_vel_recv), 50)
        print(f"Counter: {prec_counter}")
        print(f"Real: Acc={real_prec_acc:.1f}, Vel={real_prec_vel:.2f}, Pos={real_prec_pos:.1f}")
        print(f"Quantized: Acc={quan_prec_acc:.1f}, Vel={quan_prec_vel:.2f}, Pos={quan_prec_pos:.1f}")
        print(f"Received: Acc={prec_acc_recv:.1f}, Vel={prec_vel_recv:.2f}, Pos={prec_pos_recv:.1f}\n")

        # Store all preceding data in the dataframe     
        ego_traj_df.at[counter, 'Received_Prec_Acc'] = prec_acc_recv
        ego_traj_df.at[counter, 'Received_Prec_Vel'] = prec_vel_recv
        ego_traj_df.at[counter, 'Received_Prec_Pos'] = prec_pos_recv
        ego_traj_df.at[counter, 'Bit_Flip_Pairs'] = json.dumps(bit_flip_pairs_3D[counter])
        ego_traj_df.at[counter, 'XOR_Bin'] = xor_bin_list[counter]

        ### Calculate Ego trajectories for both quantized and mitigated data ###
        dT = ego_traj_df.loc[counter, 'dT']
        
        # Initialize ego vehicle state for first timestep
        if counter == 0:
            # For quantized data trajectory (benign)
            ego_vel_quantized = ego_traj_df.at[0, 'Ego_Vel_Benign']
            ego_pos_quantized = ego_traj_df.at[0, 'Ego_Pos_Benign']
            prev_ego_acc_quantized = 0
            
            # For mitigated data trajectory (actual)
            ego_vel = ego_traj_df.at[0, 'Ego_Vel']
            ego_pos = ego_traj_df.at[0, 'Ego_Pos']
            prev_ego_acc = 0
        else:
            # For quantized data trajectory
            ego_vel_quantized = ego_traj_df.loc[counter, 'Ego_Vel_Benign']
            ego_pos_quantized = ego_traj_df.loc[counter, 'Ego_Pos_Benign']
            prev_ego_acc_quantized = ego_traj_df.loc[counter-1, 'Ego_Acc_Benign']
            
            # For mitigated data trajectory
            ego_vel = ego_traj_df.loc[counter, 'Ego_Vel']
            ego_pos = ego_traj_df.loc[counter, 'Ego_Pos']
            prev_ego_acc = ego_traj_df.loc[counter-1, 'Ego_Acc']
        
        # Calculate ego acceleration using quantized (benign) preceding data
        ego_acc_quantized, safe_gap_quantized = Compute_CACC_Acc(quan_prec_acc,
                                                                 quan_prec_vel,
                                                                 quan_prec_pos,
                                                                 ego_vel_quantized,
                                                                 ego_pos_quantized,
                                                                 dT,
                                                                 prev_ego_acc_quantized,
                                                                 max_acc,
                                                                 max_dec,
                                                                 speed_limit,
                                                                 min_speed)
        next_ego_vel_quantized = ego_vel_quantized + ego_acc_quantized * dT
        next_ego_pos_quantized = ego_pos_quantized + ego_vel_quantized * dT + 0.5 * ego_acc_quantized * (dT**2)
        
        # Calculate ego acceleration using mitigated preceding data
        ego_acc, safe_gap = Compute_CACC_Acc(prec_acc_miti,
                                              prec_vel_miti,
                                              prec_pos_miti,
                                              ego_vel,
                                              ego_pos,
                                              dT,
                                              prev_ego_acc,
                                              max_acc,
                                              max_dec,
                                              speed_limit,
                                              min_speed)
        next_ego_vel = ego_vel + ego_acc * dT
        next_ego_pos = ego_pos + ego_vel * dT + 0.5 * ego_acc * (dT**2)
        
        # Store all ego data in the dataframe
        ego_traj_df.loc[counter, 'Ego_Acc_Benign'] = ego_acc_quantized
        if counter + 1 < len(ego_traj_df):
            ego_traj_df.loc[counter + 1, 'Ego_Vel_Benign'] = next_ego_vel_quantized
            ego_traj_df.loc[counter + 1, 'Ego_Pos_Benign'] = next_ego_pos_quantized
            ego_traj_df.loc[counter + 1, 'Ego_Vel'] = next_ego_vel
            ego_traj_df.loc[counter + 1, 'Ego_Pos'] = next_ego_pos

        ego_traj_df.loc[counter, 'Ego_Acc'] = ego_acc

        # Store additional metrics
        ego_traj_df.loc[counter, 'Safe_Gap_Benign'] = safe_gap_quantized
        ego_traj_df.loc[counter, 'Safe_Gap'] = safe_gap

        # Calculate space headways
        ego_traj_df.loc[counter, 'Space_Headway_Benign'] = real_prec_pos - ego_pos_quantized
        ego_traj_df.loc[counter, 'Space_Headway'] = ego_traj_df.at[counter, 'Mitigated_Prec_Pos'] - ego_pos
        ego_traj_df.loc[counter, 'Real_Gap'] = real_prec_pos - ego_pos

        # Time headways
        ego_traj_df.loc[counter, 'Time_Headway_Benign'] = (real_prec_pos - ego_pos_quantized) / ego_vel_quantized if ego_vel_quantized > 0 else 0
        ego_traj_df.loc[counter, 'Time_Headway'] = (ego_traj_df.at[counter, 'Mitigated_Prec_Pos'] - ego_pos) / ego_vel if ego_vel > 0 else 0        
        counter += 1  

    # Final processing
    ego_traj_df = ego_traj_df.head(-1)
    server_socket.close()
    print("Connection ended.")
    print("Connection Fail Rate:", len(error_indices)/len(ego_traj_df.index))
    print(f"Bit flip pairs 3D shape: {len(bit_flip_pairs_3D)} x 12 x 4")
    print(f"XOR bin list length: {len(xor_bin_list)}")
    ### Plots comparing quantized vs received trajectories ###
    PlotDf(ego_traj_df,
           columns_to_plot=['Real_Prec_Acc', 'Received_Prec_Acc', 'Mitigated_Prec_Acc'],
           title=f'{file_name}_CACC_Real_Acceleration',  
           xlabel='Time Step',
           ylabel='Acceleration (m/s²)')
    PlotDf(ego_traj_df,
           columns_to_plot=['Real_Prec_Vel', 'Received_Prec_Vel', 'Mitigated_Prec_Vel'],
           title=f'{file_name}_CACC_Real_Velocity',
           xlabel='Time Step',
           ylabel='Velocity (m/s)')
    PlotDf(ego_traj_df,
           columns_to_plot=['Real_Prec_Pos', 'Received_Prec_Pos', 'Mitigated_Prec_Pos'],
           title=f'{file_name}_CACC_Real_Position',
           xlabel='Time Step',
           ylabel='Position (m)')
    PlotDf(ego_traj_df,
           columns_to_plot=['Time_Headway_Benign', 'Time_Headway'],
           title=f'{file_name}_CACC_Time_Headway',
           xlabel='Time Step',
           ylabel='Time Headway (s)')
    PlotDf(ego_traj_df,
           columns_to_plot=['Space_Headway_Benign', 'Space_Headway'],
           title=f'{file_name}_CACC_Real_Gap',
           xlabel='Time Step',
           ylabel='Gap (m)')
    # Save the comprehensive dataframe
    ego_traj_df.to_csv('/tmp/ego_traj.csv', index=False)