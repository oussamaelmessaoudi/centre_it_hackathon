import os

def convert_tflite_to_header(tflite_path, output_header_path):
    # Check if file exists
    if not os.path.exists(tflite_path):
        print(f"Error: File {tflite_path} not found.")
        return

    with open(tflite_path, 'rb') as tflite_file:
        tflite_content = tflite_file.read()

    hex_lines = [', '.join([f'0x{byte:02x}' for byte in tflite_content[i:i+12]]) for i in range(0, len(tflite_content), 12)]
    hex_array = ',\n  '.join(hex_lines)

    # Generate header file content
    model_name = "fall_detection_model_data"
    
    header_content = f"""
#ifndef FALL_DETECTION_MODEL_H
#define FALL_DETECTION_MODEL_H

// Auto-generated from {os.path.basename(tflite_path)}
const unsigned int {model_name}_len = {len(tflite_content)};

const unsigned char {model_name}[] = {{
  {hex_array}
}};

#endif // FALL_DETECTION_MODEL_H
"""

    with open(output_header_path, 'w') as header_file:
        header_file.write(header_content)
    
    print(f"Successfully created {output_header_path}")

if __name__ == "__main__":
    # Adjusted path to point to the correct location relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    tflite_path = os.path.join(script_dir, '../fall_detection_model (1).tflite')
    output_header_path = os.path.join(script_dir, 'fall_detection_model.h')

    convert_tflite_to_header(tflite_path, output_header_path)
