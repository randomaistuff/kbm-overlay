import os
import sys
from PIL import Image

def resize_images(folder_path, target_size=(708, 379)):
    if not os.path.exists(folder_path):
        print(f"Error: Folder '{folder_path}' not found.")
        return

    print(f"Scaling all PNGs in '{folder_path}' to {target_size}...")
    
    count = 0
    for filename in os.listdir(folder_path):
        if filename.lower().endswith(".png"):
            file_path = os.path.join(folder_path, filename)
            try:
                with Image.open(file_path) as img:
                    # Convert to RGBA if not already
                    if img.mode != 'RGBA':
                        img = img.convert('RGBA')
                    
                    # Resize with high-quality Lanczos filter
                    resized_img = img.resize(target_size, Image.Resampling.LANCZOS)
                    resized_img.save(file_path)
                    count += 1
                    if count % 10 == 0:
                        print(f"  Processed {count} frames...")
            except Exception as e:
                print(f"  Failed to process {filename}: {e}")

    print(f"Done! {count} frames resized successfully.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python resize_frames.py [path_to_your_animation_folder]")
    else:
        resize_images(sys.argv[1])
