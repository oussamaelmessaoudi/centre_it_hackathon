import os
import shutil

TEMP_DIR = 'temp_repo_download'
TARGET_DIR = 'HR_IMU_data'

def move_files():
    if not os.path.exists(TEMP_DIR):
        print(f"Error: {TEMP_DIR} does not exist. Cannot move files.")
        return

    if os.path.exists(TARGET_DIR):
        print(f"Cleaning {TARGET_DIR}...")
        shutil.rmtree(TARGET_DIR)
    os.makedirs(TARGET_DIR, exist_ok=True)

    files_moved = 0
    for root, _, files in os.walk(TEMP_DIR):
        for file in files:
            if file.endswith('.mat'):
                # root is like: .../temp_repo_download/subject_01/fall
                rel_path = os.path.relpath(root, TEMP_DIR)
                parts = rel_path.split(os.sep)
                
                subject_id = "Unknown"
                for part in parts:
                    if part.lower().startswith('subject'):
                        subject_id = part
                        break
                
                if subject_id == "Unknown":
                    print(f"Skipping {file} in {root}: Could not find subject ID.")
                    continue

                new_filename = f"{subject_id}_{file}"
                src_path = os.path.join(root, file)
                dest_path = os.path.join(TARGET_DIR, new_filename)
                
                shutil.copy2(src_path, dest_path) # Use copy2 to preserve metadata and avoid permission issues with move
                files_moved += 1
                
    print(f"Successfully moved {files_moved} files to {TARGET_DIR}.")

if __name__ == "__main__":
    move_files()
