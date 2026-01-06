import scipy.io
import os
import numpy as np

def inspect_mat_file(file_path):
    try:
        mat_data = scipy.io.loadmat(file_path)
        print(f"--- Inspecting {os.path.basename(file_path)} ---")
        print(f"Keys: {list(mat_data.keys())}")
        
        for key, value in mat_data.items():
            if key.startswith('__'):
                continue
            
            print(f"\nKey: {key}")
            print(f"Type: {type(value)}")
            if isinstance(value, np.ndarray):
                print(f"Shape: {value.shape}")
                print(f"Dtype: {value.dtype}")
                # Print a small snippet if it's not too huge
                if value.size < 20:
                     print(f"Data: {value}")
                else:
                     print(f"Data (first 5): {value.flatten()[:5]}")

    except Exception as e:
        print(f"Error reading {file_path}: {e}")

if __name__ == "__main__":
    # Inspect a few files to see if they are consistent
    files_to_inspect = ['HR_IMU_data/fall1.mat', 'HR_IMU_data/bed.mat']
    for f in files_to_inspect:
        if os.path.exists(f):
            inspect_mat_file(f)
        else:
            print(f"File not found: {f}")
