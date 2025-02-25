import os
import sys
from PIL import Image, ImageOps
import numpy as np
from pathlib import Path

def process_image(image_path, canvas_size=(140, 140), invert=True):
    """Process image according to specifications"""
    try:
        # Open the image
        img = Image.open(image_path).convert('RGB')
        
        # Calculate scaling factor to fit within the canvas
        width, height = img.size
        scale_factor = min(canvas_size[0] / width, canvas_size[1] / height)
        
        # Calculate new dimensions
        new_width = int(width * scale_factor)
        new_height = int(height * scale_factor)
        
        # Resize the image while preserving aspect ratio
        resized_img = img.resize((new_width, new_height), Image.LANCZOS)
        
        # Create a new white canvas
        canvas = Image.new('RGB', canvas_size, color='white')
        
        # Calculate position to paste (center the image)
        paste_position = ((canvas_size[0] - new_width) // 2, 
                          (canvas_size[1] - new_height) // 2)
        
        # Paste the resized image onto the canvas
        canvas.paste(resized_img, paste_position, mask=None)
        
        # Convert to monochrome (1-bit) without dithering
        bw_image = canvas.convert('1', dither=Image.NONE)
        
        # Invert if requested
        if invert:
            bw_image = ImageOps.invert(bw_image)
        
        return bw_image
    
    except Exception as e:
        print(f"Error processing {image_path}: {e}")
        return None

def image_to_horizontal_bytes(img):
    """Convert 1-bit image to horizontal byte array"""
    width, height = img.size
    
    # Calculate width in bytes (ceil division by 8)
    width_bytes = (width + 7) // 8
    
    # Create a byte array to hold the result
    byte_array = []
    
    # Process each row
    for y in range(height):
        for byte_x in range(width_bytes):
            # Process 8 pixels (or fewer if at the edge)
            byte_val = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width:
                    # Get pixel value (0 or 1) and set the corresponding bit
                    pixel = 1 if img.getpixel((x, y)) == 0 else 0
                    byte_val |= (pixel << (7 - bit))
            
            byte_array.append(byte_val)
    
    return byte_array

def generate_arduino_code(byte_array, width, height, name):
    """Generate Arduino code from byte array"""
    width_bytes = (width + 7) // 8
    
    # Format the byte array as Arduino code
    code = f"// '{name}', {width}x{height}px\n"
    code += f"const unsigned char transit_logo_{name} [] PROGMEM = {{\n"
    
    # Add the image data without width/height bytes at the beginning
    for i in range(0, len(byte_array), 16):
        line = "\t" + ", ".join(f"0x{b:02X}" for b in byte_array[i:i+16])
        if i + 16 < len(byte_array):
            line += ","
        code += line + "\n"
    
    code += "};\n"
    return code

def process_folder(folder_path, output_folder=None):
    """Process all images in a folder"""
    if output_folder is None:
        output_folder = os.path.join(folder_path, "arduino_output")
    
    # Create output folder if it doesn't exist
    os.makedirs(output_folder, exist_ok=True)
    
    # Combined output for all images
    all_code = ""
    
    # Find all image files in the folder
    image_extensions = ['.png', '.jpg', '.jpeg', '.bmp', '.gif']
    image_files = []
    for ext in image_extensions:
        image_files.extend(list(Path(folder_path).glob(f"*{ext}")))
    
    if not image_files:
        print(f"No image files found in {folder_path}")
        return
    
    print(f"Found {len(image_files)} image files")
    
    # Store all bitmap names for array creation at the end
    all_bitmap_names = []
    
    # Process each image
    for img_path in image_files:
        img_name = img_path.stem.replace(" ", "_").replace("-", "_")
        all_bitmap_names.append(img_name)
        processed_img = process_image(img_path, canvas_size=(130, 130), invert=False)
        
        if processed_img:
            width, height = processed_img.size
            byte_array = image_to_horizontal_bytes(processed_img)
            
            # Generate Arduino code
            code = generate_arduino_code(byte_array, width, height, img_name)
            
            # Save individual code file
            with open(os.path.join(output_folder, f"{img_name}.h"), "w") as f:
                f.write(code)
            
            # Add to combined code
            all_code += code + "\n"
            
            print(f"Processed: {img_path.name}")
            
            # Optionally save the processed image for verification
            processed_img.save(os.path.join(output_folder, f"{img_name}_processed.png"))
    
    # Add array of all bitmaps
    total_bytes = sum(len(image_to_horizontal_bytes(process_image(img, canvas_size=(130, 130), invert=False))) 
                       for img in image_files if process_image(img, canvas_size=(130, 130), invert=False))
    
    array_code = f"\n// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = {total_bytes})\n"
    array_code += f"const int transit_logo_allArray_LEN = {len(all_bitmap_names)};\n"
    array_code += "const unsigned char* transit_logo_allArray[" + str(len(all_bitmap_names)) + "] = {\n"
    
    for name in all_bitmap_names:
        array_code += f"\ttransit_logo_{name},\n"
    
    array_code += "};"
    
    all_code += array_code
    
    # Save combined code
    with open(os.path.join(output_folder, "all_images.h"), "w") as f:
        f.write(all_code)
    
    print(f"Done! Output saved to {output_folder}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        folder_path = sys.argv[1]
        output_folder = sys.argv[2] if len(sys.argv) > 2 else None
        process_folder(folder_path, output_folder)
    else:
        print("Usage: python image_converter.py <folder_path> [output_folder]")