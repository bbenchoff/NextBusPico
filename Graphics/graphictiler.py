import os
import random
import sys
from PIL import Image

def tile_images(folder, output_path="tiled_image.jpg", grid_size=(9, 8), tile_size=(100, 100)):
    """Tiles images randomly from a folder into a 9x8 grid, ensuring all images are used at least once."""
    if not os.path.isdir(folder):
        print(f"Error: '{folder}' is not a valid directory.")
        return

    images = [os.path.join(folder, f) for f in os.listdir(folder) if f.lower().endswith(('.png', '.jpg', '.jpeg'))]

    if not images:
        print("Error: No images found in the folder.")
        return

    total_slots = grid_size[0] * grid_size[1]

    # Shuffle the image list to randomize placement
    random.shuffle(images)

    # If there are fewer images than needed, repeat them while ensuring each is used at least once
    selected_images = []
    while len(selected_images) < total_slots:
        selected_images.extend(images)  # Add all images again if needed
    selected_images = selected_images[:total_slots]  # Trim excess

    # Create a blank white canvas
    canvas_width = grid_size[0] * tile_size[0]
    canvas_height = grid_size[1] * tile_size[1]
    canvas = Image.new('RGB', (canvas_width, canvas_height), "white")

    # Arrange images
    for row in range(grid_size[1]):
        for col in range(grid_size[0]):
            if selected_images:
                img_path = selected_images.pop()
                img = Image.open(img_path).resize(tile_size, Image.Resampling.LANCZOS)
                canvas.paste(img, (col * tile_size[0], row * tile_size[1]))

    canvas.save(output_path)
    print(f"Tiled image saved as {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python graphitiler.py <image_folder>")
    else:
        tile_images(sys.argv[1])
