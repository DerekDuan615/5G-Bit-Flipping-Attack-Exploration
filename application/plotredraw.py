import pandas as pd
import matplotlib.pyplot as plt
import os
  
def plot_cacc_analysis(csv_file_path):
    """
    Create 4 separate plots from CSV data for CACC analysis.
    Parameters:
    csv_file_path (str): Path to the CSV file
    """
    # Read the CSV file
    ego_traj_df = pd.read_csv(csv_file_path)


    ego_traj_df = ego_traj_df.rename(columns={
    'Ego_Acc_Miti': 'Ego_Acc',
    'Ego_Vel_Miti': 'Ego_Vel',
    'Ego_Pos_Miti': 'Ego_Pos',
    'Time_Headway_Miti': 'Time_Headway'
    })

    plot_configs = [
        {
            'columns': ['Real_Prec_Acc', 'Ego_Acc'],
            'name': 'Acceleration'
        },
        {
            'columns': ['Real_Prec_Vel', 'Ego_Vel'],
            'name': 'Velocity'
        },
        {
            'columns': ['Real_Prec_Pos', 'Ego_Pos'],
            'name': 'Position'
        },
        {
            'columns': ['Time_Headway'],
            'name': 'Time_Headway'
        }
    ]

    # plot_configs = [
    #     {
    #         'columns': ['Real_Prec_Acc', 'Received_Prec_Acc', 'Ego_Acc_Benign', 'Ego_Acc'],
    #         'name': 'Acceleration'
    #     },
    #     {
    #         'columns': ['Real_Prec_Vel', 'Received_Prec_Vel', 'Ego_Vel_Benign', 'Ego_Vel'],
    #         'name': 'Velocity'
    #     },
    #     {
    #         'columns': ['Real_Prec_Pos', 'Received_Prec_Pos', 'Ego_Pos_Benign', 'Ego_Pos'],
    #         'name': 'Position'
    #     },
    #     {
    #         'columns': ['Time_Headway_Benign', 'Time_Headway'],
    #         'name': 'Time_Headway'
    #     }
    # ]
    
    # plot_configs = [
    #     {
    #         'columns': ['Real_Prec_Acc', 'Mitigated_Prec_Acc', 'Ego_Acc_Benign', 'Ego_Acc_Miti'],
    #         'name': 'Acceleration'
    #     },
    #     {
    #         'columns': ['Real_Prec_Vel', 'Mitigated_Prec_Vel', 'Ego_Vel_Benign', 'Ego_Vel_Miti'],
    #         'name': 'Velocity'
    #     },
    #     {
    #         'columns': ['Real_Prec_Pos', 'Mitigated_Prec_Pos', 'Ego_Pos_Benign', 'Ego_Pos_Miti'],
    #         'name': 'Position'
    #     },
    #     {
    #         'columns': ['Time_Headway_Benign', 'Time_Headway_Miti'],
    #         'name': 'Time_Headway'
    #     }
    # ]
    
    # Define markers for different lines
    markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h', 'H', '+', 'x', '8']
    
    # Create graphs directory if it doesn't exist
    os.makedirs("./graphs", exist_ok=True)
    
    for plot_idx, config in enumerate(plot_configs):
        plt.figure(figsize=(12, 6))
        
        columns_to_plot = config['columns']
        
        # Check if all columns exist in the dataframe
        available_columns = [col for col in columns_to_plot if col in ego_traj_df.columns]
        
        if not available_columns:
            print(f"Warning: No columns found for plot {plot_idx + 1}")
            continue
            
        plot_df = ego_traj_df[available_columns].copy()
        
        # Calculate marker spacing
        data_length = len(plot_df)
        base_interval = max(20, data_length // 8)
        
        # Store handles and labels in the order of columns
        handles = []
        labels = []
        
        # Plot all columns in the order they appear in the configuration
        marker_index = 0
        for i, column in enumerate(columns_to_plot):
            if column not in ego_traj_df.columns:
                continue
                
            if i == 0:
                # Plot the first column without markers (will be on top)
                line = plt.plot(plot_df.index, plot_df[column], label=column,
                               linewidth=3, zorder=10)
            else:
                # Plot other columns with markers
                marker = markers[marker_index % len(markers)]
                marker_index += 1
                
                # Plot the line
                line = plt.plot(plot_df.index, plot_df[column], label=column,
                               linewidth=3, zorder=5)
                line_color = line[0].get_color()
                
                # Add markers
                start_offset = (marker_index - 1) * (base_interval // 4)
                marker_positions = []
                for pos in range(start_offset, data_length, base_interval):
                    if pos < data_length:
                        marker_positions.append(pos)
                
                if marker_positions:
                    marker_x = [plot_df.index[pos] for pos in marker_positions]
                    marker_y = [plot_df[column].iloc[pos] for pos in marker_positions]
                    plt.scatter(marker_x, marker_y, marker=marker, s=120,
                               color=line_color, zorder=6)
            
            handles.append(line[0])
            labels.append(column)
        
        # Customize the plot with legend in column order
        plt.legend(handles, labels, fontsize=28)
        plt.tick_params(axis='both', which='major', labelsize=26)
        plt.grid(True)
        
        # Handle x-axis labels and ticks
        if plot_idx >= 2:
            # Last two plots (c and d): show both x-ticks and x-labels
            plt.xlabel('Timestep', fontsize=26)
        else:
            # First two plots (a and b): show x-tick values but no x-axis label
            ax = plt.gca()
            ax.tick_params(axis='x', which='major', labelsize=26, labelbottom=True)
            # Don't set xlabel for first two plots
        
        # Minimize margins
        plt.tight_layout()
        plt.subplots_adjust(left=0.06, right=0.98, top=0.98, bottom=0.08)
        
        # Save the figure
        save_path = f"./graphs/CACC_{config['name']}.png"
        plt.savefig(save_path, bbox_inches='tight', pad_inches=0.1, dpi=200)
        print(f"Saved image to {save_path}")
        
        plt.show()

# Example usage function
def main():
    """
    Example usage of the plotting function
    """
    csv_file_path = "./application/ego_traj.csv"  # Replace with your CSV file path
    plot_cacc_analysis(csv_file_path)

if __name__ == "__main__":
    main()