import subprocess
import os
import shutil
import stat

# --- Configuration ---
REPO_URL = 'https://github.com/nhoyh/HR_IMU_falldetection_dataset.git'
TARGET_DIR = 'HR_IMU_data'
TEMP_DIR = 'temp_repo_v2' # Use a new temp dir to avoid permission issues with the old one

def remove_readonly(func, path, excinfo):
    exception = excinfo[1]
    if func in (os.rmdir, os.remove):
        if getattr(exception, 'errno', None) == 13 or getattr(exception, 'winerror', None) == 5:
            try:
                os.chmod(path, stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH)
            except Exception:
                pass 
            try:
                func(path)
            except Exception:
                pass
            return
    raise

def download_and_move_data():
    if os.path.exists(TARGET_DIR):
        print(f"Cleaning {TARGET_DIR}...")
        shutil.rmtree(TARGET_DIR, onerror=remove_readonly)
    os.makedirs(TARGET_DIR, exist_ok=True)
    
    if os.path.exists(TEMP_DIR):
         print(f"Cleaning {TEMP_DIR}...")
         shutil.rmtree(TEMP_DIR, onerror=remove_readonly)

    print(f"Starting download from {REPO_URL}...")
    try:
        subprocess.run(['git', 'clone', REPO_URL, TEMP_DIR], check=True)
        print("Repository cloned successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Error cloning: {e}")
        return

    mat_files_found = 0
    for root, _, files in os.walk(TEMP_DIR):
        for file in files:
            if file.endswith('.mat'):
                # root: .../temp_repo_v2/subject_01/fall
                rel_path = os.path.relpath(root, TEMP_DIR)
                parts = rel_path.split(os.sep)
                
                subject_id = "Unknown"
                for part in parts:
                    if part.lower().startswith('subject'):
                        subject_id = part
                        break
                
                if subject_id == "Unknown":
                    continue

                new_filename = f"{subject_id}_{file}"
                src_path = os.path.join(root, file)
                dest_path = os.path.join(TARGET_DIR, new_filename)
                
                try:
                    shutil.move(src_path, dest_path)
                    mat_files_found += 1
                except Exception as e:
                    print(f"Error moving {file}: {e}")
                
    # Cleanup
    if os.path.exists(TEMP_DIR):
        shutil.rmtree(TEMP_DIR, onerror=remove_readonly)
        
    print(f"\n✅ Download complete! {mat_files_found} files moved to '{TARGET_DIR}'.")

if __name__ == "__main__":
    download_and_move_data()