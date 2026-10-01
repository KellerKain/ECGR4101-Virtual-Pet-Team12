from PIL import Image

INPUT = "KneelingRKnight_1.png"
OUTPUT = "KneelingRKnight_1.h"

WIDTH = 180
HEIGHT = 180

print(Image.open(INPUT).size)

# Load PNG and convert to RGB
img = Image.open(INPUT).convert("RGB")

# Resize
img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)

with open(OUTPUT, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define IMAGE_WIDTH  {WIDTH}\n")
    f.write(f"#define IMAGE_HEIGHT {HEIGHT}\n\n")

    f.write("const uint16_t image[] = {\n")

    for y in range(HEIGHT):
        f.write("    ")

        for x in range(WIDTH):
            r, g, b = img.getpixel((x, y))

            # RGB888 -> RGB565
            pixel = ((r & 0xF8) << 8) | \
                    ((g & 0xFC) << 3) | \
                    (b >> 3)

            f.write(f"0x{pixel:04X}")

            if not (x == WIDTH - 1 and y == HEIGHT - 1):
                f.write(", ")

        f.write("\n")

    f.write("};\n")