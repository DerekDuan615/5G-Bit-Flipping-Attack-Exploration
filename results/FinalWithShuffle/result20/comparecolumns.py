# import os
# print("Files in current directory:")
# print(os.listdir('.'))

import numpy as np
import pandas as pd

# def compare_columns(file_path, column1, column2, threshold=0.1):
#     """
#     Compare two columns and count rows where difference > threshold
    
#     Returns: number of rows exceeding threshold
#     """
#     df = pd.read_csv(file_path)
#     differences = abs(df[column1] - df[column2])
#     return sum(differences > threshold)

# # Usage - just use the filename:
# count = compare_columns('./application/ego_traj.csv', 'Quantized_Prec_Acc', 'Received_Prec_Acc', 0.1)
# print(f"Rows with difference > 0.1: {count}")

def compare_columns(file_path, column_pairs, tolerance=1e-10):
    """
    Compare column pairs and return row indices where they differ.
    Args:
        file_path (str): Path to CSV file
        column_pairs (list): List of tuples like [('col1', 'col2'), ('col3', 'col4')]
        tolerance (float): Tolerance for floating point comparison
    Returns:
        dict: {comparison_name: [differing_row_indices]} (indices adjusted for header row)
    """
    df = pd.read_csv(file_path)
    results = {}
    
    for col1, col2 in column_pairs:
        # Check if columns are numeric (float/int)
        if pd.api.types.is_numeric_dtype(df[col1]) and pd.api.types.is_numeric_dtype(df[col2]):
            # Use tolerance-based comparison for numeric data
            diff_mask = ~np.isclose(df[col1], df[col2], rtol=tolerance, atol=tolerance, equal_nan=True)
        else:
            # Use exact comparison for non-numeric data
            diff_mask = df[col1] != df[col2]
        
        # Get pandas indices and convert to "row numbers" (adding 1 to account for header)
        pandas_indices = df[diff_mask].index.tolist()
        row_numbers = [idx + 1 for idx in pandas_indices]  # Add 1 to account for header row
        
        results[f"{col1}_vs_{col2}"] = row_numbers
        print(f"{col1} vs {col2}: {len(row_numbers)} differences")
        
        # Debug: show actual values for first few differences
        # if len(pandas_indices) > 0:
        #     print(f"  First few differences:")
        #     for i in pandas_indices[:3]:
        #         print(f"    Row {i+1}: {df.loc[i, col1]} vs {df.loc[i, col2]}")
    
    return results

# Usage example
if __name__ == "__main__":
    # Your 3 comparisons
    comparisons = [
        ('Quantized_Prec_Acc', 'Mitigated_Prec_Acc'),
        ('Quantized_Prec_Vel', 'Mitigated_Prec_Vel'),
        ('Quantized_Prec_Pos', 'Mitigated_Prec_Pos')
    ]
    
    results = compare_columns('./application/ego_traj.csv', comparisons)
    
    # Print results
    for name, indices in results.items():
        print(f"{name}: rows {indices[:10]}...")  # Show first 10 indices