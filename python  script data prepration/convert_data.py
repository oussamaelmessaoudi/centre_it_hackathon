import os
import glob
import pandas as pd
import numpy as np
from scipy.io import loadmat

# --- Configuration ---
DATA_DIR = './HR_IMU_data'
OUTPUT_CSV_FILE = 'HR_IMU_Fall_Detection_Dataset_Labeled.csv'

# Mapping from our desired column names to the keys found in the .mat files
KEY_MAPPING = {
    'ax': 'ax', 'ay': 'ay', 'az': 'az',
    'qw': 'w', 'qx': 'x', 'qy': 'y', 'qz': 'z',
    'droll': 'droll', 'dpitch': 'dpitch', 'dyaw': 'dyaw',
    'heart': 'heart'
}

# Subject Metadata
SUBJECT_METADATA = {
    'subject_01': {'Age': 31, 'Sex': 'Male'},
    'subject_02': {'Age': 21, 'Sex': 'Female'},
    'subject_03': {'Age': 28, 'Sex': 'Male'},
    'subject_04': {'Age': 25, 'Sex': 'Male'},
    'subject_05': {'Age': 21, 'Sex': 'Female'},
    'subject_06': {'Age': 28, 'Sex': 'Male'},
    'subject_07': {'Age': 32, 'Sex': 'Male'},
    'subject_08': {'Age': 22, 'Sex': 'Female'},
    'subject_09': {'Age': 27, 'Sex': 'Male'},
    'subject_10': {'Age': 25, 'Sex': 'Male'},
    'subject_11': {'Age': 27, 'Sex': 'Male'},
    'subject_12': {'Age': 26, 'Sex': 'Male'},
    'subject_13': {'Age': 21, 'Sex': 'Female'},
    'subject_14': {'Age': 21, 'Sex': 'Female'},
    'subject_15': {'Age': 29, 'Sex': 'Female'},
    'subject_16': {'Age': 30, 'Sex': 'Male'},
    'subject_17': {'Age': 23, 'Sex': 'Female'},
    'subject_18': {'Age': 28, 'Sex': 'Male'},
    'subject_19': {'Age': 25, 'Sex': 'Male'},
    'subject_20': {'Age': 21, 'Sex': 'Female'},
    'subject_21': {'Age': 30, 'Sex': 'Male'}
}

def convert_mat_to_csv():
    all_files = glob.glob(os.path.join(DATA_DIR, '*.mat'))
    if not all_files:
        print(f"Error: No .mat files found in the directory: {DATA_DIR}")
        return

    print(f"Found {len(all_files)} .mat files. Starting conversion...")

    combined_data = []

    for file_path in all_files:
        filename = os.path.basename(file_path)
        # Filename format: subject_XX_activity.mat
        # Example: subject_01_fall1.mat
        
        try:
            parts = filename.split('_')
            # subject_01_fall1.mat -> ['subject', '01', 'fall1.mat']
            if len(parts) >= 3:
                subject_id = f"{parts[0]}_{parts[1]}" # subject_01
                scenario_label = '_'.join(parts[2:]).split('.')[0] # fall1
            else:
                # Fallback if naming is weird
                subject_id = 'Unknown'
                scenario_label = filename.split('.')[0]
        except Exception:
             subject_id = 'Unknown'
             scenario_label = filename.split('.')[0]
        
        print(f"Processing: {filename} (Subject: {subject_id}, Activity: {scenario_label})...")

        try:
            mat_data = loadmat(file_path)
            
            data_channels = {}
            length = None
            
            for target_col, mat_key in KEY_MAPPING.items():
                if mat_key in mat_data:
                    data = mat_data[mat_key].flatten()
                    data_channels[target_col] = data
                    
                    if length is None:
                        length = len(data)
                    elif len(data) != length:
                        print(f"Warning: Length mismatch for {mat_key} in {filename}. Expected {length}, got {len(data)}.")
                else:
                    # print(f"Warning: Key '{mat_key}' not found in {filename}. Filling with NaNs.")
                    pass
            
            if not data_channels:
                print(f"Skipping {filename}: No valid data found.")
                continue

            df = pd.DataFrame(data_channels)
            
            # Add metadata
            df['Scenario_Label'] = scenario_label
            
            # Labeling: 1 if scenario starts with 'fall', 0 otherwise
            if 'fall' in scenario_label.lower() and 'non' not in scenario_label.lower():
                 if scenario_label.lower().startswith('fall'):
                     df['Fall'] = 1
                 else:
                     df['Fall'] = 0
            else:
                 if scenario_label.lower().startswith('fall'):
                     df['Fall'] = 1
                 else:
                     df['Fall'] = 0

            # Add Age and Sex from the github metadata
            if subject_id in SUBJECT_METADATA:
                df['Age'] = SUBJECT_METADATA[subject_id]['Age']
                df['Sex'] = 1 if SUBJECT_METADATA[subject_id]['Sex'] == 'Male' else 0 # Encode Sex: 1 for Male, 0 for Female
            else:
                df['Age'] = np.nan
                df['Sex'] = np.nan
            
            combined_data.append(df)

        except Exception as e:
            print(f"An error occurred while processing {filename}: {e}")
            continue

    if combined_data:
        final_df = pd.concat(combined_data, ignore_index=True)
        final_df.to_csv(OUTPUT_CSV_FILE, index=False)
        
        print("\n" + "="*50)
        print(f"✅ Success! Data successfully transformed into CSV.")
        print(f"File saved as: {OUTPUT_CSV_FILE}")
        print(f"Total rows exported: {len(final_df)}")
        print("="*50)
    else:
        print("\nConversion failed: No data was successfully processed.")

if __name__ == "__main__":
    convert_mat_to_csv()