import pandas as pd

# Read the original CSV file
df = pd.read_csv('./application/ego_traj.csv')

# Select only the columns you want
columns_to_keep = ['Quantized_Prec_Acc',
                   'Quantized_Prec_Vel',
                   'Quantized_Prec_Pos',
                   'Received_Prec_Acc',
                   'Received_Prec_Vel',
                   'Received_Prec_Pos',
                   'Mitigated_Prec_Acc',
                   'Mitigated_Prec_Vel',
                   'Mitigated_Prec_Pos',                  
                   'Confidence',
                   'Miti_List',      
                   'Miti_List_2D'
                   ]  # Replace with your actual column names
new_df = df[columns_to_keep]

# Save to a new CSV file
new_df.to_csv('truc_ego_traj_file.csv', index=False)


# 'Best_Bit_Flip_Pair',